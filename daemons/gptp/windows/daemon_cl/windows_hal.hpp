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

/**@file*/

#include <minwindef.h>
#include <IPCListener.hpp>
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "packet.hpp"
#include "ieee1588.hpp"
#include "iphlpapi.h"
#include "windows_ipc.hpp"
#include "tsc.hpp"

#include "avbts_osipc.hpp"

#include <ntddndis.h>

#include <map>
#include <list>

/**
 * WindowsPCAPNetworkInterface implements an interface to the network adapter
 * through calls to the publicly available libraries known as PCap.
 */
class WindowsPCAPNetworkInterface : public OSNetworkInterface {
	friend class WindowsPCAPNetworkInterfaceFactory;
private:
	pfhandle_t handle;
	LinkLayerAddress local_addr;
public:
	/**
	 * @brief  Sends a packet to a remote address
	 * @param  addr [in] Destination link Layer address
	 * @param  payload [in] Data buffer
	 * @param  length Size of buffer
	 * @param  timestamp TRUE: Use timestamp, FALSE otherwise
	 * @return net_result structure
	 */
	virtual net_result send( LinkLayerAddress *addr, uint8_t *payload, size_t length, bool timestamp ) {
		packet_addr_t dest;
		addr->toOctetArray( dest.addr );
		if( sendFrame( handle, &dest, PTP_ETHERTYPE, payload, length ) != PACKET_NO_ERROR ) return net_fatal;
		return net_succeed;
	}
	/**
	 * @brief  Receives a packet from a remote address
	 * @param  addr [out] Remote link layer address
	 * @param  payload [out] Data buffer
	 * @param  length [out] Length of received data
	 * @param  delay [in] Specifications for PHY input and output delays in nanoseconds
	 * @return net_result structure
	 */
	virtual net_result nrecv(LinkLayerAddress *addr, uint8_t *payload, size_t &length, struct phy_delay *delay) {
		packet_addr_t dest;
		packet_error_t pferror = recvFrame( handle, &dest, payload, length );
		if( pferror != PACKET_NO_ERROR && pferror != PACKET_RECVTIMEOUT_ERROR ) return net_fatal;
		if( pferror == PACKET_RECVTIMEOUT_ERROR ) return net_trfail;
		*addr = LinkLayerAddress( dest.addr );
		return net_succeed;
	}
	/**
	 * @brief  Gets the link layer address (MAC) of the network adapter
	 * @param  addr [out] Link layer address
	 * @return void
	 */
	virtual void getLinkLayerAddress( LinkLayerAddress *addr ) {
		*addr = local_addr;
	}

	/**
	* @brief Watch for netlink changes.
	*/
	virtual void watchNetLink(IEEE1588Port *pPort);

	/**
	 * @brief  Gets the offset to the start of data in the Layer 2 Frame
	 * @return ::PACKET_HDR_LENGTH
	 */
	virtual unsigned getPayloadOffset() {
		return PACKET_HDR_LENGTH;
	}
	/**
	 * Destroys the network interface
	 */
	virtual ~WindowsPCAPNetworkInterface() {
		closeInterface( handle );
		if( handle != NULL ) freePacketHandle( handle );
	}
protected:
	/**
	 * Default constructor
	 */
	WindowsPCAPNetworkInterface() { handle = NULL; };
};

/**
 * WindowsPCAPNetworkInterface implements an interface to the network adapter
 * through calls to the publicly available libraries known as PCap.
 */
class WindowsPCAPNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	/**
	 * @brief  Create a network interface
	 * @param  net_iface [in] Network interface
	 * @param  label [in] Interface label
	 * @param  timestamper [in] HWTimestamper instance
	 * @return TRUE success; FALSE error
	 */
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

/**
 * Provides lock interface for windows
 */
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
	/**
	 * Default constructor
	 */
	WindowsLock() {
		lock_c = NULL;
	}
	/**
	 * @brief  Initializes lock interface
	 * @param  type OSLockType
	 */
	bool initialize( OSLockType type ) {
		lock_c = CreateMutex( NULL, false, NULL );
		if( lock_c == NULL ) return false;
		this->type = type;
		return true;
	}
	/**
	 * @brief Acquires lock
	 * @return OSLockResult type
	 */
	OSLockResult lock() {
		if( type == oslock_recursive ) {
			return lock_l( INFINITE );
		}
		return nonreentrant_lock_l( INFINITE );
	}
	/**
	 * @brief  Tries to acquire lock
	 * @return OSLockResult type
	 */
	OSLockResult trylock() {
		if( type == oslock_recursive ) {
			return lock_l( 0 );
		}
		return nonreentrant_lock_l( 0 );
	}
	/**
	 * @brief  Releases lock
	 * @return OSLockResult type
	 */
	OSLockResult unlock() {
		ReleaseMutex( lock_c );
		return oslock_ok;
	}
};

