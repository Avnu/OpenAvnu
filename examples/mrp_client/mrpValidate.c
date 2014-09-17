/******************************************************************************

  Copyright (c) 2013, Harman International
  Copyright (c) 2012, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/* This utility is designed to do three things:
 *  1. Allow you to very simply exercise the MRP daemon between two
 *     end stations with or without an AVB Bridge between them.
 *  2. Monitor the MxRPDUs sent by a third party device.
 *  3. Provide an example of how to use the Open-AVB MRP daemon
 *     command interface.
 *
 * To run this program load the MRP daemon on two PCs.  Run one in
 * Monitor (mon) mode, and issue M++, V++, S++, etc commands on the
 * other PC.  Wireshark can also be used to verify that MRPDUs are
 * formatted correctly for the various MxRP applications.
 *
 * Possible future enhancements:
 *  1. Allow the various parameters to be modified (e.g.: change the
 *     STREAM_ID, VLAN_ID, etc).
 *  2. Timestamp the Monitor output, including elapsed time between
 *     commands of the same type (e.g.: MMRP to next MMRP command).
 *     That way a user could validate that the periodic timer for
 *     MMRP is working correctly, or that the MSRP LeaveAll timer
 *     is working, or ....  This would also allow the user to verify
 *     the timers of a third party device or operating correctly.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "mrpd.h"
#include "mrpdclient.h"

/* global variables */
#define VERSION_STR	"0.0"
static const char *version_str =
    "mrpValidate v" VERSION_STR "\n"
    "Copyright (c) 2013, Harman International\n";
int done = 0;
char *msgbuf;

// Various parameters used by MMRP, MVRP and MSRP
// (Note: Defined here in an effort to make it easier to
//        understand examples below when it comes time
//        to build commands in real code!)
#define STREAM_DA                "010203040506"
#define STREAM_ID                "DEADBEEFBADFCA11"
#define VLAN_ID                  "0002"
#define TSPEC_MAX_FRAME_SIZE     "576"
#define TSPEC_MAX_FRAME_INTERVAL "8000"
#define PRIORITY_AND_RANK        "96"
#define ACCUMULATED_LATENCY      "1000"
#define SR_CLASS_ID              "6"
#define SR_CLASS_PRIORITY        "3"
#define BRIDGE_ID                "BADC0FFEEC0FFEE0"
#define FAILURE_CODE             "1"

int
process_ctl_msg(char *buf, int buflen)
{
	/* Unused parameters */
	(void)buflen;

	if (buf[1] == ':') {
		printf("?? RESP:\n%s", buf);
	} else {
		printf("MRPD ---> %s", buf);
	}
	fflush(stdout);
	return(0);
}


void
displayStatusMsgs(void) {
	int status;

	status = 0;
	while (status >= 0) {
		status = mrpdclient_recv(process_ctl_msg);
	}
}


//-------------------------------------------
// MMRP functions:
//  M++ Declare a MAC Address w/New
//  M+? Declare a MAC Address w/JoinMt
//  M-- Withdraw a MAC Address w/Lv
//  M?? Display the database of MAC Addresses
//-------------------------------------------
int
fnMplusplus(void) {
#define M_PLUS_PLUS "M++:M=" STREAM_DA
	return (mprdclient_sendto(M_PLUS_PLUS, sizeof(M_PLUS_PLUS)));
}

int
fnMplusquestion(void) {
#define M_PLUS_QUESTION "M+?:M=" STREAM_DA
	return (mprdclient_sendto(M_PLUS_QUESTION, sizeof(M_PLUS_QUESTION)));
}

int
fnMminusminus(void) {
#define M_MINUS_MINUS "M--:M=" STREAM_DA
	return (mprdclient_sendto(M_MINUS_MINUS, sizeof(M_MINUS_MINUS)));
}

int
fnMquestionquestion(void) {
#define M_QUESTION_QUESTION "M??"
	mprdclient_sendto(M_QUESTION_QUESTION, sizeof(M_QUESTION_QUESTION));

	displayStatusMsgs();
	return 0;
}


