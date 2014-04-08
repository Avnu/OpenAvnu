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

#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "avbts_osipc.hpp"
#include "ieee1588.hpp"

#include <list>

#define ONE_WAY_PHY_DELAY 400
#define P8021AS_MULTICAST "\x01\x80\xC2\x00\x00\x0E"
#define PTP_DEVICE "/dev/ptpXX"
#define PTP_DEVICE_IDX_OFFS 8
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)       ((~(clockid_t) (fd) << 3) | CLOCKFD)

struct timespec;
extern Timestamp tsToTimestamp(struct timespec *ts);

struct TicketingLockPrivate;
typedef struct TicketingLockPrivate * TicketingLockPrivate_t;
	

class TicketingLock {
public:
	bool lock( bool *got = NULL );
	bool unlock();
	bool init();
	TicketingLock();
	~TicketingLock();
private:
	bool init_flag;
	TicketingLockPrivate_t _private;
	bool in_use;
	uint8_t cond_ticket_issue;
	uint8_t cond_ticket_serving;
};

class LinuxTimestamper : public HWTimestamper {
public:
	virtual ~LinuxTimestamper() = 0;
	virtual bool post_init( int ifindex, int sd, TicketingLock *lock ) = 0;
};

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
	virtual net_result send
	( LinkLayerAddress *addr, uint8_t *payload, size_t length,
	  bool timestamp );

	virtual net_result nrecv
	( LinkLayerAddress *addr, uint8_t *payload, size_t &length );

	void disable_clear_rx_queue();
	void reenable_rx_queue();

	virtual void getLinkLayerAddress( LinkLayerAddress *addr ) {
		*addr = local_addr;
	}

	// No offset needed for Linux SOCK_DGRAM packet
	virtual unsigned getPayloadOffset() {
		return 0;
	}
	~LinuxNetworkInterface();
protected:
	LinuxNetworkInterface() {};
};

typedef std::list<LinuxNetworkInterface *> LinuxNetworkInterfaceList;

struct LinuxLockPrivate;
typedef LinuxLockPrivate * LinuxLockPrivate_t;

class LinuxLock : public OSLock {
    friend class LinuxLockFactory;
private:
    OSLockType type;
	LinuxLockPrivate_t _private;
protected:
    LinuxLock() {
		_private = NULL;
    }
    bool initialize( OSLockType type );
	~LinuxLock();
    OSLockResult lock();
    OSLockResult trylock();
    OSLockResult unlock();
};

class LinuxLockFactory:public OSLockFactory {
public:
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
typedef struct LinuxConditionPrivate * LinuxConditionPrivate_t;

class LinuxCondition : public OSCondition {
	friend class LinuxConditionFactory;
private:
	LinuxConditionPrivate_t _private;
protected:
	bool initialize();
public:
	bool wait_prelock();
	bool wait();
	bool signal();
	~LinuxCondition();
	LinuxCondition() {
		_private = NULL;
	}
};

class LinuxConditionFactory : public OSConditionFactory {
public:
	OSCondition * createCondition() {
		LinuxCondition *result = new LinuxCondition();
		return result->initialize() ? result : NULL;
	}
};

struct LinuxTimerQueueActionArg;

typedef std::map < int, struct LinuxTimerQueueActionArg *> LinuxTimerQueueMap_t;

void *LinuxTimerQueueHandler( void *arg );

struct LinuxTimerQueuePrivate;
typedef struct LinuxTimerQueuePrivate * LinuxTimerQueuePrivate_t;

class LinuxTimerQueue : public OSTimerQueue {
	friend class LinuxTimerQueueFactory;
	friend void *LinuxTimerQueueHandler( void *);
private:
	LinuxTimerQueueMap_t timerQueueMap;
	int key;
	bool stop;
	LinuxTimerQueuePrivate_t _private;
	OSLock *lock;
	void LinuxTimerQueueAction( LinuxTimerQueueActionArg *arg );
protected:
	LinuxTimerQueue() {
		_private = NULL;
	}
	virtual bool init();
public:
	~LinuxTimerQueue();
	bool addEvent
	( unsigned long micros, int type, ostimerq_handler func,
	  event_descriptor_t * arg, bool rm, unsigned *event );
	bool cancelEvent( int type, unsigned *event );
};

class LinuxTimerQueueFactory : public OSTimerQueueFactory {
public:
	virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock );
};



class LinuxTimer : public OSTimer {
	friend class LinuxTimerFactory;
 public:
	virtual unsigned long sleep(unsigned long micros);
 protected:
	LinuxTimer() {};
};

class LinuxTimerFactory : public OSTimerFactory {
 public:
	virtual OSTimer * createTimer() {
		return new LinuxTimer();
	}
};

struct OSThreadArg {
	OSThreadFunction func;
	void *arg;
	OSThreadExitCode ret;
};

void *OSThreadCallback(void *input);

struct LinuxThreadPrivate;
typedef LinuxThreadPrivate * LinuxThreadPrivate_t;

class LinuxThread : public OSThread {
	friend class LinuxThreadFactory;
 private:
	LinuxThreadPrivate_t _private;
	OSThreadArg *arg_inner;
 public:
	virtual bool start(OSThreadFunction function, void *arg);
	virtual bool join(OSThreadExitCode & exit_code);
	virtual ~LinuxThread();
 protected:
	LinuxThread();
};

class LinuxThreadFactory:public OSThreadFactory {
 public:
	OSThread * createThread() {
		return new LinuxThread();
	}
};

class LinuxNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	virtual bool createInterface
	( OSNetworkInterface **net_iface, InterfaceLabel *label,
	  HWTimestamper *timestamper );
};


class LinuxIPCArg : public OS_IPC_ARG {
private:
	char *group_name;
public:
	LinuxIPCArg( char *group_name ) {
		int len = strnlen(group_name,16);
		this->group_name = new char[len+1];
		strncpy( this->group_name, group_name, len+1 );
		this->group_name[len] = '\0';
	}
	virtual ~LinuxIPCArg() {
		delete group_name;
	}
	friend class LinuxSharedMemoryIPC;
};

#define DEFAULT_GROUPNAME "ptp"

class LinuxSharedMemoryIPC:public OS_IPC {
private:
	int shm_fd;
	char *master_offset_buffer;
	int err;
public:
	LinuxSharedMemoryIPC() {
		shm_fd = 0;
		err = 0;
		master_offset_buffer = NULL;
	};
	~LinuxSharedMemoryIPC();
	virtual bool init( OS_IPC_ARG *barg = NULL );
	virtual bool update
	(int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset,
	 FrequencyRatio ls_freqoffset, uint64_t local_time, uint32_t sync_count,
	 uint32_t pdelay_count, PortState port_state );
	void stop();
};


#endif/*LINUX_HAL_COMMON_HPP*/
