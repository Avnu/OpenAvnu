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

#ifndef LINUX_HAL_HPP
#define LINUX_HAL_HPP

#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "avbts_oscondition.hpp"
#include "avbts_ostimerq.hpp"
#include "avbts_ostimer.hpp"
#include "avbts_osthread.hpp"
#include "avbts_osipc.hpp"
#include "ieee1588.hpp"

#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include <string.h>
#include <errno.h>

#include <stdlib.h>

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <netinet/in.h>

#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <sys/select.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ipcdef.hpp>
#include <sys/syscall.h>
#include <math.h>
#include <grp.h>

#include <list>

extern "C" {
#include <igb.h>
}

#define ONE_WAY_PHY_DELAY 400
#define P8021AS_MULTICAST "\x01\x80\xC2\x00\x00\x0E"
#define PTP_DEVICE "/dev/ptpXX"
#define PTP_DEVICE_IDX_OFFS 8
#define CLOCKFD 3
#define FD_TO_CLOCKID(fd)       ((~(clockid_t) (fd) << 3) | CLOCKFD)

static inline Timestamp tsToTimestamp(struct timespec *ts)
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

class LinuxTimestamper;

class TicketingLock {
public:
	bool lock( bool *got = NULL ) {
		uint8_t ticket;
		bool yield = false;
		bool ret = true;
		if( !init_flag ) return false;
		
		if( pthread_mutex_lock( &cond_lock ) != 0 ) {
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
			if( pthread_cond_wait( &condition, &cond_lock ) != 0 ) {
				ret = false;
				goto unlock;
			}
		}

		if( got != NULL ) *got = true;
		
	unlock:
		if( pthread_mutex_unlock( &cond_lock ) != 0 ) {
			ret = false;
			goto done;
		}
		
		if( yield ) pthread_yield();
		
	done:
		return ret;
	}
	bool unlock() {
		bool ret = true;
		if( !init_flag ) return false;
		
		if( pthread_mutex_lock( &cond_lock ) != 0 ) {
			ret = false;
			goto done;
		}
		++cond_ticket_serving;
		if( pthread_cond_broadcast( &condition ) != 0 ) {
			ret = false;
			goto unlock;
		}

	unlock:
		if( pthread_mutex_unlock( &cond_lock ) != 0 ) {
			ret = false;
			goto done;
		}

	done:
		return ret;
	}
	bool init() {
		int err;
		if( init_flag ) return false;  // Don't do this more than once
		err = pthread_mutex_init( &cond_lock, NULL );
		if( err != 0 ) return false;
		err = pthread_cond_init( &condition, NULL );
		if( err != 0 ) return false;
		in_use = false;
		cond_ticket_issue = 0;
		cond_ticket_serving = 0;
		init_flag = true;
    
		return true;
	}
	TicketingLock() {
		init_flag = false;
	}
private:
	bool init_flag;
	pthread_cond_t condition;
	pthread_mutex_t cond_lock;      
	bool in_use;
	uint8_t cond_ticket_issue;
	uint8_t cond_ticket_serving;
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

	virtual net_result recv
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

	virtual ~LinuxNetworkInterface() {
		close( sd_event );
		close( sd_general );
	}

protected:
	LinuxNetworkInterface() {};
};

typedef std::list<LinuxNetworkInterface *> LinuxNetworkInterfaceList;

class LinuxTimestamper : public HWTimestamper {
private:
	int sd;
	Timestamp crstamp_system;
	Timestamp crstamp_device;
	pthread_mutex_t cross_stamp_lock;
	bool cross_stamp_good;
	std::list<Timestamp> rxTimestampList;
	clockid_t clockid;
	LinuxNetworkInterfaceList iface_list;

	TicketingLock *net_lock;
	
	device_t igb_dev;
	bool igb_initd;
	