/**
 * Provides the LockFactory abstraction
 */
class WindowsLockFactory : public OSLockFactory {
public:
	/**
	 * @brief  Creates a new lock mechanism
	 * @param  type Lock type - OSLockType
	 * @return New lock on OSLock format
	 */
	OSLock *createLock( OSLockType type ) {
		WindowsLock *lock = new WindowsLock();
		if( !lock->initialize( type )) {
			delete lock;
			lock = NULL;
		}
		return lock;
	}
};

/**
 * Provides a OSCondition interface for windows
 */
class WindowsCondition : public OSCondition {
	friend class WindowsConditionFactory;
private:
	SRWLOCK lock;
	CONDITION_VARIABLE condition;
protected:
	/**
	 * Initializes the locks and condition variables
	 */
	bool initialize() {
		InitializeSRWLock( &lock );
		InitializeConditionVariable( &condition );
		return true;
	}
public:
	/**
	 * @brief  Acquire a new lock and increment the condition counter
	 * @return true
	 */
	bool wait_prelock() {
		AcquireSRWLockExclusive( &lock );
		up();
		return true;
	}
	/**
	 * @brief  Waits for a condition and decrements the condition
	 * counter when the condition is met.
	 * @return TRUE if the condition is met and the lock is released. FALSE otherwise.
	 */
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
	/**
	 * @brief Sends a signal to unblock other threads
	 * @return true
	 */
	bool signal() {
		AcquireSRWLockExclusive( &lock );
		if( waiting() ) WakeAllConditionVariable( &condition );
		ReleaseSRWLockExclusive( &lock );
		return true;
	}
};

/**
 * Condition factory for windows
 */
class WindowsConditionFactory : public OSConditionFactory {
public:
	OSCondition *createCondition() {
		WindowsCondition *result = new WindowsCondition();
		return result->initialize() ? result : NULL;
	}
};

class WindowsTimerQueue;

struct TimerQueue_t;

/**
 * Timer queue handler arguments structure
 * @todo Needs more details from original developer
 */
struct WindowsTimerQueueHandlerArg {
	HANDLE timer_handle;	/*!< timer handler */
	HANDLE queue_handle;	/*!< queue handler */
	event_descriptor_t *inner_arg;	/*!< Event inner arguments */
	ostimerq_handler func;	/*!< Timer queue callback*/
	int type;				/*!< Type of timer */
	bool rm;				/*!< Flag that signalizes the argument deletion*/
	WindowsTimerQueue *queue;	/*!< Windows Timer queue */
	TimerQueue_t *timer_queue;	/*!< Timer queue*/
};

/**
 * Creates a list of type WindowsTimerQueueHandlerArg
 */
typedef std::list<WindowsTimerQueueHandlerArg *> TimerArgList_t;
/**
 * Timer queue type
 */
struct TimerQueue_t {
	TimerArgList_t arg_list;		/*!< Argument list */
	HANDLE queue_handle;			/*!< Handle to the timer queue */
	SRWLOCK lock;					/*!< Lock type */
};

/**
 * Windows Timer queue handler callback definition
 */
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

/**
 * Creates a map for the timerQueue
 */
typedef std::map<int,TimerQueue_t> TimerQueueMap_t;

/**
 * Windows timer queue interface
 */
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
	/**
	 * Default constructor. Initializes timers lock
	 */
	WindowsTimerQueue() {
		InitializeSRWLock( &retiredTimersLock );
	};
public:
	/**
	 * @brief  Create a new event and add it to the timer queue
	 * @param  micros Time in microsseconds
	 * @param  type ::Event type
	 * @param  func Callback function
	 * @param  arg [in] Event arguments
	 * @param  rm when true, allows elements to be deleted from the queue
	 * @param  event [in] Pointer to the event to be created
	 * @return Always return true
	 */
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
	/**
	 * @brief  Cancels an event from the queue
	 * @param  type ::Event type
	 * @param  event [in] Pointer to the event to be removed
	 * @return Always returns true.
	 */
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

/**
 * Windows callback for the timer queue handler
 */
