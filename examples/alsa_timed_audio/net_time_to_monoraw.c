//! \file 	net_time_to_monoraw.c
//! \author	Christopher Hall
//! \copyright	(C) 2016 Intel Corporation, All Rights Reserved
//!		\<\<INTEL CONFIDENTIAL\>\>

//! \page net_to_sys_time Network time to monotonic raw
//!
//! This application connects to the timesync daemon and converts network time
//! (802.1AS) to system time in terms of monotonic raw. The command line is:
//! <br><br><tt>
//! net_time_to_monoraw -netwkt=\<network time\> -if=\<shared memory segment\>
//! </tt><br><br>
//!
//! Prints the system time to standard out

#include <args.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ipcdef.hpp>

#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

int main( int argc, char **argv )
{
	uint64_t net_time;
	char *input_file;
	int stop_idx;
	isaudk_parse_error_t parse_error;

	uint64_t master_update_time, system_time_adj;
	int64_t system_delta, master_delta;

	struct __attribute__ ((__packed__)) {
		pthread_mutex_t lock;
		gPtpTimeData data;
	} *shm_data;

	int shm_fd;

        struct isaudk_arg args[] =
        {
                ISAUDK_DECLARE_REQUIRED_ARG(NETTIME,&net_time),
                ISAUDK_DECLARE_REQUIRED_ARG(INPUT_FILE,&input_file),
        };

        stop_idx = isaudk_parse_args( args, sizeof(args)/sizeof(args[0]),
                                      argc-1, argv+1, &parse_error );
        if( parse_error != ISAUDK_PARSE_SUCCESS )
        {
                printf( "Error parsing arguments\n" );
                return -1;
        }

	shm_fd = shm_open( input_file, O_RDWR, 0 );
	if( shm_fd == -1 )
	{
		printf( "Failed to open shared memory segment: %s(%s)\n",
			input_file, strerror( errno ));
	}

	shm_data = mmap( NULL, sizeof( *shm_data ), PROT_READ | PROT_WRITE,
			 MAP_SHARED, shm_fd, 0 );

	pthread_mutex_lock( &shm_data->lock );
	master_update_time = shm_data->data.local_time -
		shm_data->data.ml_phoffset;
	master_delta = net_time - master_update_time;
	system_delta = master_delta /
		(shm_data->data.ls_freqoffset * shm_data->data.ml_freqoffset);
	system_time_adj = system_delta +
		shm_data->data.local_time + shm_data->data.ls_phoffset;
	pthread_mutex_unlock( &shm_data->lock );

	printf( "%lu\n", system_time_adj );

	return 0;
}