	int findPhcIndex( InterfaceLabel *iface_label ) {
		int sd;
		InterfaceName *ifname;
		struct ethtool_ts_info info;
		struct ifreq ifr;
		
		if(( ifname = dynamic_cast<InterfaceName *>(iface_label)) == NULL ) {
			fprintf( stderr, "findPTPIndex requires InterfaceName\n" );
			return -1;
		}

		sd = socket( AF_UNIX, SOCK_DGRAM, 0 );
		if( sd < 0 ) {
			fprintf( stderr, "findPTPIndex: failed to open socket\n" );
			return -1;
		}

		memset( &ifr, 0, sizeof(ifr));
		memset( &info, 0, sizeof(info));
		info.cmd = ETHTOOL_GET_TS_INFO;
		ifname->toString( ifr.ifr_name, IFNAMSIZ-1 );
		ifr.ifr_data = (char *) &info;
		
		if( ioctl( sd, SIOCETHTOOL, &ifr ) < 0 ) {
			fprintf( stderr, "findPTPIndex: ioctl(SIOETHTOOL) failed\n" );
			return -1;
		}

		close(sd);

		return info.phc_index;
	}
public:
	LinuxTimestamper() {
		sd = -1;
	}
	virtual bool HWTimestamper_init
	( InterfaceLabel *iface_label, OSNetworkInterface *iface ) {
		int fd;
		pthread_mutex_init( &cross_stamp_lock, NULL );
		cross_stamp_good = false;
		struct timex tx;
		int phc_index;
		char ptp_device[] = PTP_DEVICE;

		// Determine the correct PTP clock interface
		phc_index = findPhcIndex( iface_label );
		if( phc_index < 0 ) {
			fprintf( stderr, "Failed to find PTP device index\n" );
			return false;
		}
		
		snprintf
			( ptp_device+PTP_DEVICE_IDX_OFFS,
			  sizeof(ptp_device)-PTP_DEVICE_IDX_OFFS, "%d", phc_index );
		fprintf( stderr, "Using clock device: %s\n", ptp_device );
		fd = open( ptp_device, O_RDWR );
		if( fd == -1 || (clockid = FD_TO_CLOCKID(fd)) == -1 ) {
			fprintf( stderr, "Failed to open PTP clock device\n" );
			return false;
		}
    
		tx.modes = ADJ_FREQUENCY;
		tx.freq = 0;
		if( syscall(__NR_clock_adjtime, clockid, &tx) != 0 ) {
			fprintf( stderr, "Failed to set PTP clock rate to 0\n" );
			return false;
		}
		
		if( dynamic_cast<LinuxNetworkInterface *>(iface) != NULL ) {
			iface_list.push_front
				( (dynamic_cast<LinuxNetworkInterface *>(iface)) );
		}
		
		return true; 
	}
	void setSocketDescriptor( int sd ) { this->sd = sd; };
	void setNetworkLock( TicketingLock *lock ) { net_lock = lock; };
	void updateCrossStamp( Timestamp *system_time, Timestamp *device_time ) {
		pthread_mutex_lock( &cross_stamp_lock );
		crstamp_system = *system_time;
		crstamp_device  = *device_time;
		cross_stamp_good = true;
		pthread_mutex_unlock( &cross_stamp_lock );
	}
	void pushRXTimestamp( Timestamp *tstamp ) {
		tstamp->_version = version;
		rxTimestampList.push_front(*tstamp);
	}
	bool post_init( int ifindex ) {
		int timestamp_flags = 0;
		struct ifreq device;
		struct hwtstamp_config hwconfig;
		int err;

		memset( &device, 0, sizeof(device));
		device.ifr_ifindex = ifindex;
		err = ioctl( sd, SIOCGIFNAME, &device );
		if( err == -1 ) {
			XPTPD_ERROR
				( "Failed to get interface name: %s", strerror( errno ));
			return false;
		}

		device.ifr_data = (char *) &hwconfig;
		memset( &hwconfig, 0, sizeof( hwconfig ));
		hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
		hwconfig.tx_type = HWTSTAMP_TX_ON;
		err = ioctl( sd, SIOCSHWTSTAMP, &device );
		if( err == -1 ) {
			XPTPD_ERROR
				( "Failed to configure timestamping: %s", strerror( errno ));
			return false;
		}

		timestamp_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;
		err = setsockopt
			( sd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
			  sizeof(timestamp_flags) );
		if( err == -1 ) {
			XPTPD_ERROR
				( "Failed to configure timestamping on socket: %s",
				  strerror( errno ));
			return false;
		}
		
		return true;
	}    

	virtual bool HWTimestamper_gettime
	( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
	  uint32_t *nominal_clock_rate ) {
		bool ret = false;
		pthread_mutex_lock( &cross_stamp_lock );
		if( cross_stamp_good ) {
			*system_time = crstamp_system;
			*device_time = crstamp_device;
			ret = true;
		}
		pthread_mutex_unlock( &cross_stamp_lock );
		
		return ret;
	}