VOID CALLBACK WindowsTimerQueueHandler( PVOID arg_in, BOOLEAN ignore );

/**
 * Windows implementation of OSTimerQueueFactory
 */
class WindowsTimerQueueFactory : public OSTimerQueueFactory {
public:
	/**
	 * @brief  Creates a new timer queue
	 * @param  clock [in] IEEE1588Clock type
	 * @return new timer queue
	 */
    virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock ) {
        WindowsTimerQueue *timerq = new WindowsTimerQueue();
        return timerq;
    };
};

/**
 * Windows implementation of OSTimer
 */
class WindowsTimer : public OSTimer {
    friend class WindowsTimerFactory;
public:
	/**
	 * @brief  Sleep for an amount of time
	 * @param  micros Time in microsseconds
	 * @return Time in microsseconds
	 */
    virtual unsigned long sleep( unsigned long micros ) {
        Sleep( micros/1000 );
        return micros;
    }
protected:
	/**
	 * Default constructor
	 */
    WindowsTimer() {};
};

/**
 * Windows implementation of OSTimerFactory
 */
class WindowsTimerFactory : public OSTimerFactory {
public:
	/**
	 * @brief  Creates a new timer
	 * @return New windows OSTimer
	 */
    virtual OSTimer *createTimer() {
        return new WindowsTimer();
    }
};

/**
 * OSThread argument structure
 */
struct OSThreadArg {
    OSThreadFunction func;	/*!< Thread callback function */
    void *arg;				/*!< Thread arguments */
    OSThreadExitCode ret;	/*!< Return value */
};

/**
 * Windows OSThread callback
 */
DWORD WINAPI OSThreadCallback( LPVOID input );

/**
 * Implements OSThread interface for windows
 */
class WindowsThread : public OSThread {
    friend class WindowsThreadFactory;
private:
    HANDLE thread_id;
    OSThreadArg *arg_inner;
public:
	/**
	 * @brief  Starts a new thread
	 * @param  function Function to be started as a new thread
	 * @param  arg [in] Function's arguments
	 * @return TRUE if successfully started, FALSE otherwise.
	 */
    virtual bool start( OSThreadFunction function, void *arg ) {
        arg_inner = new OSThreadArg();
        arg_inner->func = function;
        arg_inner->arg = arg;
        thread_id = CreateThread( NULL, 0, OSThreadCallback, arg_inner, 0, NULL );
        if( thread_id == NULL ) return false;
        else return true;
    }
	/**
	 * @brief  Joins a terminated thread
	 * @param  exit_code [out] Thread's return code
	 * @return TRUE in case of success. FALSE otherwise.
	 */
    virtual bool join( OSThreadExitCode &exit_code ) {
        if( WaitForSingleObject( thread_id, INFINITE ) != WAIT_OBJECT_0 ) return false;
        exit_code = arg_inner->ret;
        delete arg_inner;
        return true;
    }
protected:
	/**
	 * Default constructor
	 */
	WindowsThread() {};
};

/**
 * Provides a windows implmementation of OSThreadFactory
 */
class WindowsThreadFactory : public OSThreadFactory {
public:
	/**
	 * @brief  Creates a new windows thread
	 * @return New thread of type OSThread
	 */
	OSThread *createThread() {
		return new WindowsThread();
	}
};

#define NETCLOCK_HZ_OTHER 1056000000		/*!< @todo Not sure about what that means*/
#define NETCLOCK_HZ_NANO 1000000000			/*!< 1 Hertz in nanoseconds */
#define ONE_WAY_PHY_DELAY 8000				/*!< Phy delay TX/RX */
#define NANOSECOND_CLOCK_PART_DESCRIPTION "I217-LM"	/*!< Network adapter description */
#define NETWORK_CARD_ID_PREFIX "\\\\.\\"			/*!< Network adapter prefix */
#define OID_INTEL_GET_RXSTAMP 0xFF020264			/*!< Get RX timestamp code*/
#define OID_INTEL_GET_TXSTAMP 0xFF020263			/*!< Get TX timestamp code*/
#define OID_INTEL_GET_SYSTIM  0xFF020262			/*!< Get system time code */
#define OID_INTEL_SET_SYSTIM  0xFF020261			/*!< Set system time code */

