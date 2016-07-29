/******************************************************************************

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

#ifndef LINUX_HAL_COMMON_HPP
#define LINUX_HAL_COMMON_HPP

/**@file*/

#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "avbts_osipc.hpp"
#include "ieee1588.hpp"

#include <list>

#define ONE_WAY_PHY_DELAY 400	/*!< One way phy delay. TX or RX phy delay default value*/
#define P8021AS_MULTICAST "\x01\x80\xC2\x00\x00\x0E"	/*!< Default multicast address*/
#define PTP_DEVICE "/dev/ptpXX"			/*!< Default PTP device */
#define PTP_DEVICE_IDX_OFFS 8			/*!< PTP device index offset*/
#define CLOCKFD 3						/*!< Clock file descriptor */
#define FD_TO_CLOCKID(fd)       ((~(clockid_t) (fd) << 3) | CLOCKFD)	/*!< Converts an FD to CLOCKID */
struct timespec;

/**
 * @brief  Converts timestamp in the struct timespec format to Timestamp
 * @param  ts Timestamp on struct timespec format
 * @return timestamp on the Timestamp format
 */
extern Timestamp tsToTimestamp(struct timespec *ts);

struct TicketingLockPrivate;

/**
 * @brief Provides the type for the TicketingLock private structure
 */
typedef struct TicketingLockPrivate * TicketingLockPrivate_t;

/**
 * @brief TicketingLock: Implements the ticket lock algorithm.
 * A ticket lock consists of two counters, one containing the
 * number of requests to acquire the lock, and the other the
 * number of times the lock has been released.  A processor acquires the lock
 * by performing a fetch and increment operation on the request counter and
 * waiting until the result its ticket is equal to the value of the release
 * counter. It releases the lock by incrementing the release counter.
 */
class TicketingLock {
public:
	/**
	 * @brief  Lock mechanism.
	 * Gets a ticket and try locking the process.
	 * @param  got [out] If non-null, it is set to TRUE when the lock is acquired. FALSE otherwise.
	 * @return TRUE when successfully got the lock, FALSE otherwise.
	 */
	bool lock( bool *got = NULL );

	/**
	 * @brief  Unlock mechanism. Increments the release counter and unblock other threads
	 * waiting for a condition flag.
	 * @return TRUE in case of success, FALSE otherwise.
	 */
	bool unlock();

	/**
	 * @brief Initializes all flags and counters. Create private structures.
	 * @return TRUE in case of success, FALSE otherwise.
	 */
	bool init();

	/**
	 * @brief Default constructor sets some flags to false that will be initialized on
	 * the init method. Protects against using lock/unlock without calling init.
	 */
	TicketingLock();

	/**
	 * @brief Deletes the object and private structures.
	 */
	~TicketingLock();
private:
	bool init_flag;
	TicketingLockPrivate_t _private;
	bool in_use;
	uint8_t cond_ticket_issue;
	uint8_t cond_ticket_serving;
};

/**
 * @brief LinuxTimestamper: Provides a generic hardware
 * timestamp interface for linux based systems.
 */
class LinuxTimestamper : public HWTimestamper {
public:
	/**
	 * @brief Destructor
	 */
	virtual ~LinuxTimestamper() = 0;

	/**
	 * @brief  Provides a generic method for initializing timestamp interfaces
	 * @param  ifindex Network device interface index
	 * @param  sd Socket descriptor
	 * @param  lock [in] Pointer to ticketing Lock object
	 * @return TRUE if success, FALSE in case of error
	 */
	virtual bool post_init( int ifindex, int sd, TicketingLock *lock ) = 0;
};

/**
 * @brief Provides a Linux network generic interface
 */
class LinuxNetworkInterface : public OSNetworkInterface {
	friend class LinuxNetworkInterfaceFactory;
private:
	LinkLayerAddress local_addr;
	int sd_event;
	int sd_general;
	LinuxTimestamper *timestamper;
	int ifindex;

	TicketingLock net_lock;
public:
	/**
	 * @brief Sends a packet to a remote address
	 * @param addr [in] Remote link layer address
	 * @param etherType [in] The EtherType of the message
	 * @param payload [in] Data buffer
	 * @param length Size of data buffer
	 * @param timestamp TRUE if to use the event socket with the PTP multicast address. FALSE if to use
	 * a general socket.
	 * @return net_fatal if error, net_success if success
	 */
	virtual net_result send
	( LinkLayerAddress *addr, uint16_t etherType, uint8_t *payload, size_t length,
	  bool timestamp );