	virtual int HWTimestamper_txtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last ) {
		int err;
		int ret = -72;
		struct msghdr msg;
		struct cmsghdr *cmsg;
		struct sockaddr_ll remote;        
		struct iovec sgentry;
		struct {
			struct cmsghdr cm;
			char control[256];
		} control;
     
		if( sd == -1 ) return -1;
		memset( &msg, 0, sizeof( msg ));
    
		msg.msg_iov = &sgentry;
		msg.msg_iovlen = 1;
		
		sgentry.iov_base = NULL;
		sgentry.iov_len = 0;
		
		memset( &remote, 0, sizeof(remote));
		msg.msg_name = (caddr_t) &remote;
		msg.msg_namelen = sizeof( remote );
		msg.msg_control = &control;
		msg.msg_controllen = sizeof(control);

		err = recvmsg( sd, &msg, MSG_ERRQUEUE );
		if( err == -1 ) {
			if( errno == EAGAIN ) {
				ret = -72;
				goto done;
			}
			else {
				ret = -1;
				goto done;
			}
		}
    
		// Retrieve the timestamp
		cmsg = CMSG_FIRSTHDR(&msg);
		while( cmsg != NULL ) {
			if( cmsg->cmsg_level == SOL_SOCKET &&
				cmsg->cmsg_type == SO_TIMESTAMPING ) {
				struct timespec *ts_device, *ts_system;
				Timestamp device, system; 
				ts_system = ((struct timespec *) CMSG_DATA(cmsg)) + 1;
				system = tsToTimestamp( ts_system );
				ts_device = ts_system + 1; device = tsToTimestamp( ts_device );
				system._version = version;
				device._version = version;
				updateCrossStamp( &system, &device );
				timestamp = device;
				ret = 0;
				break;    
			}
			cmsg = CMSG_NXTHDR(&msg,cmsg);
		}

		if( ret != 0 ) {
			fprintf( stderr, "Received a error message, but didn't find a valid timestamp\n" );
		}
		
	done:
		if( ret == 0 || last ) {
			net_lock->unlock();
		}
		
		return ret;
	}

	virtual int HWTimestamper_rxtimestamp
	( PortIdentity *identity, uint16_t sequenceId, Timestamp &timestamp,
	  unsigned &clock_value, bool last ) {
		/* This shouldn't happen. Ever. */
		if( rxTimestampList.empty() ) return -72;
		timestamp = rxTimestampList.back();
		rxTimestampList.pop_back();
		
		return 0;
	}

#define ADJ_SETOFFSET           0x0100

	virtual bool HWTimestamper_adjclockphase( int64_t phase_adjust ) {
		struct timex tx;
		LinuxNetworkInterfaceList::iterator iface_iter;
		bool ret = true;
		int err;
		
		/* Walk list of interfaces disabling them all */
		iface_iter = iface_list.begin();
		for
			( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
			  ++iface_iter ) {
			(*iface_iter)->disable_clear_rx_queue();
		}
		
		rxTimestampList.clear();
		
		{
			/* Wait 180 ms - This is plenty of time for any time sync frames
			   to clear the queue */
			struct timespec wait_time = { 0, 180000000 };
			err = -1;
			while( err == -1 && wait_time.tv_nsec > 0 ) {
				if
					(( err = nanosleep( &wait_time, &wait_time )) == -1 &&
					 errno != EINTR ) {
					XPTPD_ERROR( "HWTimestamper_adjclockphase: "
								 "nanosleep failed" );
					exit(-1);
				}
			}
		}
		
		++version;
		
		tx.modes = ADJ_SETOFFSET | ADJ_NANO;
		if( phase_adjust >= 0 ) {
			tx.time.tv_sec  = phase_adjust / 1000000000LL;
			tx.time.tv_usec = phase_adjust % 1000000000LL;
		} else {
			tx.time.tv_sec  = (phase_adjust / 1000000000LL)-1;
			tx.time.tv_usec = (phase_adjust % 1000000000LL)+1000000000;
		}
		if( syscall(__NR_clock_adjtime, clockid, &tx) != 0 ) {
			fprintf( stderr, "Call to adjust phase failed, %s(%ld)\n",
					 strerror(errno), tx.time.tv_usec );
			exit(-1);
	  }
		
		// Walk list of interfaces re-enabling them
		iface_iter = iface_list.begin();
	  for( iface_iter = iface_list.begin(); iface_iter != iface_list.end();
		   ++iface_iter ) {
		  (*iface_iter)->reenable_rx_queue();
	  }
	  
	  return ret;
	}
	
	virtual bool HWTimestamper_adjclockrate( float freq_offset ) {
		struct timex tx;
		tx.modes = ADJ_FREQUENCY;
		tx.freq  = long(freq_offset) << 16;
		tx.freq += long(fmodf( freq_offset, 1.0 )*65536.0);
		if( syscall(__NR_clock_adjtime, clockid, &tx) != 0 ) {
			XPTPD_ERROR
				( "Call to adjust frequency failed, %s", strerror(errno) );
			return false;
		}
		return true;
	}
	bool HWTimestamper_PPS_start( );
	bool HWTimestamper_PPS_stop();
};


