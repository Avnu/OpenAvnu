/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation
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

#include "stdafx.h"
#include "windows_ipc.hpp"
#include "tsc.hpp"

static bool exit_flag;

BOOL WINAPI ctrl_handler( DWORD ctrl_type ) {
    bool ret;
    if( ctrl_type == CTRL_C_EVENT ) {
        exit_flag = true;
        ret = true;
    } else {
        ret = false;
    }
    return ret;
}

uint64_t scaleTSCClockToNanoseconds( uint64_t tsc_value, uint64_t tsc_frequency ) {
	long double scaled_output = ((long double)tsc_frequency)/1000000000;
	scaled_output = ((long double) tsc_value)/scaled_output;
	return (uint64_t) scaled_output;
}

int _tmain(int argc, _TCHAR* argv[])
{
    char pipename[64];
    strcpy_s( pipename, 64, PIPE_PREFIX );
    strcat_s( pipename, 64-strlen(pipename), P802_1AS_PIPENAME );
    HANDLE pipe;
	uint64_t tsc_frequency = getTSCFrequency( true );

   // Wait for Ctrl-C
    if( !SetConsoleCtrlHandler( ctrl_handler, true )) {
        printf( "Unable to register Ctrl-C handler\n" );
        return -1;
    }

	pipe = CreateFile( pipename, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if( pipe == INVALID_HANDLE_VALUE ) {
        printf( "Failed to open gptp handle, %d\n", GetLastError() );
    }

	printf( "TSC Frequency: %llu\n", tsc_frequency );
    while( !exit_flag ) {
		uint64_t now_tscns, now_8021as;
		uint64_t update_tscns, update_8021as;
		unsigned delta_tscns, delta_8021as;
		long double ml_ratio, ls_ratio;
	    WinNPipeQueryMessage qmsg;
		WinNPipeOffsetUpdateMessage omsg;
		bool ret;
		long offset;

		qmsg.init();
		ret = qmsg.write( pipe );
		if( ret != true ) {
			printf( "Failed to send query message\n" );
			break;
		}

		omsg.init();
		offset = omsg.read( pipe, 0 );

		printf( "Master-Local Offset = %lld\n", omsg.getMasterLocalOffset() );
        printf( "Master-Local Frequency Offset = %Lf\n", omsg.getMasterLocalFreqOffset() );
        printf( "Local-System Offset = %lld\n", omsg.getLocalSystemOffset() );
        printf( "Local-System Frequency Offset = %Lf\n", omsg.getLocalSystemFreqOffset() );
        printf( "Local Time = %llu\n\n", omsg.getLocalTime() );

		now_tscns = scaleTSCClockToNanoseconds( PLAT_rdtsc(), tsc_frequency );
		update_tscns = omsg.getLocalTime() + omsg.getLocalSystemOffset();
		delta_tscns = (unsigned)(now_tscns - update_tscns);
		printf( "Time now in terms of TSC scaled to nanoseconds time: %llu\n", now_tscns );
		printf( "TSC delta scaled to ns: %u\n", delta_tscns );
		ml_ratio = omsg.getMasterLocalFreqOffset();
		ls_ratio = omsg.getLocalSystemFreqOffset();
		delta_8021as = (unsigned)(ml_ratio*ls_ratio*delta_tscns);
		printf( "8021as delta scaled: %u\n", delta_8021as );
		update_8021as = omsg.getLocalTime() - omsg.getMasterLocalOffset();
		now_8021as = update_8021as + delta_8021as;
		printf( "Last update time in terms of 802.1AS time: %llu\n", update_8021as );
		printf( "Time now in terms of 802.1AS time: %llu\n", now_8021as );

		exit_flag = true;
	}

    printf( "Closing pipe\n" );
    CloseHandle( pipe );

    return 0;
}

