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

#ifndef WINDOWS_HAL_HPP
#define WINDOWS_HAL_HPP

#include <minwindef.h>
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "packet.hpp"
#include "ieee1588.hpp"
#include "iphlpapi.h"
#include "ipcdef.hpp"
#include "tsc.hpp"

#include "avbts_osipc.hpp"

#include <ntddndis.h>

#include <map>
#include <list>

class WindowsPCAPNetworkInterface : public OSNetworkInterface {
	friend class WindowsPCAPNetworkInterfaceFactory;
private:
	pfhandle_t handle;
	LinkLayerAddress local_addr;
public:
	virtual net_result send( LinkLayerAddress *addr, uint8_t *payload, size_t length, bool timestamp ) {
		packet_addr_t dest;
		addr->toOctetArray( dest.addr );
		if( sendFrame( handle, &dest, PTP_ETHERTYPE, payload, length ) != PACKET_NO_ERROR ) return net_fatal;
		return net_succeed;
	}
	virtual net_result recv( LinkLayerAddress *addr, uint8_t *payload, size_t &length ) {
		packet_addr_t dest;
		packet_error_t pferror = recvFrame( handle, &dest, payload, length );
		if( pferror != PACKET_NO_ERROR && pferror != PACKET_RECVTIMEOUT_ERROR ) return net_fatal;
		if( pferror == PACKET_RECVTIMEOUT_ERROR ) return net_trfail;
		*addr = LinkLayerAddress( dest.addr );
		return net_succeed;
	}
	virtual void getLinkLayerAddress( LinkLayerAddress *addr ) {
		*addr = local_addr;
	}
	virtual unsigned getPayloadOffset() {
		return PACKET_HDR_LENGTH;
	}
	virtual ~WindowsPCAPNetworkInterface() {
		closeInterface( handle );
		if( handle != NULL ) freePacketHandle( handle );
	}
protected:
	WindowsPCAPNetworkInterface() { handle = NULL; };
};

class WindowsPCAPNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	virtual bool createInterface( OSNetworkInterface **net_iface, InterfaceLabel *label, HWTimestamper *timestamper ) {
		WindowsPCAPNetworkInterface *net_iface_l = new WindowsPCAPNetworkInterface();
		LinkLayerAddress *addr = dynamic_cast<LinkLayerAddress *>(label);
		if( addr == NULL ) goto error_nofree;
		net_iface_l->local_addr = *addr;
		packet_addr_t pfaddr;
		addr->toOctetArray( pfaddr.addr );
		if( mallocPacketHandle( &net_iface_l->handle ) != PACKET_NO_ERROR ) goto error_nofree;
		if( openInterfaceByAddr( net_iface_l->handle, &pfaddr, 1 ) != PACKET_NO_ERROR ) goto error_free_handle;
		if( packetBind( net_iface_l->handle, PTP_ETHERTYPE ) != PACKET_NO_ERROR ) goto error_free_handle;
		*net_iface = net_iface_l;

		return true;

error_free_handle:
error_nofree:
		delete net_iface_l;

		return false;
	}
};

class WindowsLock : public OSLock {
	friend class WindowsLockFactory;
private:
	OSLockType type;
	DWORD thread_id;
	HANDLE lock_c;
	OSLockResult lock_l( DWORD timeout ) {
		DWORD wait_result = WaitForSingleObject( lock_c, timeout );
		if( wait_result == WAIT_TIMEOUT ) return oslock_held;
		else if( wait_result == WAIT_OBJECT_0 ) return oslock_ok;
		else return oslock_fail;

	}
	OSLockResult nonreentrant_lock_l( DWORD timeout ) {
		OSLockResult result;
		DWORD wait_result;
		wait_result = WaitForSingleObject( lock_c, timeout );
		if( wait_result == WAIT_OBJECT_0 ) {
			if( thread_id == GetCurrentThreadId() ) {
				result = oslock_self;
				ReleaseMutex( lock_c );
			} else {
				result = oslock_ok;
				thread_id = GetCurrentThreadId();
			}
		} else if( wait_result == WAIT_TIMEOUT ) result = oslock_held;
		else result = oslock_fail;

		return result;
	}
protected:
	WindowsLock() {
		lock_c = NULL;
	}
	bool initialize( OSLockType type ) {
		lock_c = CreateMutex( NULL, false, NULL );
		if( lock_c == NULL ) return false;
		this->type = type;
		return true;
	}
	OSLockResult lock() {
		if( type == oslock_recursive ) {
			return lock_l( INFINITE );
		}
		return nonreentrant_lock_l( INFINITE );
	}
	OSLockResult trylock() {
		if( type == oslock_recursive ) {
			return lock_l( 0 );
		}
		return nonreentrant_lock_l( 0 );
	}
	OSLockResult unlock() {
		ReleaseMutex( lock_c );
		return oslock_ok;
	}
};