class LinuxLock : public OSLock {
    friend class LinuxLockFactory;
private:
    OSLockType type;
    pthread_t thread_id;
	pthread_mutexattr_t mta;
	pthread_mutex_t mutex;
	pthread_cond_t port_ready_signal;
protected:
    LinuxLock() {
    }
    bool initialize( OSLockType type ) {
		int lock_c;
		pthread_mutexattr_init(&mta);
		if( type == oslock_recursive )
			pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
		lock_c = pthread_mutex_init(&mutex,&mta);
		if(lock_c != 0) {
			XPTPD_ERROR("Mutex initialization failed - %s\n",strerror(errno));
			return oslock_fail;
		}
		return oslock_ok;
    }
	~LinuxLock() {
		int lock_c = pthread_mutex_lock(&mutex);
		if(lock_c == 0) {
			pthread_mutex_destroy( &mutex );
		}
	}
    OSLockResult lock() {
		int lock_c;
		lock_c = pthread_mutex_lock(&mutex);
		if(lock_c != 0) {
			fprintf( stderr, "LinuxLock: lock failed %d\n", lock_c );  
			return oslock_fail;
		}
		return oslock_ok;
    }
    OSLockResult trylock() {
		int lock_c;
        lock_c = pthread_mutex_trylock(&mutex);
		if(lock_c != 0) return oslock_fail;
		return oslock_ok;
    }
    OSLockResult unlock() {
		int lock_c;
        lock_c = pthread_mutex_unlock(&mutex);
		if(lock_c != 0) {
			fprintf( stderr, "LinuxLock: unlock failed %d\n", lock_c );  
			return oslock_fail;
		}
		return oslock_ok;
    }
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

class LinuxCondition:public OSCondition {
	friend class LinuxConditionFactory;
private:
	pthread_cond_t port_ready_signal;
	pthread_mutex_t port_lock;
protected:
	bool initialize() {
		int lock_c;
		pthread_cond_init(&port_ready_signal, NULL);
		lock_c = pthread_mutex_init(&port_lock, NULL);
		if (lock_c != 0)
			return false;
		return true;
 } 
public:
	bool wait_prelock() {
		pthread_mutex_lock(&port_lock);
		up();
		return true;
	}
	bool wait() {
		pthread_cond_wait(&port_ready_signal, &port_lock);
		down();
		pthread_mutex_unlock(&port_lock);
		return true;
	}
	bool signal() {
		pthread_mutex_lock(&port_lock);
		if (waiting())
			pthread_cond_broadcast(&port_ready_signal);
		pthread_mutex_unlock(&port_lock);
		return true;
	}
};

class LinuxConditionFactory:public OSConditionFactory {
public:
	OSCondition * createCondition() {
		LinuxCondition *result = new LinuxCondition();
		return result->initialize() ? result : NULL;
	}
};

struct LinuxTimerQueueActionArg {
	timer_t timer_handle;
	struct sigevent sevp;
	event_descriptor_t *inner_arg;
	ostimerq_handler func;
	int type;
	bool rm;
};

typedef std::map < int, LinuxTimerQueueActionArg > LinuxTimerQueueMap_t;

void *LinuxTimerQueueHandler( void *arg );

class LinuxTimerQueue : public OSTimerQueue {
	friend class LinuxTimerQueueFactory;
	friend void *LinuxTimerQueueHandler( void *);
private:
	LinuxTimerQueueMap_t timerQueueMap;
	int key;
	bool stop;
	pthread_t signal_thread;
	OSLock *lock;
	void LinuxTimerQueueAction( LinuxTimerQueueActionArg *arg );
protected:
	LinuxTimerQueue();
public:
	~LinuxTimerQueue();
	bool addEvent
	( unsigned long micros, int type, ostimerq_handler func,
	  event_descriptor_t * arg, bool rm, unsigned *event );
	bool cancelEvent( int type, unsigned *event );
};

class LinuxTimerQueueFactory:public OSTimerQueueFactory {
public:
	virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock );
};