	/**
	 * @brief  Receives a packet from a remote address
	 * @param  addr [in] Remote link layer address
	 * @param  payload [out] Data buffer
	 * @param  length [out] Size of received data buffer
	 * @return net_succeed in case of successful reception, net_trfail in case there is
	 * an error on the transmit side, net_fatal if error on reception
	 */
	virtual net_result nrecv
	( LinkLayerAddress *addr, uint8_t *payload, size_t &length, struct phy_delay *delay );

	/**
	 * @brief  Disables rx socket descriptor rx queue
	 * @return void
	 */
	void disable_rx_queue();;

	/**
	 * @brief  Clears and enables the rx socket descriptor
	 * @return void
	 */
	void clear_reenable_rx_queue();

	/**
	 * @brief  Gets the local link layer address
	 * @param  addr [out] Pointer to the LinkLayerAddress object
	 * @return void
	 */
	virtual void getLinkLayerAddress( LinkLayerAddress *addr ) {
		*addr = local_addr;
	}

	/**
	 * @brief Watch for net link changes.
	 */
	virtual void watchNetLink(IEEE1588Port *pPort);

	/**
	 * @brief Gets the payload offset
	 * @return payload offset
	 */
	virtual unsigned getPayloadOffset() {
		return 0;
	}
	/**
	 * @brief Destroys the network interface
	 */
	~LinuxNetworkInterface();
protected:
	/**
	 * @brief Default constructor
	 */
	LinuxNetworkInterface() {};
};

/**
 * @brief Provides a list of LinuxNetworkInterface members
 */
typedef std::list<LinuxNetworkInterface *> LinuxNetworkInterfaceList;

struct LinuxLockPrivate;
/**
 * @brief Provides a type for the LinuxLock class
 */
typedef LinuxLockPrivate * LinuxLockPrivate_t;

/**
 * @brief Extends OSLock generic interface to Linux
 */
class LinuxLock : public OSLock {
	friend class LinuxLockFactory;
private:
	OSLockType type;
	LinuxLockPrivate_t _private;
protected:
	/**
	 * @brief Default constructor.
	 */
	LinuxLock() {
		_private = NULL;
	}

	/**
	 * @brief Initializes all mutexes and locks
	 * @param type OSLockType enumeration. If oslock_recursive then set pthreads
	 * attributes to PTHREAD_MUTEX_RECURSIVE
	 * @return If successful, returns oslock_ok. Returns oslock_fail otherwise
	 */
	bool initialize( OSLockType type );

	/**
	 * @brief Destroys mutexes if lock is still valid
	 */
	~LinuxLock();

	/**
	 * @brief  Provides a simple lock mechanism.
	 * @return oslock_fail if lock has failed, oslock_ok otherwise.
	 */
	OSLockResult lock();

	/**
	 * @brief Provides a simple trylock mechanism.
	 * @return oslock_fail if lock has failed, oslock_ok otherwise.
	 */
	OSLockResult trylock();

	/**
	 * @brief  Provides a simple unlock mechanism.
	 * @return oslock_fail if unlock has failed, oslock_ok otherwise.
	 */
	OSLockResult unlock();
};

/**
 * @brief Provides a factory pattern for LinuxLock class
 */
class LinuxLockFactory:public OSLockFactory {
public:
	/**
	 * @brief Creates the locking mechanism
	 * @param type OSLockType enumeration
	 * @return Pointer to OSLock object
	 */
	OSLock * createLock(OSLockType type) {
		LinuxLock *lock = new LinuxLock();
		if (lock->initialize(type) != oslock_ok) {
			delete lock;
			lock = NULL;
		}
		return lock;
	}
};

struct LinuxConditionPrivate;
/**
 * @brief Provides a private type for the LinuxCondition class
 */
typedef struct LinuxConditionPrivate * LinuxConditionPrivate_t;

/**
 * @brief Extends OSCondition class to Linux
 */
class LinuxCondition : public OSCondition {
	friend class LinuxConditionFactory;
private:
	LinuxConditionPrivate_t _private;
protected:

	/**
	 * @brief Initializes locks and mutex conditions
	 * @return TRUE if it is ok, FALSE in case of error
	 */
	bool initialize();
public:

	/**
	 * @brief Counts up the amount of times we call the locking
	 * mechanism
	 * @return TRUE after incrementing the counter.
	 */
	bool wait_prelock();

	/**
	 * @brief  Waits until the ready signal condition is met and decrements
	 * the counter.
	 */
	bool wait();

