/*************************************************************************************************************
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

#define SHAPER_LOG_COMPONENT "Main"
#include "shaper_log.h"

#define STREAMDA_LENGTH 18
#define SHAPER_PORT 15365 /* Unassigned at https://www.iana.org/assignments/port-numbers */
#define MAX_CLIENT_CONNECTIONS 10
#define USER_COMMAND_PROMPT "\nEnter the command:  "

typedef struct cmd_ip
{
	int reserve_bw;
	int unreserve_bw;
	char interface[IFNAMSIZ];
	int measurement_interval;//usec
	int max_frame_size;
	int max_frame_interval;
	char stream_da[STREAMDA_LENGTH];
	int delete_qdisc;
	int quit;
}cmd_ip;

typedef struct stream_da
{
	char dest_addr[STREAMDA_LENGTH];
	int bandwidth;
	char class_id[5];
	int filter_handle;
	struct stream_da *next;
}stream_da;

stream_da *head = NULL;
int sr_classa=0, sr_classb=0;
int classa_48=0, classa_44=0, classb_48=0;
int classa_bw_48=0, classa_bw_44=0, classb_bw_48=0;
int daemonize=0,c=0;
int filterhandle_classa=1,filterhandle_classb=20;
char classid_a_48[]="2:10";
char classid_a_44[]="2:20";
char classid_b_48[]="3:30";
char interface[IFNAMSIZ] = {0};
int bandwidth = 0;
int classa_parent = 2, classb_parent=3;

void log_client_error_message(int sockfd, const char *fmt, ...)
{
	static char error_msg[200], combined_error_msg[250];
	va_list args;

	/* Get the error message. */
	va_start(args, fmt);
	vsprintf(error_msg, fmt, args);
	va_end(args);

	if (sockfd < 0)
	{
		/* Received from stdin.  Just log this error. */
		SHAPER_LOG_ERROR(error_msg);
	}
	else
	{
		/* Log this as a remote client error. */
		sprintf(combined_error_msg, "Client %d: %s", sockfd, error_msg);
		SHAPER_LOG_ERROR(combined_error_msg);

		/* Send the error message to the client. */
		sprintf(combined_error_msg, "ERROR: %s\n", error_msg);
		send(sockfd, combined_error_msg, strlen(combined_error_msg), 0);
	}
}

int is_empty()
{
	return head == NULL;
}

void insert_stream_da(int sockfd, char dest_addr[], int bandwidth, char class_id[], int filter_handle)
{
	stream_da *node = (stream_da *)malloc(sizeof(stream_da));
	if (node == NULL)
	{
		log_client_error_message(sockfd, "Unable to allocate memory. Exiting program");
		shaperLogExit();
		exit(1);
	}
	strcpy(node->dest_addr,dest_addr);
	node->bandwidth = bandwidth;
	strcpy(node->class_id,class_id);
	node->filter_handle=filter_handle;
	node->next = NULL;
	if (is_empty())
	{
		head = node;
	}
	else
	{
		stream_da *current = head;
		while (current->next != NULL)
		{
			current = current->next;
		}
		current->next = node;
	}
}

int check_stream_da(int sockfd, char dest_addr[])
{
	stream_da *current = head;
	while (current != NULL)
	{
		if (!strcmp(current->dest_addr, dest_addr))
		{
			log_client_error_message(sockfd, "Stream DA already present");
			return 1;
		}
		current = current->next;
	}
	return 0;
}

stream_da* get_stream_da(int sockfd, char dest_addr[])
{
	stream_da *current = head;
	while (current != NULL)
	{
		if (!strcmp(current->dest_addr, dest_addr))
		{

			return current;
		}
		current = current->next;
	}
	log_client_error_message(sockfd, "Unknown Stream DA");
	return NULL;
}

void remove_stream_da(int sockfd, char dest_addr[])
{
	stream_da *current = head;
	(void) sockfd;

	if (current != NULL && !strcmp(current->dest_addr, dest_addr))
	{
		head = current->next;
		free(current);
		return;
	}

	while (current->next != NULL)
	{
		if (!strcmp(current->next->dest_addr, dest_addr))
		{
			stream_da *temp = current->next;
			current->next = current->next->next;
			free(temp);
			return;
		}
		current = current->next;
	}
}

void delete_streamda_list()
{
	stream_da *current = head;
	stream_da *next = NULL;
	while (current != NULL)
	{
		next = current->next;
		free(current);
		current = next;
	}
	head = NULL;
}