//-------------------------------------------
// MVRP functions:
//  V++ Declare a VLAN Join w/New
//  V+? Declare a VLAN Join w/JoinMt
//  V-- Withdraw a VLAN Join w/Lv
//  V?? Display the database of VLAN Joins
//-------------------------------------------
int
fnVplusplus(void) {
#define V_PLUS_PLUS "V++:I=" VLAN_ID
	return (mprdclient_sendto(V_PLUS_PLUS, sizeof(V_PLUS_PLUS)));
}

int
fnVplusquestion(void) {
#define V_PLUS_QUESTION "V+?:I=" VLAN_ID
	return (mprdclient_sendto(V_PLUS_QUESTION, sizeof(V_PLUS_QUESTION)));
}

int
fnVminusminus(void) {
#define V_MINUS_MINUS "V--:I=" VLAN_ID
	return (mprdclient_sendto(V_MINUS_MINUS, sizeof(V_MINUS_MINUS)));
}

int
fnVquestionquestion(void) {
#define V_QUESTION_QUESTION "V??"
	mprdclient_sendto(V_QUESTION_QUESTION, sizeof(V_QUESTION_QUESTION));

	displayStatusMsgs();
	return 0;
}


/********************************************
 * MSRP Talker functions:
 *  S++ Declare a Talker Advertise for Stream w/New
 *  S+? Declare a Talker Advertise for Stream w/JoinMt
 *  S-- Withdraw a Talker Advertise for Stream w/Lv
 *  S?? Display the database of Talker Advertise for Stream IDs
 ********************************************/
int
fnSTplusplus(void) {
#define ST_PLUS_PLUS "S++:S=" STREAM_ID \
                     ",A=" STREAM_DA \
                     ",V=" VLAN_ID \
                     ",Z=" TSPEC_MAX_FRAME_SIZE \
                     ",I=" TSPEC_MAX_FRAME_INTERVAL \
                     ",P=" PRIORITY_AND_RANK \
                     ",L=" ACCUMULATED_LATENCY
	return (mprdclient_sendto(ST_PLUS_PLUS, sizeof(ST_PLUS_PLUS)));
}

int
fnSTplusquestion(void) {
#define ST_PLUS_QUESTION "S+?:S=" STREAM_ID \
                         ",A=" STREAM_DA \
                         ",V=" VLAN_ID \
                         ",Z=" TSPEC_MAX_FRAME_SIZE \
                         ",I=" TSPEC_MAX_FRAME_INTERVAL \
                         ",P=" PRIORITY_AND_RANK \
                         ",L=" ACCUMULATED_LATENCY
	return (mprdclient_sendto(ST_PLUS_QUESTION, sizeof(ST_PLUS_QUESTION)));
}

int
fnSTminusminus(void) {
#define ST_MINUS_MINUS "S--:S=" STREAM_ID
	return (mprdclient_sendto(ST_MINUS_MINUS, sizeof(ST_MINUS_MINUS)));
}

int
fnSquestionquestion(void) {
#define S_QUESTION_QUESTION "S??"
	mprdclient_sendto(S_QUESTION_QUESTION, sizeof(S_QUESTION_QUESTION));

	displayStatusMsgs();
	return 0;
}

/******************************************************************
 * MSRP Talker Failed functions:
 *  S++...B=...C=... Declare a Talker Failed for Stream w/New
 *  S+?...B=...C=... Declare a Talker Failed for Stream w/JoinMt
 *  these are the same format as the Talker Advertise string with the
 *  addition of a bridge ID and a failure code at the end of the
 *  string.
 ******************************************************************/
int
fnSTFplusplus(void) {
#define STF_PLUS_PLUS "S++:S=" STREAM_ID \
                     ",A=" STREAM_DA \
                     ",V=" VLAN_ID \
                     ",Z=" TSPEC_MAX_FRAME_SIZE \
                     ",I=" TSPEC_MAX_FRAME_INTERVAL \
                     ",P=" PRIORITY_AND_RANK \
                     ",L=" ACCUMULATED_LATENCY \
                     ",B=" BRIDGE_ID \
                     ",C=" FAILURE_CODE
	return (mprdclient_sendto(STF_PLUS_PLUS, sizeof(STF_PLUS_PLUS)));
}