/**
 * Windows HWTimestamper implementation
 */
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
	/**
	 * @brief  Initializes the network adaptor and the hw timestamper interface
	 * @param  iface_label InterfaceLabel
	 * @param  net_iface Network interface
	 * @return TRUE if success; FALSE if error
	 */
	virtual bool HWTimestamper_init( InterfaceLabel *iface_label, OSNetworkInterface *net_iface );
	/**
	 * @brief  Get the cross timestamping information.
	 * The gPTP subsystem uses these samples to calculate
	 * ratios which can be used to translate or extrapolate
	 * one clock into another clock reference. The gPTP service
	 * uses these supplied cross timestamps to perform internal
	 * rate estimation and conversion functions.
	 * @param  system_time [out] System time
	 * @param  device_time [out] Device time
	 * @param  local_clock [out] Local clock
	 * @param  nominal_clock_rate [out] Nominal clock rate
	 * @return True in case of success. FALSE in case of error
	 */
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

	/**
	 * @brief  Gets the TX timestamp
	 * @param  identity [in] PortIdentity interface
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] TX hardware timestamp
	 * @param  clock_value Not used
	 * @param  last Not used
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	virtual int HWTimestamper_txtimestamp( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp, unsigned &clock_value, bool last )
	{
		DWORD buf[4], buf_tmp[4];
		DWORD returned = 0;
		uint64_t tx_r,tx_s;
		DWORD result;
		while(( result = readOID( OID_INTEL_GET_TXSTAMP, buf_tmp, sizeof(buf_tmp), &returned )) == ERROR_SUCCESS ) {
			memcpy( buf, buf_tmp, sizeof( buf ));
		}
		if( result != ERROR_GEN_FAILURE ) {
			fprintf( stderr, "Error is: %d\n", result );
			return GPTP_EC_FAILURE;
		}
		if( returned != sizeof(buf_tmp) ) return GPTP_EC_EAGAIN;
		tx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		tx_s = scaleNativeClockToNanoseconds( tx_r );
		tx_s += ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( tx_s );
		timestamp._version = version;

		return GPTP_EC_SUCCESS;
	}

	/**
	 * @brief  Gets the RX timestamp
	 * @param  identity PortIdentity interface
	 * @param  sequenceId  Sequence ID
	 * @param  timestamp [out] RX hardware timestamp
	 * @param  clock_value [out] Not used
	 * @param  last Not used
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
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
		if( result != ERROR_GEN_FAILURE ) return GPTP_EC_FAILURE;
		if( returned != sizeof(buf_tmp) ) return GPTP_EC_EAGAIN;
		packet_sequence_id = *((uint32_t *) buf+3) >> 16;
		if( PLAT_ntohs( packet_sequence_id ) != sequenceId ) return GPTP_EC_EAGAIN;
		rx_r = (((uint64_t)buf[1]) << 32) | buf[0];
		rx_s = scaleNativeClockToNanoseconds( rx_r );
		rx_s -= ONE_WAY_PHY_DELAY;
		timestamp = nanoseconds64ToTimestamp( rx_s );
		timestamp._version = version;

		return GPTP_EC_SUCCESS;
	}
};


/**
 * Named pipe interface
 */
class WindowsNamedPipeIPC : public OS_IPC {
private:
	HANDLE pipe_;
	LockableOffset lOffset_;
	PeerList peerList_;
public:
	/**
	 * Default constructor. Initializes the IPC interface
	 */
	WindowsNamedPipeIPC() : pipe_(INVALID_HANDLE_VALUE) { };
	/**
	 * Destroys the IPC interface
	 */
	~WindowsNamedPipeIPC() {
		if (pipe_ != 0 && pipe_ != INVALID_HANDLE_VALUE)
			::CloseHandle(pipe_);
	}
	/**
	 * @brief  Initializes the IPC arguments
	 * @param  arg [in] IPC arguments. Not in use
	 * @return Always returns TRUE.
	 */
	virtual bool init(OS_IPC_ARG *arg = NULL);
	/**
	 * @brief  Updates IPC interface values
	 * @param  ml_phoffset Master to local phase offset
	 * @param  ls_phoffset Local to system phase offset
	 * @param  ml_freqoffset Master to local frequency offset
	 * @param  ls_freq_offset Local to system frequency offset
	 * @param  local_time Local time
	 * @param  sync_count Counts of sync mesasges
	 * @param  pdelay_count Counts of pdelays
	 * @param  port_state PortState information
     * @param  asCapable asCapable flag
	 * @return TRUE if sucess; FALSE if error
	 */
	virtual bool update(int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset, FrequencyRatio ls_freq_offset, uint64_t local_time,
		uint32_t sync_count, uint32_t pdelay_count, PortState port_state, bool asCapable);
};

#endif