class WindowsLockFactory : public OSLockFactory {
public:
	OSLock *createLock( OSLockType type ) {
		WindowsLock *lock = new WindowsLock();
		if( !lock->initialize( type )) {
			delete lock;
			lock = NULL;
		}
		return lock;
	}
};

class WindowsCondition : public OSCondition {
	friend class WindowsConditionFactory;
private:
	SRWLOCK lock;
	CONDITION_VARIABLE condition;
protected:
	bool initialize() {
		InitializeSRWLock( &lock );
		InitializeConditionVariable( &condition );
		return true;
	}
public:
	bool wait_prelock() {
		AcquireSRWLockExclusive( &lock );
		up();
		return true;
	}
	bool wait() {
		BOOL result = SleepConditionVariableSRW( &condition, &lock, INFINITE, 0 );
		bool ret = false;
		if( result == TRUE ) {
			down();
			ReleaseSRWLockExclusive( &lock );
			ret = true;
		}
		return ret;
	}
	bool signal() {
		AcquireSRWLockExclusive( &lock );
		if( waiting() ) WakeAllConditionVariable( &condition );
		ReleaseSRWLockExclusive( &lock );
		return true;
	}
};

class WindowsConditionFactory : public OSConditionFactory {
public:
	OSCondition *createCondition() {
		WindowsCondition *result = new WindowsCondition();
		return result->initialize() ? result : NULL;
	}
};

class WindowsTimerQueue;

struct TimerQueue_t;

struct WindowsTimerQueueHandlerArg {
	HANDLE timer_handle;
	HANDLE queue_handle;
	event_descriptor_t *inner_arg;
	ostimerq_handler func;
	int type;
	bool rm;
	WindowsTimerQueue *queue;
	TimerQueue_t *timer_queue;
};

typedef std::list<WindowsTimerQueueHandlerArg *> TimerArgList_t;
struct TimerQueue_t {
	TimerArgList_t arg_list;
	HANDLE queue_handle;
	SRWLOCK lock;
};


VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

typedef std::map<int,TimerQueue_t> TimerQueueMap_t;

class WindowsTimerQueue : public OSTimerQueue {
	friend class WindowsTimerQueueFactory;
	friend VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );
private:
	TimerQueueMap_t timerQueueMap;
	TimerArgList_t retiredTimers;
	SRWLOCK retiredTimersLock;
	void cleanupRetiredTimers() {
		AcquireSRWLockExclusive( &retiredTimersLock );
		while( !retiredTimers.empty() ) {
			WindowsTimerQueueHandlerArg *retired_arg = retiredTimers.front();
			retiredTimers.pop_front();
			ReleaseSRWLockExclusive( &retiredTimersLock );
			DeleteTimerQueueTimer( retired_arg->queue_handle, retired_arg->timer_handle, INVALID_HANDLE_VALUE );
			if( retired_arg->rm ) delete retired_arg->inner_arg;
			delete retired_arg;
			AcquireSRWLockExclusive( &retiredTimersLock );
		}
		ReleaseSRWLockExclusive( &retiredTimersLock );

	}
protected:
	WindowsTimerQueue() {
		InitializeSRWLock( &retiredTimersLock );
	};
