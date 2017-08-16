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

#include <linux_hal_common.hpp>
#include <sys/types.h>
#include <avbts_clock.hpp>
#include <avbts_port.hpp>

#include <pthread.h>
#include <linux_ipc.hpp>

#include <sys/mman.h>
#include <fcntl.h>
#include <grp.h>
#include <net/if.h>

#include <unistd.h>
#include <errno.h>

#include <signal.h>
#include <net/ethernet.h> /* the L2 protocols */

#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>

#include <fstream>

Timestamp tsToTimestamp(struct timespec *ts)
{
	Timestamp ret;
	int seclen = sizeof(ts->tv_sec) - sizeof(ret.seconds_ls);
	if (seclen > 0) {
		ret.seconds_ms =
		    ts->tv_sec >> (sizeof(ts->tv_sec) - seclen) * 8;
		ret.seconds_ls = ts->tv_sec & 0xFFFFFFFF;
	} else {
		ret.seconds_ms = 0;
		ret.seconds_ls = ts->tv_sec;
	}
	ret.nanoseconds = ts->tv_nsec;
	return ret;
}

LinuxNetworkInterface::~LinuxNetworkInterface() {
	close( sd_event );
	close( sd_general );
}

net_result LinuxNetworkInterface::send
( LinkLayerAddress *addr, uint16_t etherType, uint8_t *payload, size_t length, bool timestamp ) {
	struct sockaddr_in remoteIpv4;
	struct sockaddr_in6 remoteIpv6;
	sockaddr *remote;
	size_t remoteSize;
	int err;
#ifdef APTP
	if (4 == addr->IpVersion())
	{
		memset(&remoteIpv4, 0, sizeof(remoteIpv4));
		remoteIpv4.sin_family = AF_INET;
		remoteIpv4.sin_port = htons(addr->Port());
		addr->getAddress(&remoteIpv4.sin_addr);
		remote = reinterpret_cast<sockaddr*>(&remoteIpv4);
		remoteSize = sizeof(remoteIpv4);
	}
	else if (6 == addr->IpVersion())
	{
		memset(&remoteIpv6, 0, sizeof(remoteIpv6));
		remoteIpv6.sin6_family = AF_INET6;
		remoteIpv6.sin6_port = htons(addr->Port());
		addr->getAddress(&remoteIpv6.sin6_addr);
		remote = reinterpret_cast<sockaddr*>(&remoteIpv6);
		remoteSize = sizeof(remoteIpv6);
	}
#else
	sockaddr_ll remoteOrig;
	memset(&remoteOrig, 0, sizeof(remoteOrig));
	remoteOrig.sll_family = AF_PACKET;
	remoteOrig.sll_protocol = PLAT_htons(etherType);
	remoteOrig.sll_ifindex = ifindex;
	remoteOrig.sll_halen = ETH_ALEN;
	addr->toOctetArray(remoteOrig.sll_addr);
	remote = reinterpret_cast<sockaddr*>(&remoteOrig);
	remoteSize = sizeof(remoteOrig);
#endif	

	if (timestamp) 
	{
#ifndef ARCH_INTELCE
#ifndef APTP
		net_lock.lock();
#endif
#endif
		GPTP_LOG_VERBOSE("sendto  sd_event");
		err = sendto(sd_event, payload, length, 0, remote, remoteSize);
	} 
	else
	{
		GPTP_LOG_VERBOSE("sendto  sd_general");
		err = sendto(sd_general, payload, length, 0, remote, remoteSize);
  	}
	
	
	if (err == -1)
	{
		GPTP_LOG_ERROR( "Failed to send: %s(%d)", strerror(errno), errno );
		return net_fatal;
	}
	return net_succeed;
}


void LinuxNetworkInterface::disable_rx_queue() {
	if (!net_lock.lock())
	{
		fprintf( stderr, "D rx lock failed\n" );
		_exit(0);
	}
}

void LinuxNetworkInterface::clear_reenable_rx_queue() {
	char buf[256];

	while( recvfrom( sd_event, buf, 256, MSG_DONTWAIT, NULL, 0 ) != -1 );

	if( !net_lock.unlock() ) {
		fprintf( stderr, "D failed unlock rx lock\n");
	}
}