void usage (int sockfd)
{
	const char *usage = "Usage:\n"
			"	-r	Reserve Bandwidth\n"
			"	-u	Unreserve Bandwidth\n"
			"	-i	Interface\n"
			"	-s	Measurement interval (in microseconds)\n"
			"	-b	Maximum frame size (in bytes)\n"
			"	-f	Maximum frame interval\n"
			"	-a	Stream Destination Address\n"
			"	-d	Delete qdisc\n"
			"	-q	Quit Application\n"
			"Reserving Bandwidth Example:\n"
			"	-ri eth2 -s 125 -b 74 -f 1 -a ff:ff:ff:ff:ff:11\n"
			"Unreserving Bandwidth Example:\n"
			"	-ua ff:ff:ff:ff:ff:11\n"
			"Quit Example:\n"
			"	-q\n"
			"Delete qdisc Example:\n"
			"	-d\n";
	if (sockfd < 0)
	{
		printf("%s",usage);
	}
	else
	{
		send(sockfd, usage, strlen(usage),0);
	}

}

cmd_ip parse_cmd(char command[])
{
	cmd_ip inputs={0};
	int i=0;
	char temp[100];
	while(command[i++] != '\0')
	{

		if (command[i] == 'r')
		{
			inputs.reserve_bw=1;
			i++;
		}

		if (command[i] == 'u')
		{
			inputs.unreserve_bw=1;
			i++;
		}

		if (command[i] == 'i')
		{
			int k=0;
			i = i+2;
			while (command[i] != ' ' && k < IFNAMSIZ-1)
			{
				inputs.interface[k] = command[i];
				k++;i++;
			}
			inputs.interface[k] = '\0';
		}

		if (command[i] == 's')
		{
			int k=0;
			i = i+2;
			while (command[i]  != ' ' && k < (int) sizeof(temp)-1)
			{
				temp[k] = command[i];
				k++;i++;
			}
			temp[k] = '\0';
			inputs.measurement_interval = atoi(temp);
		}

		if (command[i] == 'b')
		{
			int k=0;
			i = i+2;
			while (command[i]  != ' ' && k < (int) sizeof(temp)-1)
			{
				temp[k] = command[i];
				k++;i++;
			}
			temp[k] = '\0';
			inputs.max_frame_size = atoi(temp);
		}

		if (command[i] == 'f')
		{
			int k=0;
			i = i+2;
			while (command[i]  != ' ' && k < (int) sizeof(temp)-1)
			{
				temp[k] = command[i];
				k++;i++;
			}
			temp[k] = '\0';
			inputs.max_frame_interval = atoi(temp);
		}

		if (command[i] == 'a')
		{
			int k=0;
			i = i+2;
			while (command[i]  != ' ' && k < STREAMDA_LENGTH-1)
			{
				inputs.stream_da[k] = command[i];
				k++;i++;
			}
			inputs.stream_da[k] = '\0';
		}

		if (command[i] == 'd')
		{
			inputs.delete_qdisc=1;
			i++;
		}

		if (command[i] == 'q')
		{
			inputs.quit = 1;
			i++;
		}
	}
	return inputs;
}

void tc_class_command(int sockfd, char command[],char class_id[],char interface[], int bandwidth, int cburst)
{
	char tc_command[1000]={0};
	sprintf(tc_command, "tc class %s dev %s classid %s htb rate %dkbit cburst %d",command, interface, class_id, bandwidth, cburst);
	if (system(tc_command) < 0)
	{
		log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
	}
}

void add_filter(int sockfd, char interface[], int parent, int filter_handle, char class_id[], char dest_addr[])
{
	char tc_command[1000]={0};
	sprintf(tc_command, "tc filter add dev %s prio 1 handle 800::%d parent %d: u32 classid %s  match ether dst %s", interface,filter_handle,parent,class_id,dest_addr );
	if (system(tc_command) < 0)
	{
		log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
	}
}