public:
	bool addEvent( unsigned long micros, int type, ostimerq_handler func, event_descriptor_t *arg, bool rm, unsigned *event ) {
		WindowsTimerQueueHandlerArg *outer_arg = new WindowsTimerQueueHandlerArg();
		cleanupRetiredTimers();
		if( timerQueueMap.find(type) == timerQueueMap.end() ) {
			timerQueueMap[type].queue_handle = CreateTimerQueue();
			InitializeSRWLock( &timerQueueMap[type].lock );
		}
		outer_arg->queue_handle = timerQueueMap[type].queue_handle;
		outer_arg->inner_arg = arg;
		outer_arg->func = func;
		outer_arg->queue = this;
		outer_arg->type = type;
		outer_arg->rm = rm;
		outer_arg->timer_queue = &timerQueueMap[type];
		AcquireSRWLockExclusive( &timerQueueMap[type].lock );
		CreateTimerQueueTimer( &outer_arg->timer_handle, timerQueueMap[type].queue_handle, WindowsTimerQueueHandler, (void *) outer_arg, micros/1000, 0, 0 );
		timerQueueMap[type].arg_list.push_front(outer_arg);
		ReleaseSRWLockExclusive( &timerQueueMap[type].lock );
		return true;
	}
	bool cancelEvent( int type, unsigned *event ) {
		TimerQueueMap_t::iterator iter = timerQueueMap.find( type );
		if( iter == timerQueueMap.end() ) return false;
		AcquireSRWLockExclusive( &timerQueueMap[type].lock );
		while( ! timerQueueMap[type].arg_list.empty() ) {
			WindowsTimerQueueHandlerArg *del_arg = timerQueueMap[type].arg_list.front();
			timerQueueMap[type].arg_list.pop_front();
			ReleaseSRWLockExclusive( &timerQueueMap[type].lock );
			DeleteTimerQueueTimer( del_arg->queue_handle, del_arg->timer_handle, INVALID_HANDLE_VALUE );
			if( del_arg->rm ) delete del_arg->inner_arg;
			delete del_arg;
			AcquireSRWLockExclusive( &timerQueueMap[type].lock );
		}
		ReleaseSRWLockExclusive( &timerQueueMap[type].lock );

		return true;
	}
};

VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

class WindowsTimerQueueFactory : public OSTimerQueueFactory {
public:
    virtual OSTimerQueue *createOSTimerQueue() {
        WindowsTimerQueue *timerq = new WindowsTimerQueue();
        return timerq;
    };
};

class WindowsTimer : public OSTimer {
    friend class WindowsTimerFactory;
public:
    virtual unsigned long sleep( unsigned long micros ) {
        Sleep( micros/1000 );
        return micros;
    }
protected:
    WindowsTimer() {};
};

class WindowsTimerFactory : public OSTimerFactory {
public:
    virtual OSTimer *createTimer() {
        return new WindowsTimer();
    }
};

struct OSThreadArg {
    OSThreadFunction func;
    void *arg;
    OSThreadExitCode ret;
};

DWORD WINAPI OSThreadCallback( LPVOID input );

class WindowsThread : public OSThread {
    friend class WindowsThreadFactory;
private:
    HANDLE thread_id;
    OSThreadArg *arg_inner;
public:
    virtual bool start( OSThreadFunction function, void *arg ) {
        arg_inner = new OSThreadArg();
        arg_inner->func = function;
        arg_inner->arg = arg;
        thread_id = CreateThread( NULL, 0, OSThreadCallback, arg_inner, 0, NULL );
        if( thread_id == NULL ) return false;
        else return true;
    }
    virtual bool join( OSThreadExitCode &exit_code ) {
        if( WaitForSingleObject( thread_id, INFINITE ) != WAIT_OBJECT_0 ) return false;
        exit_code = arg_inner->ret;
        delete arg_inner;
        return true;
    }
protected:
	WindowsThread() {};
};

class WindowsThreadFactory : public OSThreadFactory {
public:
	OSThread *createThread() {
		return new WindowsThread();
	}
};

#define NETCLOCK_HZ_OTHER 1056000000
#define NETCLOCK_HZ_NANO 1000000000
#define ONE_WAY_PHY_DELAY 8000
#define NANOSECOND_CLOCK_PART_DESCRIPTION "I217-LM"
#define NETWORK_CARD_ID_PREFIX "\\\\.\\"
#define OID_INTEL_GET_RXSTAMP 0xFF020264
#define OID_INTEL_GET_TXSTAMP 0xFF020263
#define OID_INTEL_GET_SYSTIM  0xFF020262
#define OID_INTEL_SET_SYSTIM  0xFF020261

