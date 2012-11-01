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


#include "ieee1588.hpp"
#include "avbts_clock.hpp"
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "windows_hal.hpp"
#include <tchar.h>

#define MACSTR_LENGTH 17

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

    // Create Low level network interface object
    uint8_t local_addr_ostr[ETHER_ADDR_OCTETS];
    if( argc < 2 ) return -1;
    parseMacAddr( argv[1], local_addr_ostr );
    LinkLayerAddress local_addr(local_addr_ostr);
    
    // Create Clock object
    IEEE1588Clock *clock = new IEEE1588Clock( false, timerq_factory, ipc );  // Do not force slave
    // Create HWTimestamper object
    HWTimestamper *timestamper = new WindowsTimestamper();
    // Create Port Object linked to clock and low level
    IEEE1588Port *port = new IEEE1588Port( clock, 1, false, timestamper, false, 0, &local_addr,
                                            condition_factory, thread_factory, timer_factory, lock_factory );
    if( !port->init_port() ) {
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