// Returns 1 if successful, -1 on an error, or 0 if exit requested.
int process_command(int sockfd, char command[])
{
	cmd_ip input = parse_cmd(command);
	char tc_command[1000]={0};
	int maxburst = 0;

	if (input.reserve_bw && input.unreserve_bw)
	{
		log_client_error_message(sockfd, "Cannot reserve and unreserve bandwidth at the same time");
		usage(sockfd);
		return -1;
	}


	if (input.reserve_bw)
	{
		if (input.max_frame_size == 0 || input.measurement_interval == 0 || input.max_frame_interval == 0)
		{
			log_client_error_message(sockfd, "max_frame_size, measurement_interval and max_frame_interval are mandatory inputs");
			usage(sockfd);
			return -1;
		}
	}
	if (input.unreserve_bw)
	{
		if (input.stream_da == 0)
		{
			log_client_error_message(sockfd, "Stream Destination Address is required to unreserve bandwidth");
			usage(sockfd);
			return -1;
		}
	}
	if ((input.quit==1 || input.delete_qdisc==1) &&(input.reserve_bw==1 || input.unreserve_bw==1))
	{
		log_client_error_message(sockfd, "Quit or Delete cannot be used with other commands");
		usage(sockfd);
		return -1;
	}

	if (input.quit == 1 || input.delete_qdisc == 1)
	{
		if (sr_classa != 0 || sr_classb != 0)
		{
			//delete all the Stream DAs in list
			delete_streamda_list();
			if (strlen(interface)!=0)
			{
				//delete qdisc
				sprintf(tc_command, "tc qdisc del dev %s root handle 1:", interface);
				if (system(tc_command) < 0)
				{
					log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
				}
			}
			sr_classa = sr_classb = classa_48 = classa_44 = classb_48 = classa_bw_48 = classa_bw_44 = classb_bw_48 = 0;
		}

		if (input.quit == 1)
		{
			return 0;
		}
	}

	if (sr_classa == 0 && sr_classb == 0)
	{
		//Initializing qdisc
		if (strlen(input.interface)==0)
		{
			if (input.unreserve_bw || input.reserve_bw)
			{
				log_client_error_message(sockfd, "Interface is mandatory for the first command");
			}
			usage(sockfd);
			return -1;
		}
		sprintf(tc_command, "tc qdisc add dev %s root handle 1: mqprio num_tc 4 map 0 1 2 3 2 0 0 1 1 1 1 1 3 3 3 3 queues 1@0 1@1 1@2 1@3 hw 0", input.interface);
		if (system(tc_command) < 0)
		{
			log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
			return -1;
		}
		strcpy(interface,input.interface);
	}

	if (input.unreserve_bw || input.reserve_bw)
	{
		bandwidth = ceil(((1/(input.measurement_interval*pow(10,-6))) * (input.max_frame_size*8) * input.max_frame_interval)/1000);
		maxburst = input.max_frame_size*input.max_frame_interval*2;
	}

	if (input.reserve_bw == 1)
	{
		if (check_stream_da(sockfd, input.stream_da))
		{
			return 1;
		}
		if (input.measurement_interval == 125 || input.measurement_interval == 136)
		{
			if (sr_classa == 0)
			{
				sr_classa=1;
				//Create qdisc for Class A traffic
				sprintf(tc_command, "tc qdisc add dev %s handle %d:  parent 1:5 htb", interface, classa_parent);
				if (system(tc_command) < 0)
				{
					log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
					return -1;
				}
			}

			if (input.measurement_interval == 125)
			{
				classa_bw_48 = classa_bw_48 + bandwidth;
				if (classa_48 == 0)
				{
					classa_48 = 1;
					tc_class_command(sockfd, "add",classid_a_48,interface,classa_bw_48, maxburst);
				}
				else
				{
					tc_class_command(sockfd, "change",classid_a_48, interface,classa_bw_48, maxburst);
				}

				add_filter(sockfd, interface, classa_parent, filterhandle_classa, classid_a_48, input.stream_da );
				insert_stream_da(sockfd, input.stream_da, bandwidth, classid_a_48, filterhandle_classa );
				filterhandle_classa++;
			}
			else
			{
				classa_bw_44 = classa_bw_44 + bandwidth;
				if (classa_44 == 0)
				{
					classa_44 = 1;
					tc_class_command(sockfd, "add",classid_a_44,interface,classa_bw_44, maxburst);
				}
				else
				{
					tc_class_command(sockfd, "change",classid_a_44, interface,classa_bw_44, maxburst);
				}

				add_filter(sockfd, interface, classa_parent, filterhandle_classa, classid_a_44, input.stream_da );
				insert_stream_da(sockfd, input.stream_da, bandwidth, classid_a_44, filterhandle_classa);
				filterhandle_classa++;
			}
		}
		else if (input.measurement_interval == 250)
		{
			if (sr_classb == 0)
			{
				sr_classb = 1;
				//Create qdisc for Class B traffic
				sprintf(tc_command, "tc qdisc add dev %s handle %d:  parent 1:6 htb", interface, classb_parent);
				if (system(tc_command) < 0)
				{
					log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
					return -1;
				}
			}

			classb_bw_48 = classb_bw_48 + bandwidth;
			if (classb_48 == 0)
			{
				classb_48 = 1;
				tc_class_command(sockfd, "add",classid_b_48,interface,classb_bw_48, maxburst);
			}
			else
			{
				tc_class_command(sockfd, "change",classid_b_48, interface,classb_bw_48, maxburst);
			}
			add_filter(sockfd, interface, classb_parent, filterhandle_classb, classid_b_48, input.stream_da );
			insert_stream_da(sockfd, input.stream_da, bandwidth, classid_b_48, filterhandle_classb);
			filterhandle_classb++;
		}
		else
		{
			log_client_error_message(sockfd, "Measurement Interval doesn't match that of Class A(125 or 136) or Class B(250) traffic. Enter a valid measurement interval");
			return -1;
		}
	}
	else if (input.unreserve_bw==1)
	{
		stream_da *remove_stream = get_stream_da(sockfd, input.stream_da);
		if (remove_stream != NULL)
		{
			int class_bw = 0;
			if (!strcmp(remove_stream->class_id,classid_a_48))
			{
				classa_bw_48 = classa_bw_48 - remove_stream->bandwidth;
				class_bw = classa_bw_48;
			}
			else if (!strcmp(remove_stream->class_id,classid_a_44))
			{
				classa_bw_44 = classa_bw_44 - remove_stream->bandwidth;
				class_bw = classa_bw_44;
			}
			else
			{
				classb_bw_48 = classb_bw_48 - remove_stream->bandwidth;
				class_bw = classb_bw_48;
			}
			if (class_bw == 0)
			{
				class_bw=1;
			}
			tc_class_command(sockfd, "change",remove_stream->class_id, interface,class_bw, maxburst);
			char parent[3]={0};
			strncpy(parent,remove_stream->class_id,2);
			sprintf(tc_command, "tc filter del dev %s parent %s handle 800::%d prio 1 protocol all u32",interface,parent,remove_stream->filter_handle);
			if (system(tc_command) < 0)
			{
				log_client_error_message(sockfd, "command(\"%s\") failed", tc_command);
				return -1;
			}
			remove_stream_da(sockfd, remove_stream->dest_addr);
		}
	}

	return 1;
}