class WindowsTimestamper : public HWTimestamper {
private:
    // No idea whether the underlying implementation is thread safe
	HANDLE miniport;
	LARGE_INTEGER tsc_hz;
	LARGE_INTEGER netclock_hz;
	DWORD readOID( NDIS_OID oid, void *output_buffer, DWORD size, DWORD *size_returned ) {
		NDIS_OID oid_l = oid;
		DWORD rc = DeviceIoControl(
			miniport,
			IOCTL_NDIS_QUERY_GLOBAL_STATS,
			&oid_l,
			sizeof(oid_l),
			output_buffer,
			size,
			size_returned,
			NULL );
		if( rc == 0 ) return GetLastError();
		return ERROR_SUCCESS;
	}
	Timestamp nanoseconds64ToTimestamp( uint64_t time ) {
		Timestamp timestamp;
		timestamp.nanoseconds = time % 1000000000;
		timestamp.seconds_ls = (time / 1000000000) & 0xFFFFFFFF;
		timestamp.seconds_ms = (uint16_t)((time / 1000000000) >> 32);
		return timestamp;
	}
	uint64_t scaleNativeClockToNanoseconds( uint64_t time ) {
		long double scaled_output = ((long double)netclock_hz.QuadPart)/1000000000;
		scaled_output = ((long double) time)/scaled_output;
		return (uint64_t) scaled_output;
	}
	uint64_t scaleTSCClockToNanoseconds( uint64_t time ) {
		long double scaled_output = ((long double)tsc_hz.QuadPart)/1000000000;
		scaled_output = ((long double) time)/scaled_output;
		return (uint64_t) scaled_output;
	}
public:
	virtual bool HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface );
    virtual bool HWTimestamper_gettime( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock, uint32_t *nominal_clock_rate )
    {
        DWORD buf[6];
        DWORD returned;
        uint64_t now_net, now_tsc;
        DWORD result;

        memset( buf, 0xFF, sizeof( buf ));
        if(( result = readOID( OID_INTEL_GET_SYSTIM, buf, sizeof(buf), &returned )) != ERROR_SUCCESS ) return false;

        now_net = (((uint64_t)buf[1]) << 32) | buf[0];
        now_net = scaleNativeClockToNanoseconds( now_net );
        *device_time = nanoseconds64ToTimestamp( now_net );
		device_time->_version = version;

        now_tsc = (((uint64_t)buf[3]) << 32) | buf[2];
        now_tsc = scaleTSCClockToNanoseconds( now_tsc );
        *system_time = nanoseconds64ToTimestamp( now_tsc );
		system_time->_version = version;

        return true;
    }

	virtual int HWTimestamper_txtimestamp( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp, unsigned &clock_value, bool last )
	{
		DWORD buf[4], buf_tmp[4];
		DWORD returned = 0;
		uint64_t tx_r,tx_s;
		DWORD result;
		while(( result = readOID( OID_INTEL_GET_TXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) return -1;
		if( returned != sizeof(buf_tmp) ) return -72;
		tx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		tx_s = scaleNativeClockToNanoseconds( tx_r );
		tx_s += ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( tx_s );
		timestamp._version = version;

		return 0;
	}

	virtual int HWTimestamper_rxtimestamp( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp, unsigned &clock_value, bool last )
	{
		DWORD buf[4], buf_tmp[4];
		DWORD returned;
		uint64_t rx_r,rx_s;
		DWORD result;
		uint16_t packet_sequence_id;
		while(( result = readOID( OID_INTEL_GET_RXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) return -1;
		if( returned != sizeof(buf_tmp) ) return -72;
		packet_sequence_id = *((uint32_t *) buf+3) >> 16;
		if( PLAT_ntohs( packet_sequence_id ) != sequenceId ) return -72;
		rx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		rx_s = scaleNativeClockToNanoseconds( rx_r );
		rx_s -= ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( rx_s );
		timestamp._version = version;

		return 0;
	}
};



class WindowsNamedPipeIPC : public OS_IPC {
private:
	HANDLE pipe;
public:
	WindowsNamedPipeIPC() { };
	~WindowsNamedPipeIPC() {
		CloseHandle( pipe );
	}
	virtual bool init( OS_IPC_ARG *arg = NULL ) {
		char pipename[64];
		PLAT_strncpy( pipename, PIPE_PREFIX, 63 );
		PLAT_strncpy( pipename+strlen(pipename), P802_1AS_PIPENAME, 63-strlen(pipename) );
		pipe = CreateNamedPipe( pipename, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_MESSAGE, PIPE_UNLIMITED_INSTANCES,
			OUTSTANDING_MESSAGES*sizeof( WindowsNPipeMessage ), 0, 0, NULL );
		if( pipe == INVALID_HANDLE_VALUE ) return false;
		return true;
	}
	virtual bool update( int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset, FrequencyRatio ls_freq_offset, uint64_t local_time ) {
		WindowsNPipeMessage msg( ml_phoffset, ls_phoffset, ml_freqoffset, ls_freq_offset, local_time );
		if( !msg.write( pipe )) {
			CloseHandle(pipe);
			return init();
		}
		return true;
	}
};

#endif