int
fnSTFplusquestion(void) {
#define STF_PLUS_QUESTION "S+?:S=" STREAM_ID \
                         ",A=" STREAM_DA \
                         ",V=" VLAN_ID \
                         ",Z=" TSPEC_MAX_FRAME_SIZE \
                         ",I=" TSPEC_MAX_FRAME_INTERVAL \
                         ",P=" PRIORITY_AND_RANK \
                         ",L=" ACCUMULATED_LATENCY \
                         ",B=" BRIDGE_ID \
                         ",C=" FAILURE_CODE
	return (mprdclient_sendto(STF_PLUS_QUESTION, sizeof(STF_PLUS_QUESTION)));
}


//-------------------------------------------
// MSRP Listener functions:
//  S+L Declare a Listener Ready for Stream w/New
//  S-L Withdraw a Listener Ready for Stream w/Lv
//-------------------------------------------
int
fnSLplusplus(void) {
#define SL_PLUS_PLUS "S+L:L=" STREAM_ID ",D=2"
	return (mprdclient_sendto(SL_PLUS_PLUS, sizeof(SL_PLUS_PLUS)));
}

int
fnSLminusminus(void) {
#define SL_MINUS_MINUS "S-L:L=" STREAM_ID
	return (mprdclient_sendto(SL_MINUS_MINUS, sizeof(SL_MINUS_MINUS)));
}


//-------------------------------------------
// MSRP Domain functions:
//  S+D Declare a Domain w/New
//  S-D Withdraw a Domain w/Lv
//-------------------------------------------
int
fnSDplusplus(void) {
#define SD_PLUS_PLUS "S+D:C=" SR_CLASS_ID \
                     ",P=" SR_CLASS_PRIORITY \
                     ",V=" VLAN_ID
	return (mprdclient_sendto(SD_PLUS_PLUS, sizeof(SD_PLUS_PLUS)));
}

int
fnSDminusminus(void) {
#define SD_MINUS_MINUS "S-D:C=" SR_CLASS_ID \
                       ",P=" SR_CLASS_PRIORITY \
                       ",V=" VLAN_ID
	return (mprdclient_sendto(SD_MINUS_MINUS, sizeof(SD_MINUS_MINUS)));
}


int fnExit(void) {
	done = 1;
	return 0;
}

int
dump_ctl_msg(char *buf, int buflen)
{
	/* Unused parameters */
	(void)buflen;

	printf("%s", buf);
	fflush(stdout);
	return(0);
}

int fnMonitor(void) {
	/* First we need to have the mrpd daemon put this application in the
	 * list of attached clients that wish to receive reports when any MMRP,
	 * MSRP or MVRP attributes are received.
	 *
	 * Note that any M*, S* or V* command will put this application in the
	 * mrpd attached clients list.  However, the M??, S?? and V?? do not
	 * cause any MRP attribute declarations or registrations, and are
	 * therefore the preferred method for doing this for monitoring.
	 */
	mprdclient_sendto(M_QUESTION_QUESTION, sizeof(M_QUESTION_QUESTION));
	mprdclient_sendto(S_QUESTION_QUESTION, sizeof(S_QUESTION_QUESTION));
	mprdclient_sendto(V_QUESTION_QUESTION, sizeof(V_QUESTION_QUESTION));

	/* Now just wait for any MRPDUs to be received... */
	while (1) {
		mrpdclient_recv(dump_ctl_msg);
	}

  return 0;
}

//-------------------------------------------
// Menu system
//-------------------------------------------
struct menu_option {
	char	*option;
	char	*desc;
	int	(*fn)(void);
};