static void x_readEvent(int sockint, IEEE1588Port *pPort, int ifindex)
{
	int status;
	char buf[4096];
	struct iovec iov = { buf, sizeof buf };
	struct sockaddr_nl snl;
	struct msghdr msg = { (void *) &snl, sizeof snl, &iov, 1, NULL, 0, 0 };
	struct nlmsghdr *msgHdr;
	struct ifinfomsg *ifi;

	status = recvmsg(sockint, &msg, 0);

	if (status < 0) {
		GPTP_LOG_ERROR("read_netlink: Error recvmsg: %d", status);
		return;
	}

	if (status == 0) {
		GPTP_LOG_ERROR("read_netlink: EOF");
		return;
	}

	// Process the NETLINK messages
	for (msgHdr = (struct nlmsghdr *)buf; NLMSG_OK(msgHdr, (unsigned int)status); msgHdr = NLMSG_NEXT(msgHdr, status))
	{
		if (msgHdr->nlmsg_type == NLMSG_DONE)
			return;

		if (msgHdr->nlmsg_type == NLMSG_ERROR) {
			GPTP_LOG_ERROR("netlink message error");
			return;
		}

		if (msgHdr->nlmsg_type == RTM_NEWLINK) {
			ifi = (struct ifinfomsg *)NLMSG_DATA(msgHdr);
			if (ifi->ifi_index == ifindex) {
				if ((ifi->ifi_flags & IFF_RUNNING)) {
					pPort->processEvent(LINKUP);
				}
				else {
					pPort->processEvent(LINKDOWN);
				}
			}
		}
	}
	return;
}