	/**
	 * @brief  Unblock all threads that are busy waiting for a condition
	 * @return TRUE
	 */
	bool signal();

	/*
	 * Default constructor
	 */
	~LinuxCondition();

	/*
	 * Destructor: Deletes internal variables
	 */
	LinuxCondition() {
		_private = NULL;
	}
};

/**
 * @brief Implements factory design pattern for LinuxCondition class
 */
class LinuxConditionFactory : public OSConditionFactory {
public:
	/**
	 * @brief Creates LinuxCondition objects
	 * @return Pointer to the OSCondition object in case of success. NULL otherwise
	 */
	OSCondition * createCondition() {
		LinuxCondition *result = new LinuxCondition();
		return result->initialize() ? result : NULL;
	}
};

struct LinuxTimerQueueActionArg;

/**
 * @brief Provides a map type for the LinuxTimerQueue class
 */
typedef std::map < int, struct LinuxTimerQueueActionArg *> LinuxTimerQueueMap_t;

/**
 * @brief  Linux timer queue handler. Deals with linux queues
 * @param  arg [in] LinuxTimerQueue arguments
 * @return void
 */
void *LinuxTimerQueueHandler( void *arg );

struct LinuxTimerQueuePrivate;
/**
 * @brief Provides a private type for the LinuxTimerQueue class
 */
typedef struct LinuxTimerQueuePrivate * LinuxTimerQueuePrivate_t;

/**
 * @brief Extends OSTimerQueue to Linux
 */
class LinuxTimerQueue : public OSTimerQueue {
	friend class LinuxTimerQueueFactory;
	friend void *LinuxTimerQueueHandler( void * arg );
private:
	LinuxTimerQueueMap_t timerQueueMap;
	int key;
	bool stop;
	LinuxTimerQueuePrivate_t _private;
	OSLock *lock;
	void LinuxTimerQueueAction( LinuxTimerQueueActionArg *arg );
protected:
	/**
	 * @brief Default constructor
	 */
	LinuxTimerQueue() {
		_private = NULL;
	}

	/**
	 * @brief Initializes internal variables
	 * @return TRUE if success, FALSE otherwise.
	 */
	virtual bool init();
public:
	/**
	 * @brief Deletes pre-allocated internal variables.
	 */
	~LinuxTimerQueue();

	/**
	 * @brief Add an event to the timer queue
	 * @param micros Time in microsseconds
	 * @param type  Event type
	 * @param func Callback
	 * @param arg inner argument of type event_descriptor_t
	 * @param rm when true, allows elements to be deleted from the queue
	 * @param event [inout] Pointer to the event
	 * @return TRUE success, FALSE fail
	 */
	bool addEvent
	( unsigned long micros, int type, ostimerq_handler func,
	  event_descriptor_t * arg, bool rm, unsigned *event );

	/**
	 * @brief Removes an event from the timer queue
	 * @param type Event type
	 * @param event [inout] Pointer to the event
	 * @return TRUE success, FALSE fail
	 */
	bool cancelEvent( int type, unsigned *event );
};

/**
 * @brief Implements factory design pattern for linux
 */
class LinuxTimerQueueFactory : public OSTimerQueueFactory {
public:
	/**
	 * @brief Creates Linux timer queue
	 * @param clock [in] Pointer to IEEE15588Clock type
	 * @return Pointer to OSTimerQueue
	 */
	virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock );
};

/**
 * @brief Extends the OSTimer generic class to Linux
 */
class LinuxTimer : public OSTimer {
	friend class LinuxTimerFactory;
 public:
	/**
	 * @brief Sleeps for a given amount of time in microsseconds
	 * @param  micros Time in micro-seconds
	 * @return -1 if error, micros (input argument) if success
	 */
	virtual unsigned long sleep(unsigned long micros);
 protected:
	/**
	 * @brief Destroys linux timer
	 */
	LinuxTimer() {};
};

/**
 * @brief Provides factory design pattern for LinuxTimer
 */
class LinuxTimerFactory : public OSTimerFactory {
 public:
	 /**
	  * @brief  Creates the linux timer
	  * @return Pointer to OSTimer object
	  */
	virtual OSTimer * createTimer() {
		return new LinuxTimer();
	}
};

/**
 * @brief Provides the default arguments for the OSThread class
 */
struct OSThreadArg {
	OSThreadFunction func;  /*!< Callback function */
	void *arg;	/*!< Input arguments */
	OSThreadExitCode ret; /*!< Return code */
};