class LinuxTimer : public OSTimer {
	friend class LinuxTimerFactory;
 public:
	virtual unsigned long sleep(unsigned long micros) {
		struct timespec req = { 0, micros * 1000 };
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
 protected:
	LinuxTimer() {};
};

class LinuxTimerFactory:public OSTimerFactory {
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

class LinuxThread:public OSThread {
	friend class LinuxThreadFactory;
 private:

	pthread_t thread_id;
	OSThreadArg *arg_inner;
 public:
	virtual bool start(OSThreadFunction function, void *arg) {
		sigset_t set;
		sigset_t oset;
		int err;
		arg_inner = new OSThreadArg();
		arg_inner->func = function;
		arg_inner->arg = arg;
		sigemptyset(&set);
		sigaddset(&set, SIGALRM);
		err = pthread_sigmask(SIG_BLOCK, &set, &oset);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_BLOCK ... )");
			return false;
		}
		err = pthread_create(&thread_id, NULL, OSThreadCallback,
				     arg_inner);
		if (err != 0)
			return false;
		sigdelset(&oset, SIGALRM);
		err = pthread_sigmask(SIG_SETMASK, &oset, NULL);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_SETMASK ... )");
			return false;
		}

		return true;
	}
	virtual bool join(OSThreadExitCode & exit_code) {
		int err;
		err = pthread_join(thread_id, NULL);
		if (err != 0)
			return false;
		exit_code = arg_inner->ret;
		delete arg_inner;
		return true;
	}
 protected:
	LinuxThread() {};
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
	  HWTimestamper *timestamper ) {
		struct ifreq device;
		int err;
		struct sockaddr_ll ifsock_addr;
		struct packet_mreq mr_8021as;
		LinkLayerAddress addr;
		int ifindex;
		
		LinuxNetworkInterface *net_iface_l = new LinuxNetworkInterface();
		
		if( !net_iface_l->net_lock.init()) {
			XPTPD_ERROR( "Failed to initialize network lock");
			return false;
		}
		
		InterfaceName *ifname = dynamic_cast<InterfaceName *>(label);
		if( ifname == NULL ){
			XPTPD_ERROR( "ifname == NULL");
			return false;
		}
		
		net_iface_l->sd_general = socket( PF_PACKET, SOCK_DGRAM, 0 );
		if( net_iface_l->sd_general == -1 ) {
			XPTPD_ERROR( "failed to open general socket: %s", strerror(errno));
			return false;
		}
		net_iface_l->sd_event = socket( PF_PACKET, SOCK_DGRAM, 0 );
		if( net_iface_l->sd_event == -1 ) {
			XPTPD_ERROR
				( "failed to open event socket: %s \n", strerror(errno));
			return false;
		}
		
		memset( &device, 0, sizeof(device));
		ifname->toString( device.ifr_name, IFNAMSIZ );
		err = ioctl( net_iface_l->sd_event, SIOCGIFHWADDR, &device );
		if( err == -1 ) {
			XPTPD_ERROR
				( "Failed to get interface address: %s", strerror( errno ));
			return false;
		}
		
		addr = LinkLayerAddress( (uint8_t *)&device.ifr_hwaddr.sa_data );
		net_iface_l->local_addr = addr;
		err = ioctl( net_iface_l->sd_event, SIOCGIFINDEX, &device );
		if( err == -1 ) {
			XPTPD_ERROR
				( "Failed to get interface index: %s", strerror( errno ));
			return false;
		}
		ifindex = device.ifr_ifindex;
		net_iface_l->ifindex = ifindex;
		memset( &mr_8021as, 0, sizeof( mr_8021as ));
		mr_8021as.mr_ifindex = ifindex;
		mr_8021as.mr_type = PACKET_MR_MULTICAST;
		mr_8021as.mr_alen = 6;
		memcpy( mr_8021as.mr_address, P8021AS_MULTICAST, mr_8021as.mr_alen );
		err = setsockopt
			( net_iface_l->sd_event, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			  &mr_8021as, sizeof( mr_8021as ));
		if( err == -1 ) {
			XPTPD_ERROR
				( "Unable to add PTP multicast addresses to port id: %u",
				  ifindex );
			return false;
		}
		
		memset( &ifsock_addr, 0, sizeof( ifsock_addr ));
		ifsock_addr.sll_family = AF_PACKET;
		ifsock_addr.sll_ifindex = ifindex;
		ifsock_addr.sll_protocol = htons( PTP_ETHERTYPE );
		err = bind
			( net_iface_l->sd_event, (sockaddr *) &ifsock_addr,
			  sizeof( ifsock_addr ));	
		if( err == -1 ) { 
			XPTPD_ERROR( "Call to bind() failed: %s", strerror(errno) );
			return false;
		}
		
		net_iface_l->timestamper =
			dynamic_cast <LinuxTimestamper *>(timestamper);
		if(net_iface_l->timestamper == NULL) {
			XPTPD_ERROR( "timestamper == NULL\n" );
			return false;
		}
		net_iface_l->timestamper->setSocketDescriptor( net_iface_l->sd_event );
		net_iface_l->timestamper->setNetworkLock( &net_iface_l->net_lock );
		if( !net_iface_l->timestamper->post_init( ifindex )) {
			XPTPD_ERROR( "post_init failed\n" );
			return false;
		}
		*net_iface = net_iface_l;
		
		return true;
	}
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
	pthread_mutexattr_t mta;
	pthread_mutex_t mutex;