int init_socket()
{
	int socketfd = 0;
	struct sockaddr_in serv_addr;
	int yes=1;
	if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1 )
	{
		SHAPER_LOGF_ERROR("Could not open socket %d.  Error %d (%s)", socketfd, errno, strerror(errno));
		return -1;
	}
	if (fcntl(socketfd, F_SETFL, O_NONBLOCK) < 0)
	{
		SHAPER_LOGF_ERROR("Could not set the socket to non-blocking.  Error %d (%s)", errno, strerror(errno));
		close(socketfd);
		return -1;
	}


	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	serv_addr.sin_port = htons(SHAPER_PORT);
	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	if (bind(socketfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0)
	{
		SHAPER_LOGF_ERROR("bind() error %d (%s)", errno, strerror(errno));
		return -1;
	}
	if (listen(socketfd, 10)<0)
	{
		SHAPER_LOGF_ERROR("listen() error %d (%s)", errno, strerror(errno));
		return -1;
	}

	return socketfd;

}

int main (int argc, char *argv[])
{
	char command[100];
	int socketfd = 0,newfd = 0;
	int clientfd[MAX_CLIENT_CONNECTIONS];
	int i, nextclientindex;
	fd_set read_fds;
	int fdmax;
	int recvbytes;
	int exit_received = 0;

	shaperLogInit();

	while((c = getopt(argc,argv,"d"))>=0)
	{
		switch(c)
		{
			case 'd':
			daemonize = 1;
			break;
		}
	}

	if (daemonize == 1 && daemon(1,0) < 0)
	{
		SHAPER_LOGF_ERROR("Error %d (%s) starting the daemon", errno, strerror(errno));
		shaperLogExit();
		return 1;
	}

	if ((socketfd = init_socket())<0)
	{
		shaperLogExit();
		return 1;
	}

	for (i = 0; i < MAX_CLIENT_CONNECTIONS; ++i)
	{
		clientfd[i] = -1;
	}
	nextclientindex = 0;

	fputs(USER_COMMAND_PROMPT, stdout);
	fflush(stdout);

	while (!exit_received)
	{
		FD_ZERO(&read_fds);
		if (!daemonize)
		{
			FD_SET(STDIN_FILENO, &read_fds);
		}
		FD_SET(socketfd, &read_fds);
		fdmax = socketfd;
		for (i = 0; i < MAX_CLIENT_CONNECTIONS; ++i)
		{
			if (clientfd[i]>0)
			{
				FD_SET(clientfd[i],&read_fds);
				if (clientfd[i]>fdmax)
				{
					fdmax = clientfd[i];
				}
			}
		}
		int n = select(fdmax+1,&read_fds,NULL,NULL,NULL);
		if(n == -1)
		{
			SHAPER_LOGF_ERROR("select() error %d (%s)", errno, strerror(errno));
			break;
		}
		else
		{
			/* Handle any commands received via stdin. */
			if (FD_ISSET(STDIN_FILENO, &read_fds))
			{
				recvbytes = read(STDIN_FILENO, command, sizeof(command) - 1);
				if (recvbytes <= 0)
				{
					SHAPER_LOGF_ERROR("Error %d reading from stdin (%s)", errno, strerror(errno));
				}
				else
				{
					command[recvbytes] = '\0';

					/* Process the command data. */
					int ret = process_command(-1, command);
					if (!ret)
					{
						/* Received a command to exit. */
						exit_received = 1;
					}
					else
					{
						/* Prompt the user again. */
						shaperLogDisplayAll();
						fputs(USER_COMMAND_PROMPT, stdout);
						fflush(stdout);
					}
				}
			}

			if (FD_ISSET(socketfd,&read_fds))
			{
				newfd = accept(socketfd, (struct sockaddr*)NULL, NULL);
				if (clientfd[nextclientindex] != -1)
				{
					/* Find the next available index. */
					for (i = (nextclientindex + 1) % MAX_CLIENT_CONNECTIONS; i != nextclientindex; i = (i + 1) % MAX_CLIENT_CONNECTIONS)
					{
						if (clientfd[nextclientindex] == -1)
						{
							/* Found an empty array slot. */
							break;
						}
					}
					if (i == nextclientindex)
					{
						/* No more client slots available.  Connection rejected. */
						SHAPER_LOG_WARNING("Out of client connection slots.  Connection rejected.");
						close(newfd);
						newfd = -1;
					}
				}


				if (newfd != -1)
				{
					clientfd[nextclientindex] = newfd;
					nextclientindex = (nextclientindex + 1) % MAX_CLIENT_CONNECTIONS; /* Next slot used for the next try. */

					/* Send a prompt to the user. */
					send(newfd, USER_COMMAND_PROMPT, strlen(USER_COMMAND_PROMPT), 0);
				}
			}

			for (i = 0; i < MAX_CLIENT_CONNECTIONS; ++i)
			{
				if (clientfd[i] != -1 && FD_ISSET(clientfd[i], &read_fds))
				{
					recvbytes = recv(clientfd[i], command, sizeof(command) - 1, 0);
					if (recvbytes < 0)
					{
						SHAPER_LOGF_ERROR("Error %d reading from socket %d (%s).  Connection closed.", errno, clientfd[i], strerror(errno));
						close(clientfd[i]);
						clientfd[i] = -1;
						nextclientindex = i; /* We know this slot will be empty. */
						continue;
					}
					if (recvbytes == 0)
					{
						SHAPER_LOGF_INFO("Socket %d closed", clientfd[i]);
						close(clientfd[i]);
						clientfd[i] = -1;
						nextclientindex = i; /* We know this slot will be empty. */
						continue;
					}

					command[recvbytes] = '\0';
					while (recvbytes > 0 && isspace(command[recvbytes - 1]))
					{
						/* Remove trailing whitespace. */
						command[--recvbytes] = '\0';
					}
					SHAPER_LOGF_INFO("The received command is \"%s\"",command);
					int ret = process_command(clientfd[i], command);
					if (!ret)
					{
						/* Received a command to exit. */
						exit_received = 1;
					}
					else
					{
						/* Send another prompt to the user. */
						send(clientfd[i], USER_COMMAND_PROMPT, strlen(USER_COMMAND_PROMPT), 0);
					}
				}
			}
		}
	}//main while loop

	close(socketfd);

	/* Close any connected sockets. */
	for (i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
		if (clientfd[i] != -1) {
			SHAPER_LOGF_INFO("Socket %d closed", clientfd[i]);
			close(clientfd[i]);
			clientfd[i] = -1;
		}
	}

	shaperLogExit();

	return 0;
}