/**
 * @brief  OSThread callback
 * @param  input OSThreadArg structure
 * @return void
 */
void *OSThreadCallback(void *input);

struct LinuxThreadPrivate;
/**
 * @brief Provides a private type for the LinuxThread class
 */
typedef LinuxThreadPrivate * LinuxThreadPrivate_t;

/**
 * @brief Extends OSThread class to Linux
 */
class LinuxThread : public OSThread {
	friend class LinuxThreadFactory;
 private:
	LinuxThreadPrivate_t _private;
	OSThreadArg *arg_inner;
 public:
	/**
	 * @brief  Starts a new thread
	 * @param  function Callback to the thread to be started
	 * @param  arg Function's parameters
	 * @return TRUE if no error during init, FALSE otherwise
	 */
	virtual bool start(OSThreadFunction function, void *arg);

	/**
	 * @brief  Joins a new thread
	 * @param  exit_code Callback's return code
	 * @return TRUE if ok, FALSE if error.
	 */
	virtual bool join(OSThreadExitCode & exit_code);
	virtual ~LinuxThread();
 protected:
	LinuxThread();
};

/**
 * @brief Provides factory design pattern for LinuxThread class
 */
class LinuxThreadFactory:public OSThreadFactory {
 public:
	 /**
	  * @brief Creates a new LinuxThread
	  * @return Pointer to LinuxThread object
	  */
	 OSThread * createThread() {
		 return new LinuxThread();
	 }
};

/**
 * @brief Extends OSNetworkInterfaceFactory for LinuxNetworkInterface
 */
class LinuxNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	/**
	 * @brief  Creates a new interface
	 * @param net_iface [out] Network interface. Created internally.
	 * @param label [in] Label to be cast to the interface's name internally
	 * @param timestamper [in] Pointer to a hardware timestamp object
	 * @return TRUE if no error during interface creation, FALSE otherwise
	 */
	virtual bool createInterface
	( OSNetworkInterface **net_iface, InterfaceLabel *label,
	  HWTimestamper *timestamper );
};

/**
 * @brief Extends IPC ARG generic interface to linux
 */
class LinuxIPCArg : public OS_IPC_ARG {
private:
	char *group_name;
public:
	/**
	 * @brief  Initializes IPCArg object
	 * @param group_name [in] Group's name
	 */
	LinuxIPCArg( char *group_name ) {
		int len = strnlen(group_name,16);
		this->group_name = new char[len+1];
		strncpy( this->group_name, group_name, len+1 );
		this->group_name[len] = '\0';
	}
	/**
	 * @brief Destroys IPCArg internal variables
	 */
	virtual ~LinuxIPCArg() {
		delete group_name;
	}
	friend class LinuxSharedMemoryIPC;
};

#define DEFAULT_GROUPNAME "ptp"		/*!< Default groupname for the shared memory interface*/

/**
 * @brief Linux shared memory interface
 */
class LinuxSharedMemoryIPC:public OS_IPC {
private:
	int shm_fd;
	char *master_offset_buffer;
	int err;
public:
	/**
	 * @brief Initializes the internal flags
	 */
	LinuxSharedMemoryIPC() {
		shm_fd = 0;
		err = 0;
		master_offset_buffer = NULL;
	};
	/**
	 * @brief Destroys and unlinks shared memory
	 */
	~LinuxSharedMemoryIPC();

	/**
	 * @brief  Initializes shared memory with DEFAULT_GROUPNAME case arg is null
	 * @param  barg Groupname of the shared memory
	 * @return TRUE if no error, FALSE otherwise
	 */
	virtual bool init( OS_IPC_ARG *barg = NULL );

	/**
	 * @brief Updates IPC values
	 * @param ml_phoffset Master to local phase offset
	 * @param ls_phoffset Local to slave phase offset
	 * @param ml_freqoffset Master to local frequency offset
	 * @param ls_freqoffset Local to slave frequency offset
	 * @param local_time Local time
	 * @param sync_count Count of syncs
	 * @param pdelay_count Count of pdelays
	 * @param port_state Port's state
	 * @param asCapable asCapable flag
	 * @return TRUE
	 */
	virtual bool update
	(int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset,
	 FrequencyRatio ls_freqoffset, uint64_t local_time, uint32_t sync_count,
	 uint32_t pdelay_count, PortState port_state, bool asCapable );

	/**
	 * @brief unmaps and unlink shared memory
	 * @return void
	 */
	void stop();
};


#endif/*LINUX_HAL_COMMON_HPP*/
