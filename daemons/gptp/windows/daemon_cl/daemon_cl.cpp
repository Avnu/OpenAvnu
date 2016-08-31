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

#include <Windows.h>
#include <winnt.h>
#include "ieee1588.hpp"
#include "avbts_clock.hpp"
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "windows_hal.hpp"
#include <tchar.h>

#define MACSTR_LENGTH 17

static bool exit_flag;

void print_usage( char *arg0 ) {
	fprintf( stderr,
		"%s "
		"[-R <priority 1>] <network interface>\n"
		"where <network interface> is a MAC address entered as xx-xx-xx-xx-xx-xx\n",
		arg0 );
}

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

int parseMacAddr( _TCHAR *macstr, uint8_t *octet_string ) {
	int i;
	_TCHAR *cur = macstr;

	if( strnlen_s( macstr, MACSTR_LENGTH ) != 17 ) return -1;

	for( i = 0; i < ETHER_ADDR_OCTETS; ++i ) {
		octet_string[i] = strtol( cur, &cur, 16 ) & 0xFF;
		++cur;
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	IEEE1588PortInit_t portInit;

	portInit.clock = NULL;
	portInit.index = 1;
	portInit.forceSlave = false;
	portInit.accelerated_sync_count = 0;
	portInit.timestamper = NULL;
	portInit.offset = 0;
	portInit.net_label = NULL;
	portInit.automotive_profile = false;
	portInit.isGM = false;
	portInit.testMode = false;
	portInit.initialLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.initialLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.condition_factory = NULL;
	portInit.thread_factory = NULL;
	portInit.timer_factory = NULL;
	portInit.lock_factory = NULL;

	bool syntonize = false;
	uint8_t priority1 = 248;
	int i;
	int phy_delays[4] =	{ -1, -1, -1, -1 };

	// Register default network interface
	WindowsPCAPNetworkInterfaceFactory *default_factory = new WindowsPCAPNetworkInterfaceFactory();
	OSNetworkInterfaceFactory::registerFactory( factory_name_t( "default" ), default_factory );

	// Create thread, lock, timer, timerq factories
	portInit.thread_factory = new WindowsThreadFactory();
	portInit.lock_factory = new WindowsLockFactory();
	portInit.timer_factory = new WindowsTimerFactory();
	portInit.condition_factory = new WindowsConditionFactory();
	WindowsNamedPipeIPC *ipc = new WindowsNamedPipeIPC();
	WindowsTimerQueueFactory *timerq_factory = new WindowsTimerQueueFactory();

	if( !ipc->init() ) {
		delete ipc;
		ipc = NULL;
	}

	// If there are no arguments, output usage
	if (1 == argc) {
		print_usage(argv[0]);
		return -1;
	}


	/* Process optional arguments */
	for( i = 1; i < argc; ++i ) {
		if( ispunct(argv[i][0]) ) {
			if( toupper( argv[i][1] ) == 'H' ) {
				print_usage( argv[0] );
				return -1;
			}
			else if( toupper( argv[i][1] ) == 'R' ) {
				if( i+1 >= argc ) {
					printf( "Priority 1 value must be specified on "
						"command line, using default value\n" );
				} else {
					unsigned long tmp = strtoul( argv[i+1], NULL, 0 ); ++i;
					if( tmp > 254 ) {
						printf( "Invalid priority 1 value, using "
							"default value\n" );
					} else {
						priority1 = (uint8_t) tmp;
					}
				}
			}
		}
	}

	// the last argument is supposed to be a MAC address, so decrement argv index to read it
	i--;

	// Create Low level network interface object
	uint8_t local_addr_ostr[ETHER_ADDR_OCTETS];
	parseMacAddr( argv[i], local_addr_ostr );
	LinkLayerAddress local_addr(local_addr_ostr);
	portInit.net_label = &local_addr;
	// Create HWTimestamper object
	portInit.timestamper = new WindowsTimestamper();
	// Create Clock object
	portInit.clock = new IEEE1588Clock( false, false, priority1, portInit.timestamper, timerq_factory, ipc, portInit.lock_factory );  // Do not force slave
	// Create Port Object linked to clock and low level
	IEEE1588Port *port = new IEEE1588Port( &portInit );
	if (!port->init_port(phy_delays)) {
		printf( "Failed to initialize port\n" );
		return -1;
	}
	port->processEvent( POWERUP );

	// Wait for Ctrl-C
	if( !SetConsoleCtrlHandler( ctrl_handler, true )) {
		printf( "Unable to register Ctrl-C handler\n" );
		return -1;
	}

	while( !exit_flag ) Sleep( 1200 );

	delete( ipc );

	return 0;
}