public:
	LinuxSharedMemoryIPC() {
		shm_fd = 0;
		err = 0;
		master_offset_buffer = NULL;
	};
	~LinuxSharedMemoryIPC() {
		munmap(master_offset_buffer, SHM_SIZE);
		shm_unlink(SHM_NAME);
	}
	virtual bool init( OS_IPC_ARG *barg = NULL ) {
		LinuxIPCArg *arg;
		struct group *grp;
		const char *group_name;
		if( barg == NULL ) {
			group_name = DEFAULT_GROUPNAME;
		} else {
			arg = dynamic_cast<LinuxIPCArg *> (barg);
			if( arg == NULL ) {
				XPTPD_ERROR( "Wrong IPC init arg type" );
				goto exit_error;
			} else {
				group_name = arg->group_name;
			}
		}
		grp = getgrnam( group_name );
		if( grp == NULL ) {
			XPTPD_ERROR( "Group %s not found", group_name );
			goto exit_error;
		}
		
		shm_fd = shm_open( SHM_NAME, O_RDWR | O_CREAT, 0660 );
		if( shm_fd == -1 ) {
			XPTPD_ERROR( "shm_open()" );
			goto exit_error;
		}
		if (fchown(shm_fd, -1, grp->gr_gid) < 0) {
			XPTPD_ERROR("fchwon()");
			goto exit_unlink;
		}
		if( ftruncate( shm_fd, SHM_SIZE ) == -1 ) {
			XPTPD_ERROR( "ftruncate()" );
			goto exit_unlink;
		}
		master_offset_buffer = (char *) mmap
			( NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_LOCKED | MAP_SHARED,
			  shm_fd, 0 );
		if( master_offset_buffer == (char *) -1 ) {
			XPTPD_ERROR( "mmap()" );
			goto exit_unlink;
		}
		/*create a mutex */
		err = pthread_mutexattr_init( &mta );
		if(err != 0) {
			XPTPD_ERROR
				("sharedmem - Mutex initialization failed - %s\n",
				 strerror(errno));
			return false;
		}
                
		return true;
	exit_unlink:
		shm_unlink( SHM_NAME ); 
	exit_error:
		return false;
	}
	virtual bool update
	(int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset,
	 FrequencyRatio ls_freqoffset, uint64_t local_time, uint32_t sync_count,
	 uint32_t pdelay_count, PortState port_state ) {
		int buf_offset = 0;
		char *shm_buffer = master_offset_buffer;
		gPtpTimeData *ptimedata;
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
		ptimedata->port_state   = port_state;
		/* unlock */
		pthread_mutex_unlock((pthread_mutex_t *) shm_buffer);
		return true;
	}
	void stop() {
		munmap( master_offset_buffer, SHM_SIZE );
		shm_unlink( SHM_NAME );
	}
};



#endif