void LinuxNetworkInterface::watchNetLink(IEEE1588Port *pPort)
{
	fd_set netLinkFD;
	int netLinkSocket;

	struct sockaddr_nl addr;

	netLinkSocket = socket (AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (netLinkSocket < 0) {
		GPTP_LOG_ERROR("NETLINK socket open error");
		return;
	}

	memset((void *) &addr, 0, sizeof (addr));

	addr.nl_family = AF_NETLINK;
	addr.nl_pid = getpid ();
	addr.nl_groups = RTMGRP_LINK;

	if (bind (netLinkSocket, (struct sockaddr *) &addr, sizeof (addr)) < 0) {
		GPTP_LOG_ERROR("Socket bind failed");
		return;
	}

	while (1) {
		FD_ZERO(&netLinkFD);
		FD_CLR(netLinkSocket, &netLinkFD);
		FD_SET(netLinkSocket, &netLinkFD);

		// Wait forever for a net link event
		int retval = select(FD_SETSIZE, &netLinkFD, NULL, NULL, NULL);
		if (retval == -1)
			; // Error on select. We will ignore and keep going
		else if (retval) {
			x_readEvent(netLinkSocket, pPort, ifindex);
		}
		else {
			; // Would be timeout but Won't happen because we wait forever
		}
	}
}

struct LinuxTimerQueuePrivate {
	pthread_t signal_thread;
};

struct LinuxTimerQueueActionArg {
	timer_t timer_handle;
	struct sigevent sevp;
	event_descriptor_t *inner_arg;
	ostimerq_handler func;
	int type;
	bool rm;
};

LinuxTimerQueue::~LinuxTimerQueue() {
	pthread_join(_private->signal_thread,NULL);
	if( _private != NULL ) delete _private;

	// Deallocate the timer argument wrappers!!!!
}

bool LinuxTimerQueue::init() {
	_private = new LinuxTimerQueuePrivate;
	if( _private == NULL ) return false;

	return true;
}

void *LinuxTimerQueueHandler( void *arg ) {
	LinuxTimerQueue *timerq = (LinuxTimerQueue *) arg;
	sigset_t waitfor;
	struct timespec timeout;
	timeout.tv_sec = 0; timeout.tv_nsec = 100000000; /* 100 ms */

	sigemptyset( &waitfor );

	while( !timerq->stop ) {
		siginfo_t info;
		LinuxTimerQueueMap_t::iterator iter;
		sigaddset( &waitfor, SIGUSR1 );
		if( sigtimedwait( &waitfor, &info, &timeout ) == -1 ) {
			if( errno == EAGAIN ) continue;
			else break;
		}
		if( timerq->lock->lock() != oslock_ok ) {
			break;
		}

		iter = timerq->timerQueueMap.find(info.si_value.sival_int);
		if( iter != timerq->timerQueueMap.end() ) {
		    struct LinuxTimerQueueActionArg *arg = iter->second;
			timerq->timerQueueMap.erase(iter);
			timerq->LinuxTimerQueueAction( arg );
			if( arg->rm ) {
				delete arg->inner_arg;
			}
			timer_delete(arg->timer_handle);
			delete arg;
		}
		if( timerq->lock->unlock() != oslock_ok ) {
			break;
		}
	}

	return NULL;
}

void LinuxTimerQueue::LinuxTimerQueueAction( LinuxTimerQueueActionArg *arg ) {
	arg->func( arg->inner_arg );

    return;
}

OSTimerQueue *LinuxTimerQueueFactory::createOSTimerQueue
	( IEEE1588Clock *clock ) {
	LinuxTimerQueue *ret = new LinuxTimerQueue();

	if( !ret->init() ) {
		return NULL;
	}

	if( pthread_create
		( &(ret->_private->signal_thread),
		  NULL, LinuxTimerQueueHandler, ret ) != 0 ) {
		delete ret;
		return NULL;
	}

	ret->stop = false;
	ret->key = 0;
	ret->lock = clock->timerQLock();

	return ret;
}



bool LinuxTimerQueue::addEvent(unsigned long micros, int type, 
 ostimerq_handler func, event_descriptor_t * arg, bool rm, unsigned *event)
{
	LinuxTimerQueueActionArg *outer_arg;
	int err;
	LinuxTimerQueueMap_t::iterator iter;


	outer_arg = new LinuxTimerQueueActionArg;
	outer_arg->inner_arg = arg;
	outer_arg->rm = rm;
	outer_arg->func = func;
	outer_arg->type = type;

	// !!! Experimental for reducing the number of per process timers
	// // Find key that we can use
	// auto lowerBound = timerQueueMap.lower_bound(type);
	// if (lowerBound != timerQueueMap.end() && 
	//  !timerQueueMap.key_comp()(type, lowerBound->first))
	// {
	// 	// Update the timer
	// 	lb->second = outer_arg
	// }
	// else
	// {
	// 	// Create a new timer
	// 	timerQueueMap.insert(lb, LinuxTimerQueueMap_t::value_type(type, outer_arg));
	// }

	while( timerQueueMap.find( key ) != timerQueueMap.end() ) {
		++key;
	}

	{
		struct itimerspec its;
		memset(&(outer_arg->sevp), 0, sizeof(outer_arg->sevp));
		outer_arg->sevp.sigev_notify = SIGEV_SIGNAL;
		outer_arg->sevp.sigev_signo  = SIGUSR1;
		outer_arg->sevp.sigev_value.sival_int = key;
		if ( timer_create
			 (CLOCK_MONOTONIC, &outer_arg->sevp, &outer_arg->timer_handle)
			 == -1) {
			GPTP_LOG_ERROR("timer_create failed - %s", strerror(errno));
			return false;
		}
		timerQueueMap[key] = outer_arg;

		memset(&its, 0, sizeof(its));
		its.it_value.tv_sec = micros / 1000000;
		its.it_value.tv_nsec = (micros % 1000000) * 1000;
		err = timer_settime( outer_arg->timer_handle, 0, &its, NULL );
		if( err < 0 ) {
			fprintf
				( stderr, "Failed to arm timer: %s\n",
				  strerror( errno ));
			return false;
		}
	}

	return true;
}


bool LinuxTimerQueue::cancelEvent( int type, unsigned *event ) {
	LinuxTimerQueueMap_t::iterator iter;
	for( iter = timerQueueMap.begin(); iter != timerQueueMap.end();) {
		if( (iter->second)->type == type ) {
			// Delete element
			if( (iter->second)->rm ) {
				delete (iter->second)->inner_arg;
			}
			timer_delete(iter->second->timer_handle);
			delete iter->second;
			timerQueueMap.erase(iter++);
		} else {
			++iter;
		}
	}

	return true;
}


void* OSThreadCallback( void* input ) {
    OSThreadArg *arg = (OSThreadArg*) input;

    arg->ret = arg->func( arg->arg );
    return 0;
}

bool LinuxTimestamper::post_init( int ifindex, int sd, TicketingLock *lock ) {
	return true;
}

LinuxTimestamper::~LinuxTimestamper() {}

unsigned long LinuxTimer::sleep(unsigned long micros) {
	struct timespec req = { 0, (long int)(micros * 1000) };
	struct timespec rem;
	int ret = nanosleep( &req, &rem );
	while( ret == -1 && errno == EINTR ) {
		req = rem;
		ret = nanosleep( &req, &rem );
	}
	if( ret == -1 ) {
		fprintf
			( stderr, "Error calling nanosleep: %s\n", strerror( errno ));
		_exit(-1);
	}
	return micros;
}

struct TicketingLockPrivate {
	pthread_cond_t condition;
	pthread_mutex_t cond_lock;
};

bool TicketingLock::lock( bool *got ) {
	uint8_t ticket;
	bool yield = false;
	bool ret = true;
	if( !init_flag ) return false;

	if( pthread_mutex_lock( &_private->cond_lock ) != 0 ) {
		GPTP_LOG_VERBOSE("TicketingLock::lock Not LOCKed");
		ret = false;	
		goto done;
	}
	// Take a ticket
	ticket = cond_ticket_issue++;
	while( ticket != cond_ticket_serving ) {
		if( got != NULL ) {			
			*got = false;
			--cond_ticket_issue;
			yield = true;
			goto unlock;
		}
		if( pthread_cond_wait( &_private->condition, &_private->cond_lock ) != 0 ) {
			GPTP_LOG_VERBOSE("TicketingLock::lock Waiting");
			ret = false;
			goto unlock;
		}
	}

	if( got != NULL )
	{
		*got = true;
		//GPTP_LOG_VERBOSE("TicketingLock::lock LOCKED!!!  ticket:%d  cond_ticket_serving:%d", ticket, cond_ticket_serving);
	}

 unlock:
	if( pthread_mutex_unlock( &_private->cond_lock ) != 0 ) {
		GPTP_LOG_VERBOSE("TicketingLock::lock UNLOCKed");
		ret = false;
		goto done;
	}

	if( yield ) pthread_yield();

 done:
	return ret;
}

bool TicketingLock::unlock() {
	bool ret = true;
	if( !init_flag ) return false;
    //GPTP_LOG_VERBOSE("TicketingLock::unlock");
	if( pthread_mutex_lock( &_private->cond_lock ) != 0 ) {
		GPTP_LOG_VERBOSE("TicketingLock::unlock LOCKed");
		ret = false;
		goto done;
	}
	++cond_ticket_serving;
	//GPTP_LOG_VERBOSE("TicketingLock::unlock CALLED  cond_ticket_serving:%d", cond_ticket_serving);
	if( pthread_cond_broadcast( &_private->condition ) != 0 ) {
		GPTP_LOG_VERBOSE("TicketingLock::unlock Cond Broadcast");
		ret = false;
		goto unlock;
	}

 unlock:
	if( pthread_mutex_unlock( &_private->cond_lock ) != 0 ) {
		GPTP_LOG_VERBOSE("TicketingLock::unlock UNLOCKed");
		ret = false;
		goto done;
	}

 done:
	return ret;
}

bool TicketingLock::init() {
	int err;
	if( init_flag ) return false;  // Don't do this more than once
	_private = new TicketingLockPrivate;
	if( _private == NULL ) return false;
	err = pthread_mutex_init( &_private->cond_lock, NULL );
	if( err != 0 ) return false;
	err = pthread_cond_init( &_private->condition, NULL );
	if( err != 0 ) return false;
	in_use = false;
	cond_ticket_issue = 0;
	cond_ticket_serving = 0;
	init_flag = true;

	return true;
}

TicketingLock::TicketingLock() {
	init_flag = false;
	_private = NULL;
	cond_ticket_issue = 0;
	cond_ticket_serving = 0;
}

TicketingLock::~TicketingLock() {
	if( _private != NULL ) delete _private;
}

struct LinuxLockPrivate {
    pthread_t thread_id;
	pthread_mutexattr_t mta;
	pthread_mutex_t mutex;
	pthread_cond_t port_ready_signal;
};

bool LinuxLock::initialize( OSLockType type ) {
	int lock_c;

	_private = new LinuxLockPrivate;
	if( _private == NULL ) return false;

	pthread_mutexattr_init(&_private->mta);
	if( type == oslock_recursive )
		pthread_mutexattr_settype(&_private->mta, PTHREAD_MUTEX_RECURSIVE);
	lock_c = pthread_mutex_init(&_private->mutex,&_private->mta);
	if(lock_c != 0) {
		GPTP_LOG_ERROR("Mutex initialization failed - %s",strerror(errno));
		return oslock_fail;
	}
	return oslock_ok;
}

LinuxLock::~LinuxLock() {
	int lock_c = pthread_mutex_lock(&_private->mutex);
	if(lock_c == 0) {
		pthread_mutex_destroy( &_private->mutex );
	}
}

OSLockResult LinuxLock::lock() {
	int lock_c;
	lock_c = pthread_mutex_lock(&_private->mutex);
	if(lock_c != 0) {
		fprintf( stderr, "LinuxLock: lock failed %d\n", lock_c );
		return oslock_fail;
	}
	//GPTP_LOG_VERBOSE("LinuxLock::lock   LOCKED");
	return oslock_ok;
}

OSLockResult LinuxLock::trylock() {
	int lock_c;
	lock_c = pthread_mutex_trylock(&_private->mutex);
	if(lock_c != 0)
	{
		GPTP_LOG_VERBOSE("LinuxLock::trylock   Try lock FAILED");
		return oslock_fail;
	}
	//GPTP_LOG_VERBOSE("LinuxLock::trylock   Try LOCK");
	return oslock_ok;
}

OSLockResult LinuxLock::unlock() {
	int lock_c;
	lock_c = pthread_mutex_unlock(&_private->mutex);
	if(lock_c != 0) {
		fprintf( stderr, "LinuxLock: unlock failed %d\n", lock_c );
		return oslock_fail;
	}
	//GPTP_LOG_VERBOSE("LinuxLock::unlock   UnLOCKED");
	return oslock_ok;
}

struct LinuxConditionPrivate {
	pthread_cond_t port_ready_signal;
	pthread_mutex_t port_lock;
};


LinuxCondition::~LinuxCondition() {
	if( _private != NULL ) delete _private;
}

bool LinuxCondition::initialize() {
	int lock_c;

	_private = new LinuxConditionPrivate;
	if( _private == NULL ) return false;

	pthread_cond_init(&_private->port_ready_signal, NULL);
	lock_c = pthread_mutex_init(&_private->port_lock, NULL);
	if (lock_c != 0)
		return false;
	return true;
}

bool LinuxCondition::wait_prelock() {
	pthread_mutex_lock(&_private->port_lock);
	up();
	//GPTP_LOG_VERBOSE("LinuxCondition::wait_prelock   LOCKED");
	return true;
}

bool LinuxCondition::wait() {
	pthread_cond_wait(&_private->port_ready_signal, &_private->port_lock);
	//GPTP_LOG_VERBOSE("LinuxCondition::wait   WAIT");
	down();
	pthread_mutex_unlock(&_private->port_lock);
	//GPTP_LOG_VERBOSE("LinuxCondition::wait   UNLOCK");
	return true;
}

bool LinuxCondition::signal() {
	pthread_mutex_lock(&_private->port_lock);
	if (waiting())
	{
		pthread_cond_broadcast(&_private->port_ready_signal);
		//GPTP_LOG_VERBOSE("LinuxCondition::signal   BROADCAST");
	}
	pthread_mutex_unlock(&_private->port_lock);
	return true;
}

struct LinuxThreadPrivate {
	pthread_t thread_id;
};

bool LinuxThread::start(OSThreadFunction function, void *arg) {
	sigset_t set;
	sigset_t oset;
	int err;

	_private = new LinuxThreadPrivate;
	if( _private == NULL ) return false;

	arg_inner = new OSThreadArg();
	arg_inner->func = function;
	arg_inner->arg = arg;
	sigemptyset(&set);
	sigaddset(&set, SIGALRM);
	err = pthread_sigmask(SIG_BLOCK, &set, &oset);
	if (err != 0) {
		GPTP_LOG_ERROR
			("Add timer pthread_sigmask( SIG_BLOCK ... )");
		return false;
	}
	err = pthread_create(&_private->thread_id, NULL, OSThreadCallback,
						 arg_inner);
	if (err != 0)
		return false;
	sigdelset(&oset, SIGALRM);
	err = pthread_sigmask(SIG_SETMASK, &oset, NULL);
	if (err != 0) {
		GPTP_LOG_ERROR
			("Add timer pthread_sigmask( SIG_SETMASK ... )");
		return false;
	}

	return true;
}

bool LinuxThread::join(OSThreadExitCode & exit_code) {
	int err;
	err = pthread_join(_private->thread_id, NULL);
	if (err != 0)
		return false;
	exit_code = arg_inner->ret;
	delete arg_inner;
	return true;
}

LinuxThread::LinuxThread() {
	_private = NULL;
};

LinuxThread::~LinuxThread() 
{
	delete _private;
}

LinuxSharedMemoryIPC::~LinuxSharedMemoryIPC() {
	munmap(master_offset_buffer, SHM_SIZE);
	shm_unlink(SHM_NAME);
}

bool LinuxSharedMemoryIPC::init( OS_IPC_ARG *barg ) {
	LinuxIPCArg *arg;
	struct group *grp;
	const char *group_name;
	pthread_mutexattr_t shared;
	mode_t oldumask = umask(0);

	if( barg == NULL ) {
		group_name = DEFAULT_GROUPNAME;
	} else {
		arg = dynamic_cast<LinuxIPCArg *> (barg);
		if( arg == NULL ) {
			GPTP_LOG_ERROR( "Wrong IPC init arg type" );
			goto exit_error;
		} else {
			group_name = arg->group_name;
		}
	}
	grp = getgrnam( group_name );
	if( grp == NULL ) {
		GPTP_LOG_ERROR( "Group %s not found, will try root (0) instead", group_name );
	}

	shm_fd = shm_open( SHM_NAME, O_RDWR | O_CREAT, 0660 );
	if( shm_fd == -1 ) {
		GPTP_LOG_ERROR( "shm_open(): %s", strerror(errno) );
		goto exit_error;
	}
	(void) umask(oldumask);
	if (fchown(shm_fd, -1, grp != NULL ? grp->gr_gid : 0) < 0) {
		GPTP_LOG_ERROR("shm_open(): Failed to set ownership");
	}
	if( ftruncate( shm_fd, SHM_SIZE ) == -1 ) {
		GPTP_LOG_ERROR( "ftruncate()" );
		goto exit_unlink;
	}
	master_offset_buffer = (char *) mmap(NULL, SHM_SIZE,
	 PROT_READ | PROT_WRITE, MAP_LOCKED | MAP_SHARED, shm_fd, 0);
	if (master_offset_buffer == (char *)-1) 
	{
		GPTP_LOG_ERROR( "mmap()" );
		goto exit_unlink;
	}
	/*create mutex attr */
	err = pthread_mutexattr_init(&shared);
	if(err != 0) {
		GPTP_LOG_ERROR
			("mutex attr initialization failed - %s",
			 strerror(errno));
		goto exit_unlink;
	}
	pthread_mutexattr_setpshared(&shared,1);
	/*create a mutex */
	err = pthread_mutex_init((pthread_mutex_t *) master_offset_buffer, &shared);
	if(err != 0) {
		GPTP_LOG_ERROR
			("sharedmem - Mutex initialization failed - %s",
			 strerror(errno));
		goto exit_unlink;
	}
	return true;
 exit_unlink:
	shm_unlink( SHM_NAME );
 exit_error:
	return false;
}

bool LinuxSharedMemoryIPC::update
(int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset,
 FrequencyRatio ls_freqoffset, uint64_t local_time, uint32_t sync_count,
 uint32_t pdelay_count, PortState port_state, bool asCapable,
 const std::string& adrRegSocketIp, uint16_t adrRegSocketPort)
{
	int buf_offset = 0;
	pid_t process_id = getpid();
	char *shm_buffer = master_offset_buffer;
	gPtpTimeData *ptimedata;
	if( shm_buffer != NULL ) {
		/* lock */
		pthread_mutex_lock((pthread_mutex_t *) shm_buffer);
		buf_offset += sizeof(pthread_mutex_t);
		ptimedata   = (gPtpTimeData *) (shm_buffer + buf_offset);
		ptimedata->ml_phoffset = ml_phoffset;
		ptimedata->ls_phoffset = ls_phoffset;
		ptimedata->ml_freqoffset = ml_freqoffset;
		ptimedata->ls_freqoffset = ls_freqoffset;
		ptimedata->local_time = local_time;
		ptimedata->sync_count   = sync_count;
		ptimedata->pdelay_count = pdelay_count;
      ptimedata->asCapable = asCapable;
		ptimedata->port_state   = port_state;
		ptimedata->process_id   = process_id;
      ptimedata->addressRegistrationSocketPort = adrRegSocketPort;

		/* unlock */
		pthread_mutex_unlock((pthread_mutex_t *) shm_buffer);
	}
	return true;
}

void LinuxSharedMemoryIPC::stop() {
	if (master_offset_buffer != NULL)
	{
		munmap(master_offset_buffer, SHM_SIZE);
		shm_unlink( SHM_NAME );
	}
}

bool LinuxNetworkInterfaceFactory::createInterface(OSNetworkInterface **net_iface,
 InterfaceLabel *label, HWTimestamper *timestamper, int ipVersion)
{
	struct ifreq device;
	int err;
	LinkLayerAddress addr;
	int ifindex;

	LinuxNetworkInterface *net_iface_l = new LinuxNetworkInterface();

	net_iface_l->IpVersion(ipVersion);

	if( !net_iface_l->net_lock.init()) {
		GPTP_LOG_ERROR( "Failed to initialize network lock");
		return false;
	}

	if (!net_iface_l->fNetLockEvent.init())
	{
		GPTP_LOG_ERROR( "Failed to initialize event network lock");
		return false;
	}

	if (!net_iface_l->fNetLockGeneral.init())
	{
		GPTP_LOG_ERROR( "Failed to initialize general network lock");
		return false;
	}

	InterfaceName *ifname = dynamic_cast<InterfaceName *>(label);
	if( ifname == NULL ){
		GPTP_LOG_ERROR( "ifname == NULL");
		return false;
	}


	int domain = 4 == ipVersion ? AF_INET : AF_INET6;
#ifdef APTP
	net_iface_l->sd_general = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
#else
	net_iface_l->sd_general = socket( PF_PACKET, SOCK_DGRAM, 0 );	
#endif
	if( net_iface_l->sd_general == -1 ) {
		GPTP_LOG_ERROR( "failed to open general socket: %s", strerror(errno));
		return false;
	}
#ifdef APTP
	net_iface_l->sd_event = socket(domain, SOCK_DGRAM, IPPROTO_UDP);
#else
	net_iface_l->sd_event = socket( PF_PACKET, SOCK_DGRAM, 0 );
#endif

	GPTP_LOG_VERBOSE("createInterface  net_iface_l->sd_event: %d", net_iface_l->sd_event);
	if( net_iface_l->sd_event == -1 ) {
		GPTP_LOG_ERROR
			( "failed to open event socket: %s ", strerror(errno));
		return false;
	}

	memset( &device, 0, sizeof(device));
	ifname->toString( device.ifr_name, IFNAMSIZ - 1 );
	
	GPTP_LOG_VERBOSE("device.ifr_name: %s", device.ifr_name);

	err = ioctl( net_iface_l->sd_event, SIOCGIFHWADDR, &device );
	if( err == -1 ) {
		GPTP_LOG_ERROR
			( "Failed to get interface address: %s", strerror( errno ));
		return false;
	}

	GPTP_LOG_VERBOSE("device.ifr_hwaddr.sa_data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", device.ifr_hwaddr.sa_data[0],
		device.ifr_hwaddr.sa_data[1],device.ifr_hwaddr.sa_data[2],device.ifr_hwaddr.sa_data[3],device.ifr_hwaddr.sa_data[4],
		device.ifr_hwaddr.sa_data[5],device.ifr_hwaddr.sa_data[6],device.ifr_hwaddr.sa_data[7],device.ifr_hwaddr.sa_data[8],
		device.ifr_hwaddr.sa_data[9],device.ifr_hwaddr.sa_data[10],device.ifr_hwaddr.sa_data[11],device.ifr_hwaddr.sa_data[12],
		device.ifr_hwaddr.sa_data[13]);


	addr = LinkLayerAddress( (uint8_t *)&device.ifr_hwaddr.sa_data );
	net_iface_l->local_addr = addr;
	err = ioctl( net_iface_l->sd_event, SIOCGIFINDEX, &device );
	if( err == -1 ) {
		GPTP_LOG_ERROR("Failed to get interface index: %s", strerror( errno ));
		return false;
	}
	ifindex = device.ifr_ifindex;
	net_iface_l->ifindex = ifindex;

	GPTP_LOG_VERBOSE("LinuxNetworkInterfaceFactory::createInterface  ifindex: %d", ifindex);

	struct sockaddr_in evntIpv4;
	struct sockaddr_in6 evntIpv6;
	sockaddr *evntAddr;
	size_t evntAddrSize;
	struct sockaddr_in genIpv4;
	struct sockaddr_in6 genIpv6;
	sockaddr *genAddr;
	size_t genAddrSize;

#if APTP
	if (4 == ipVersion)
	{
		memset(&evntIpv4, 0, sizeof(evntIpv4));
		evntIpv4.sin_port = htons(EVENT_PORT);
		evntIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
		evntIpv4.sin_family = AF_INET;
		evntAddrSize = sizeof(evntIpv4);
		evntAddr = reinterpret_cast<sockaddr*>(&evntIpv4);
		memset(&genIpv4, 0, sizeof(genIpv4));
		genIpv4.sin_port = htons(GENERAL_PORT);
		genIpv4.sin_addr.s_addr = htonl(INADDR_ANY);
		genIpv4.sin_family = AF_INET;
		genAddrSize = sizeof(genIpv4);
		genAddr = reinterpret_cast<sockaddr*>(&genIpv4);
	}
	else if (6 == ipVersion)
	{
		memset(&evntIpv6, 0, sizeof(evntIpv6));
		evntIpv6.sin6_port = htons(EVENT_PORT);
		evntIpv6.sin6_family = AF_INET6;
		evntIpv6.sin6_addr = in6addr_any;
		evntAddrSize = sizeof(evntIpv6);
		evntAddr = reinterpret_cast<sockaddr*>(&evntIpv6);
		memset(&genIpv6, 0, sizeof(evntIpv6));
		genIpv6.sin6_port = htons(GENERAL_PORT);
		genIpv6.sin6_family = AF_INET6;
		genIpv6.sin6_addr = in6addr_any;
		genAddrSize = sizeof(genIpv6);
		genAddr = reinterpret_cast<sockaddr*>(&genIpv6);
	}
	else
	{
		GPTP_LOG_ERROR("Invalid ip version %d not supported.", ipVersion);
		return false;
	}
#else
	struct packet_mreq mr_8021as;
	memset( &mr_8021as, 0, sizeof( mr_8021as ));
	mr_8021as.mr_ifindex = ifindex;
	mr_8021as.mr_type = PACKET_MR_MULTICAST;
	mr_8021as.mr_alen = 6;
	memcpy( mr_8021as.mr_address, P8021AS_MULTICAST, mr_8021as.mr_alen );
	err = setsockopt
		( net_iface_l->sd_event, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
		  &mr_8021as, sizeof( mr_8021as ));
	if( err == -1 ) {
		GPTP_LOG_ERROR
			( "Unable to add PTP multicast addresses to port id: %u",
			  ifindex );
		return false;
	}

	struct sockaddr_ll ifsock_addr;
	memset( &ifsock_addr, 0, sizeof( ifsock_addr ));
	ifsock_addr.sll_family = AF_PACKET;
	ifsock_addr.sll_ifindex = ifindex;
	ifsock_addr.sll_protocol = PLAT_htons( PTP_ETHERTYPE );	
	evntAddrSize = sizeof(ifsock_addr);
	evntAddr = reinterpret_cast<sockaddr*>(&ifsock_addr);
	genAddrSize = sizeof(ifsock_addr);
	genAddr = reinterpret_cast<sockaddr*>(&ifsock_addr);
#endif

	GPTP_LOG_VERBOSE("LinuxNetworkInterfaceFactory::createInterface  ipVersion: %d", ipVersion);

	err = bind(net_iface_l->sd_event, evntAddr, evntAddrSize);
	if (-1 == err)
	{
		GPTP_LOG_ERROR("Call to bind() for sd_event failed (%d): %s", errno, strerror(errno) );
		return false;
	}

	err = bind(net_iface_l->sd_general, genAddr, genAddrSize);
	if (-1 == err)
	{
		GPTP_LOG_ERROR("Call to bind() for sd_general failed (%d): %s", errno, strerror(errno) );
		return false;
	}

	net_iface_l->timestamper =
		dynamic_cast <LinuxTimestamper *>(timestamper);
	if (nullptr == net_iface_l->timestamper)
	{
		GPTP_LOG_ERROR("timestamper == NULL");
		return false;
	}

	if (!net_iface_l->timestamper->post_init(ifindex, net_iface_l->sd_event,
	 &net_iface_l->net_lock ))
	{
		GPTP_LOG_ERROR("post_init failed\n");
		return false;
	}

	*net_iface = net_iface_l;

	return true;
}

std::string& StripLeading(std::string& value)
{
	using std::string;

   // No need to manipulate empty strings.
   if (value.empty())
   {
      return value;
   }

   // Iterate from the start of the string
   // until we hit the first non-space character
   const string::iterator limit = value.end();
   string::iterator       p     = value.begin();
   for (; p != limit && isspace(*p); ++p)
   { 
   } // Intentionally empty

   // Remove the spaces
   value.erase(value.begin(), p);

   return value;
}

bool LinuxNetworkInterface::IsWireless(const std::string& netInterfaceName) const
{
	// Validate the network interface name
	size_t pos;
	std::ifstream devStream("/proc/net/dev");
	std::string line;
	std::string systemInterface;
	bool isValidInterfaceName = false;
	while (std::getline(devStream, line))
	{
		pos = line.find(":");
		if (pos != std::string::npos)
		{
			systemInterface = line.substr(0, pos);
			if (StripLeading(systemInterface) == netInterfaceName)
			{
				isValidInterfaceName = true;
				break;
			}
		}
	}

	bool isWireless = false;
	if (isValidInterfaceName)
	{
		// Determine if the interface is wired or not
		std::string wirelessInterface;
		std::ifstream wirelessStream("/proc/net/wireless");
		while (std::getline(devStream, line))
		{
			pos = line.find(":");
			if (pos != std::string::npos)
			{
				wirelessInterface = line.substr(0, pos);
				if (StripLeading(wirelessInterface) == netInterfaceName)
				{
					isWireless = true;
					break;
				}
			}
		}
	}
	else
	{
		GPTP_LOG_ERROR("Invalid network interface name.");
		isWireless = false;
	}
	return isWireless;
}