struct menu_option menu[] = {
	{"M++",  "Declare an MMRP MAC Address (NEW)",    fnMplusplus},
	{"M+?",  "Declare an MMRP MAC Address (JOIN)",   fnMplusquestion},
	{"M--",  "Withdraw an MMRP MAC Address (LEAVE)", fnMminusminus},
	{"M??",  "Dump the MMRP MAC Address table",      fnMquestionquestion},
	{"   ",  "", 0},
	{"V++",  "Declare a VLAN ID (NEW)",              fnVplusplus},
	{"V+?",  "Declare a VLAN ID (JOIN)",             fnVplusquestion},
	{"V--",  "Withdraw a VLAN ID (LEAVE)",           fnVminusminus},
	{"V??",  "Dump the VLAN ID table",               fnVquestionquestion},
	{"   ",  "", 0},
	{"S++",  "Declare a Talker Advertise (NEW)",     fnSTplusplus},
	{"S+?",  "Declare a Talker Advertise (JOIN)",    fnSTplusquestion},
	{"S--",  "Withdraw a Talker Advertise (LEAVE)",  fnSTminusminus},
	{"S??",  "Dump the MSRP table",                  fnSquestionquestion},
	{"   ",  "", 0},
	{"SF++", "Declare a Talker Failed (NEW)",        fnSTFplusplus},
	{"SF+?", "Declare a Talker Failed (JOIN)",       fnSTFplusquestion},
	{"   ",  "", 0},
	{"S+L",  "Declare a Listener Ready (NEW)",       fnSLplusplus},
	{"S-L",  "Withdraw a Listener Ready (LEAVE)",    fnSLminusminus},
	{"   ",  "", 0},
	{"S+D",  "Declare a Domain (NEW)",               fnSDplusplus},
	{"S-D",  "Withdraw a Domain (LEAVE)",            fnSDminusminus},
	{"   ",  "", 0},
	{"mon",  "Monitor MRP notifications",            fnMonitor},
	{"   ",  "", 0},
	{"exit", "Exit the program",                     fnExit},
	{0, 0, 0}
};

void
displayMenu(void) {
	int i;

	printf("\n\nSelect from the following options:\n");
	for (i=0; 0 != menu[i].option; i++) {
		if ('\0' == menu[i].desc[0]) {
			printf("\n");
		} else {
			printf("%5s - %s\n", menu[i].option, menu[i].desc);
		}
	}
	printf("Note: StreamID=" STREAM_ID ", StreamDA=" STREAM_DA
	       ", VLAN ID=" VLAN_ID "\n");
}

int
runMenuFunc(char *option) {
	int i;
	int rc = -1;

	for (i=0; 0 != menu[i].option; i++) {
		if (0 == strcmp(option, menu[i].option)) {
			rc = menu[i].fn();
			break;
		}
	}
	if (NULL == menu[i].fn) {
		printf("ERROR: '%s' is not a listed option\n\n", option);

	}
	return rc;
}

//-------------------------------------------

int
main(int argc, char *argv[]) {
	char	option[10];
	char	saved[10];
	int	rc = 0;

	(void) argc;
	(void) argv;
	(void) rc;

	printf("%s\n", version_str);

	msgbuf = malloc(1500);
	if (NULL == msgbuf) {
		printf("memory allocation error - exiting\n");
		return -1;
	}

	rc = mrpdclient_init(MRPD_PORT_DEFAULT);
	if (rc) {
		printf("init failed\n");
		goto out;
	}

	saved[0]='\0';
	while (!done) {
		displayMenu();
		printf("Option to run? ");
		if (NULL == fgets(option, sizeof(option), stdin)) {
			done = 1;
			printf("\n");
			continue;
		}
		option[strlen(option)-1] = '\0'; // remove assumed LF
		if ('\0' == option[0]) {
			// Repeat last cmd if nothing entered
			strncpy(option, saved, sizeof(option));
		}

		rc = runMenuFunc(option);
		if (rc >= 0) {
			// Save last command if valid
			strncpy(saved, option, sizeof(option));
		}
	}
	rc = mprdclient_close();

out:
	return rc;
}
