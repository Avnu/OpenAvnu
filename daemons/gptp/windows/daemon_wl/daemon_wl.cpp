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
#include "windows_hal_common.hpp"
#include "windows_hal_wireless.hpp"
#include <min_port.hpp>
#include <md_wireless.hpp>
#include <tchar.h>

#define MACSTR_LENGTH 17

static bool exit_flag;

void print_usage( char *arg0 ) {
	fprintf( stderr,
		"%s "
		"[-R <priority 1>] <network interface> <peer address>\n",
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
	bool force_slave = false;
	int32_t offset = 0;
	bool syntonize = false;
	uint8_t priority1 = 248;
	int i;

	// Register default network interface
	WindowsPCAPNetworkInterfaceFactory *default_factory = new WindowsPCAPNetworkInterfaceFactory();
	OSNetworkInterfaceFactory::registerFactory( factory_name_t( "default" ), default_factory );

	// Create thread, lock, timer, timerq factories
	WindowsThreadFactory *thread_factory = new WindowsThreadFactory();
	WindowsTimerQueueFactory *timerq_factory = new WindowsTimerQueueFactory();
	WindowsLockFactory *lock_factory = new WindowsLockFactory();
	WindowsTimerFactory *timer_factory = new WindowsTimerFactory();
	WindowsConditionFactory *condition_factory = new WindowsConditionFactory();
	WindowsNamedPipeIPC *ipc = new WindowsNamedPipeIPC();
	if( !ipc->init() ) {
		delete ipc;
		ipc = NULL;
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
					if( tmp > 255 ) {
						printf( "Invalid priority 1 value, using "
							"default value\n" );
					} else {
						priority1 = (uint8_t) tmp;
					}
				}
			}
			else if (ispunct(argv[i][0])) {
				++i;
				break;
			}
		}
	}

	InterfaceLabel *local_addr[2];
	
	if(i >= argc) {
		print_usage( argv[0] );
		return -1;
	}
	uint8_t local_addr_ostr[ETHER_ADDR_OCTETS];
	parseMacAddr(argv[i++], local_addr_ostr);
	local_addr[0] = new LinkLayerAddress(local_addr_ostr);

	if (i >= argc) {
		print_usage(argv[0]);
		return -1;
	}
	parseMacAddr(argv[i++], local_addr_ostr);
	local_addr[1] = new LinkLayerAddress(local_addr_ostr);

	if (i >= argc) {
		print_usage(argv[0]);
		return -1;
	}
	uint8_t peer_addr_ostr1[ETHER_ADDR_OCTETS];
	parseMacAddr(argv[i++], peer_addr_ostr1);
	LinkLayerAddress peer_addr1(peer_addr_ostr1);

	// Create HWTimestamper object
	WindowsWirelessTimestamper *timestamper = new WindowsWirelessTimestamper(condition_factory);
	// Create Clock object
	IEEE1588Clock *clock =
		new IEEE1588Clock
		(false, false, priority1, timestamper, timerq_factory,
		lock_factory, ipc);
	timestamper->setClock(clock);
	if (!timestamper->HWTimestamper_init(local_addr, NULL, lock_factory, thread_factory, timer_factory)) {
		printf("Failed to initialize timestamper exiting...\n");
		return -1;
	}
	ipc->setSystemClockFrequency(timestamper->getSystemFrequency());

	timestamper->addPeer(peer_addr1);

	while (i < argc) {
		uint8_t peer_addr_ostr2[ETHER_ADDR_OCTETS];
		parseMacAddr(argv[i++], peer_addr_ostr2);
		LinkLayerAddress peer_addr2(peer_addr_ostr2);
		timestamper->addPeer(peer_addr2);
	}

	// Wait for Ctrl-C
	if( !SetConsoleCtrlHandler( ctrl_handler, true )) {
		printf( "Unable to register Ctrl-C handler\n" );
		return -1;
	}

	while( !exit_flag ) Sleep( 1200 );

	delete( ipc );

	return 0;
}

