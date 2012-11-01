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

#define ONE_WAY_PHY_DELAY 400
#define P8021AS_MULTICAST "\x01\x80\xC2\x00\x00\x0E"

static inline Timestamp tsToTimestamp(struct timespec *ts)
{
	Timestamp ret;
	if (sizeof(ts->tv_sec) > sizeof(ret.seconds_ls)) {
		ret.seconds_ms = ts->tv_sec >> (sizeof(ret.seconds_ls) * 8);
		ret.seconds_ls = ts->tv_sec & 0xFFFFFFFF;
		ret.nanoseconds = ts->tv_nsec;
	} else {
		ret.seconds_ms = 0;
		ret.seconds_ls = ts->tv_sec;
		ret.nanoseconds = ts->tv_nsec;
	}
	return ret;
}

class LinuxTimestamper:public HWTimestamper {
 private:
	int sd;
	Timestamp crstamp_system;
	Timestamp crstamp_device;
	pthread_mutex_t cross_stamp_lock;
	bool cross_stamp_good;
	 std::list < Timestamp > rxTimestampList;
 public:
	 virtual bool HWTimestamper_init(InterfaceLabel * iface_label) {
		pthread_mutex_init(&cross_stamp_lock, NULL);
		sd = -1;
		cross_stamp_good = false;
		return true;
	} 
	void setSocketDescriptor(int sd) {
		this->sd = sd;
	};
	void updateCrossStamp(Timestamp * system_time, Timestamp * device_time) {
		pthread_mutex_lock(&cross_stamp_lock);
		crstamp_system = *system_time;
		crstamp_device = *device_time;
		cross_stamp_good = true;
		pthread_mutex_unlock(&cross_stamp_lock);
	}
	void pushRXTimestamp(Timestamp * tstamp) {
		rxTimestampList.push_front(*tstamp);
	}
	bool post_init(int ifindex) {
		int timestamp_flags = 0;
		struct ifreq device;
		struct hwtstamp_config hwconfig;
		int err;

		memset(&device, 0, sizeof(device));
		device.ifr_ifindex = ifindex;
		err = ioctl(sd, SIOCGIFNAME, &device);
		if (err == -1) {
			XPTPD_ERROR("Failed to get interface name: %s",
				    strerror(errno));
			return false;
		}

		device.ifr_data = (char *)&hwconfig;
		memset(&hwconfig, 0, sizeof(hwconfig));
		hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_L2_EVENT;
		hwconfig.tx_type = HWTSTAMP_TX_ON;
		printf("TX type = %u\n", hwconfig.tx_type);
		printf("RX filter = %u\n", hwconfig.rx_filter);
		err = ioctl(sd, SIOCSHWTSTAMP, &device);
		if (err == -1) {
			XPTPD_ERROR("Failed to configure timestamping: %s",
				    strerror(errno));
			return false;
		}

		timestamp_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
		timestamp_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;
		err =
		    setsockopt(sd, SOL_SOCKET, SO_TIMESTAMPING,
			       &timestamp_flags, sizeof(timestamp_flags));
		if (err == -1) {
			XPTPD_ERROR
			    ("Failed to configure timestamping on socket: %s",
			     strerror(errno));
			return false;
		}

		return true;
	}

	virtual bool HWTimestamper_gettime(Timestamp * system_time,
					   Timestamp * device_time,
					   uint32_t * local_clock,
					   uint32_t * nominal_clock_rate) {
		bool ret = false;
		pthread_mutex_lock(&cross_stamp_lock);
		if (cross_stamp_good) {
			*system_time = crstamp_system;
			*device_time = crstamp_device;
			ret = true;
		}
		pthread_mutex_unlock(&cross_stamp_lock);
		return ret;
	}

	virtual int HWTimestamper_txtimestamp(PortIdentity * identity,
					      uint16_t sequenceId,
					      Timestamp & timestamp,
					      unsigned &clock_value,
					      bool last) {
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

		if (sd == -1)
			return -1;
		memset(&msg, 0, sizeof(msg));

		msg.msg_iov = &sgentry;
		msg.msg_iovlen = 1;

		sgentry.iov_base = NULL;
		sgentry.iov_len = 0;

		memset(&remote, 0, sizeof(remote));
		msg.msg_name = (caddr_t) & remote;
		msg.msg_namelen = sizeof(remote);
		msg.msg_control = &control;
		msg.msg_controllen = sizeof(control);

		err = recvmsg(sd, &msg, MSG_ERRQUEUE);
		if (err == -1) {
			if (errno == EAGAIN)
				return -72;
			else
				return -1;
		}

		cmsg = CMSG_FIRSTHDR(&msg);
		while (cmsg != NULL) {
			if (cmsg->cmsg_level == SOL_SOCKET
			    && cmsg->cmsg_type == SO_TIMESTAMPING) {
				struct timespec *ts_device, *ts_system;
				Timestamp device, system;
				ts_system =
				    ((struct timespec *)CMSG_DATA(cmsg)) + 1;
				system = tsToTimestamp(ts_system);
				ts_device = ts_system + 1;
				device = tsToTimestamp(ts_device);
				updateCrossStamp(&system, &device);
				timestamp = device;
				ret = 0;
				break;
			}
			cmsg = CMSG_NXTHDR(&msg, cmsg);
		}

		return ret;
	}

	virtual int HWTimestamper_rxtimestamp(PortIdentity * identity,
					      uint16_t sequenceId,
					      Timestamp & timestamp,
					      unsigned &clock_value,
					      bool last) {
		if (rxTimestampList.empty())
			return -72;
		timestamp = rxTimestampList.back();
		rxTimestampList.pop_back();

		return 0;
	}
};

class LinuxLock:public OSLock {
	friend class LinuxLockFactory;
 private:
	 OSLockType type;
	pthread_t thread_id;
	int lock_c;
	pthread_mutexattr_t mta;
	pthread_mutex_t mutex;
	pthread_cond_t port_ready_signal;
 protected:
	LinuxLock() {
		lock_c = NULL;
	} 
	bool initialize(OSLockType type) {
		pthread_mutexattr_init(&mta);
		if (type == oslock_recursive)
			pthread_mutexattr_settype(&mta,
						  PTHREAD_MUTEX_RECURSIVE);
		lock_c = pthread_mutex_init(&mutex, &mta);
		if (lock_c != 0) {
			XPTPD_ERROR("Mutex initialization faile - %s\n",
				    strerror(errno));
			return oslock_fail;
		}
		return oslock_ok;

	}
	OSLockResult lock() {
		lock_c = pthread_mutex_lock(&mutex);
		if (lock_c != 0)
			return oslock_fail;
		return oslock_ok;
	}
	OSLockResult trylock() {
		lock_c = pthread_mutex_trylock(&mutex);
		if (lock_c != 0)
			return oslock_fail;
		return oslock_ok;
	}
	OSLockResult unlock() {
		lock_c = pthread_mutex_unlock(&mutex);
		if (lock_c != 0)
			return oslock_fail;
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

class LinuxTimerQueue;

struct TimerQueue_t;

struct LinuxTimerQueueHandlerArg {
	timer_t timer_handle;
	struct sigevent sevp;
	event_descriptor_t *inner_arg;
	ostimerq_handler func;
	int type;
	bool rm;
	TimerQueue_t *timer_queue;
};

typedef std::list < LinuxTimerQueueHandlerArg * >TimerArgList_t;

struct TimerQueue_t {
	TimerArgList_t arg_list;
	pthread_mutex_t lock;
};

typedef std::map < int, TimerQueue_t > TimerQueueMap_t;

void LinuxTimerQueueHandler(union sigval arg_in);

class LinuxTimerQueue:public OSTimerQueue {
	friend class LinuxTimerQueueFactory;
	friend void LinuxTimerQueueHandler(union sigval arg_in);
 private:
	TimerQueueMap_t timerQueueMap;
	TimerArgList_t retiredTimers;
	bool in_callback;
 protected:
	LinuxTimerQueue() {
		int err;
		pthread_mutex_t retiredTimersLock;
		pthread_mutexattr_t retiredTimersLockAttr;
		err = pthread_mutexattr_init(&retiredTimersLockAttr);
		if (err != 0) {
			XPTPD_ERROR("mutexattr_init()");
			exit(0);
		}
		err =
		    pthread_mutexattr_settype(&retiredTimersLockAttr,
					      PTHREAD_MUTEX_NORMAL);
		if (err != 0) {
			XPTPD_ERROR("mutexattr_settype()");
			exit(0);
		}
		err =
		    pthread_mutex_init(&retiredTimersLock,
				       &retiredTimersLockAttr);
		if (err != 0) {
			XPTPD_ERROR("mutex_init()");
			exit(0);
		}
	};
 public:
	bool addEvent(unsigned long micros, int type, ostimerq_handler func,
		      event_descriptor_t * arg, bool rm, unsigned *event) {
		LinuxTimerQueueHandlerArg *outer_arg =
		    new LinuxTimerQueueHandlerArg();
		if (timerQueueMap.find(type) == timerQueueMap.end()) {
			pthread_mutex_init(&timerQueueMap[type].lock, NULL);
		}
		outer_arg->inner_arg = arg;
		outer_arg->func = func;
		outer_arg->type = type;
		outer_arg->timer_queue = &timerQueueMap[type];
		sigset_t set;
		sigset_t oset;
		int err;
		timer_t timerhandle;
		struct sigevent ev;

		sigemptyset(&set);
		sigaddset(&set, SIGALRM);
		err = pthread_sigmask(SIG_BLOCK, &set, &oset);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_BLOCK ... )");
			exit(0);
		}
		pthread_mutex_lock(&timerQueueMap[type].lock);
		{
			struct itimerspec its;
			memset(&ev, 0, sizeof(ev));
			ev.sigev_notify = SIGEV_THREAD;
			ev.sigev_value.sival_ptr = outer_arg;
			ev.sigev_notify_function = LinuxTimerQueueHandler;
			ev.sigev_notify_attributes = new pthread_attr_t;
			pthread_attr_init((pthread_attr_t *) ev.sigev_notify_attributes);
			pthread_attr_setdetachstate((pthread_attr_t *) ev.sigev_notify_attributes,
						    PTHREAD_CREATE_DETACHED);
			if (timer_create(CLOCK_MONOTONIC, &ev, &timerhandle) == -1) {
				XPTPD_ERROR("timer_create failed - %s\n",
					    strerror(errno));
				exit(0);
			}
			outer_arg->timer_handle = timerhandle;
			outer_arg->sevp = ev;

			memset(&its, 0, sizeof(its));
			its.it_value.tv_sec = micros / 1000000;
			its.it_value.tv_nsec = (micros % 1000000) * 1000;
			timer_settime(outer_arg->timer_handle, 0, &its, NULL);
		}
		timerQueueMap[type].arg_list.push_front(outer_arg);

		pthread_mutex_unlock(&timerQueueMap[type].lock);

		err = pthread_sigmask(SIG_SETMASK, &oset, NULL);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_SETMASK ... )");
			exit(0);
		}
		return true;
	}
	bool cancelEvent(int type, unsigned *event) {
		TimerQueueMap_t::iterator iter = timerQueueMap.find(type);
		if (iter == timerQueueMap.end())
			return false;
		sigset_t set;
		sigset_t oset;
		int err;
		sigemptyset(&set);
		sigaddset(&set, SIGALRM);
		err = pthread_sigmask(SIG_BLOCK, &set, &oset);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_BLOCK ... )");
			exit(0);
		}
		pthread_mutex_lock(&timerQueueMap[type].lock);
		while (!timerQueueMap[type].arg_list.empty()) {
			LinuxTimerQueueHandlerArg *del_arg =
			    timerQueueMap[type].arg_list.front();
			timerQueueMap[type].arg_list.pop_front();
			timer_delete(del_arg->timer_handle);
			delete(pthread_attr_t *) del_arg->sevp.
			    sigev_notify_attributes;
			if (del_arg->rm)
				delete del_arg->inner_arg;
			delete del_arg;
		}
		pthread_mutex_unlock(&timerQueueMap[type].lock);
		err = pthread_sigmask(SIG_SETMASK, &oset, NULL);
		if (err != 0) {
			XPTPD_ERROR
			    ("Add timer pthread_sigmask( SIG_SETMASK ... )");
			exit(0);
		}
		return true;
	}
};

void LinuxTimerQueueHandler(union sigval arg_in)
{
	LinuxTimerQueueHandlerArg *arg =
	    (LinuxTimerQueueHandlerArg *) arg_in.sival_ptr;
	bool runnable = false;
	unsigned size;

	pthread_mutex_lock(&arg->timer_queue->lock);
	size = arg->timer_queue->arg_list.size();
	arg->timer_queue->arg_list.remove(arg);
	if (arg->timer_queue->arg_list.size() < size) {
		runnable = true;
	}
	pthread_mutex_unlock(&arg->timer_queue->lock);

	if (runnable) {
		arg->func(arg->inner_arg);
		if (arg->rm)
			delete arg->inner_arg;
		delete(pthread_attr_t *) arg->sevp.sigev_notify_attributes;
	}
}

class LinuxTimerQueueFactory:public OSTimerQueueFactory {
 public:
	virtual OSTimerQueue * createOSTimerQueue() {
		LinuxTimerQueue *timerq = new LinuxTimerQueue();
		return timerq;
	};
};

class LinuxTimer:public OSTimer {
	friend class LinuxTimerFactory;
 public:
	virtual unsigned long sleep(unsigned long micros) {
		struct timespec req = { 0, micros };	/* Should be micros*1000 -Chris */
		nanosleep(&req, NULL);
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

void *OSThreadCallback(void *input)
{
	OSThreadArg *arg = (OSThreadArg *) input;
	arg->ret = arg->func(arg->arg);
	return 0;
}

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

class LinuxPCAPNetworkInterface:public OSNetworkInterface {
	friend class LinuxPCAPNetworkInterfaceFactory;
 private:
	LinkLayerAddress local_addr;
	int sd_event;
	int sd_general;
	LinuxTimestamper *timestamper;
	int ifindex;
 public:
	virtual net_result send(LinkLayerAddress * addr, uint8_t * payload,
				size_t length, bool timestamp) {
		sockaddr_ll *remote = NULL;
		int err;
		remote = new struct sockaddr_ll;
		memset(remote, 0, sizeof(*remote));
		remote->sll_family = AF_PACKET;
		remote->sll_protocol = htons(PTP_ETHERTYPE);
		remote->sll_ifindex = ifindex;
		remote->sll_halen = ETH_ALEN;
		addr->toOctetArray(remote->sll_addr);
		if (timestamp) {
			err = sendto(sd_event, payload, length, 0,
				   (sockaddr *) remote, sizeof(*remote));
		} else {
			err = sendto(sd_general, payload, length, 0,
				   (sockaddr *) remote, sizeof(*remote));
		}
		delete remote;
		if (err == -1) {
			XPTPD_ERROR("Failed to send: %s(%d)", strerror(errno),
				    errno);
			return net_fatal;
		}
		return net_succeed;
	}

	virtual net_result recv(LinkLayerAddress * addr, uint8_t * payload,
				size_t & length) {
		fd_set readfds;
		int err;
		struct msghdr msg;
		struct cmsghdr *cmsg;
		struct {
			struct cmsghdr cm;
			char control[256];
		} control;
		struct sockaddr_ll remote;
		struct iovec sgentry;

		struct timeval timeout = { 0, 100000 };

		FD_ZERO(&readfds);
		FD_SET(sd_event, &readfds);

		err = select(sd_event + 1, &readfds, NULL, NULL, &timeout);
		if (err == 0) {
			return net_trfail;
		} else if (err == -1) {
			if (err == EINTR) {
				XPTPD_ERROR("select() recv signal");
			} else {
				XPTPD_ERROR("select() failed");
				return net_fatal;
			}
		}

		memset(&msg, 0, sizeof(msg));

		msg.msg_iov = &sgentry;
		msg.msg_iovlen = 1;

		sgentry.iov_base = payload;
		sgentry.iov_len = length;

		memset(&remote, 0, sizeof(remote));
		msg.msg_name = (caddr_t) & remote;
		msg.msg_namelen = sizeof(remote);
		msg.msg_control = &control;
		msg.msg_controllen = sizeof(control);

		err = recvmsg(sd_event, &msg, 0);
		if (err < 0)
			return net_fatal;
		*addr = LinkLayerAddress(remote.sll_addr);

		if (err > 0 && !(payload[0] & 0x8)) {
			cmsg = CMSG_FIRSTHDR(&msg);
			while (cmsg != NULL) {
				if (cmsg->cmsg_level == SOL_SOCKET
				    && cmsg->cmsg_type == SO_TIMESTAMPING) {
					struct timespec *ts_device, *ts_system;
					Timestamp device, system;
					ts_system =
					    ((struct timespec *)CMSG_DATA(cmsg))
					    + 1;
					system = tsToTimestamp(ts_system);
					ts_device = ts_system + 1;
					device = tsToTimestamp(ts_device);
					timestamper->updateCrossStamp(&system,
								      &device);
					timestamper->pushRXTimestamp(&device);
					break;
				}
				cmsg = CMSG_NXTHDR(&msg, cmsg);
			}
		}

		length = err;
		return net_succeed;
	}

	virtual void getLinkLayerAddress(LinkLayerAddress * addr) {
		*addr = local_addr;
	}

	virtual unsigned getPayloadOffset() {
		return 0;
	}

	virtual ~ LinuxPCAPNetworkInterface() {
		close(sd_event);
		close(sd_general);
	}

 protected:
	LinuxPCAPNetworkInterface() {};
};

class LinuxPCAPNetworkInterfaceFactory:public OSNetworkInterfaceFactory {
 public:
	virtual bool createInterface(OSNetworkInterface ** net_iface,
				     InterfaceLabel * label,
				     HWTimestamper * timestamper) {
		struct ifreq device;
		int err;
		struct sockaddr_ll ifsock_addr;
		struct packet_mreq mr_8021as;
		LinkLayerAddress addr;
		int ifindex;

		LinuxPCAPNetworkInterface *net_iface_l =
		    new LinuxPCAPNetworkInterface();
		InterfaceName *ifname = dynamic_cast < InterfaceName * >(label);
		if (ifname == NULL) {
			XPTPD_ERROR("ifame == NULL \n");
			return false;
		}

		net_iface_l->sd_general =
		    socket(PF_PACKET, SOCK_DGRAM, htons(PTP_ETHERTYPE));
		if (net_iface_l->sd_general == -1) {
			XPTPD_ERROR("failed to open general socket: %s \n",
				    strerror(errno));
			return false;
		}
		net_iface_l->sd_event =
		    socket(PF_PACKET, SOCK_DGRAM, htons(PTP_ETHERTYPE));
		if (net_iface_l->sd_event == -1) {
			XPTPD_ERROR("failed to open event socket: %s \n",
				    strerror(errno));
			return false;
		}

		memset(&device, 0, sizeof(device));
		ifname->toString(device.ifr_name, IFNAMSIZ);
		err = ioctl(net_iface_l->sd_event, SIOCGIFHWADDR, &device);
		if (err == -1) {
			XPTPD_ERROR("Failed to get interface address: %s",
				    strerror(errno));
			return false;
		}
		addr = LinkLayerAddress(((struct sockaddr_ll *)&device.ifr_hwaddr)->sll_addr);
		net_iface_l->local_addr = addr;

		err = ioctl(net_iface_l->sd_event, SIOCGIFINDEX, &device);
		if (err == -1) {
			XPTPD_ERROR("Failed to get interface index: %s",
				    strerror(errno));
			return false;
		}
		ifindex = device.ifr_ifindex;
		net_iface_l->ifindex = ifindex;
		memset(&mr_8021as, 0, sizeof(mr_8021as));
		mr_8021as.mr_ifindex = ifindex;
		mr_8021as.mr_type = PACKET_MR_MULTICAST;
		mr_8021as.mr_alen = 6;
		memcpy(mr_8021as.mr_address, P8021AS_MULTICAST,
		       mr_8021as.mr_alen);
		err = setsockopt(net_iface_l->sd_event, SOL_PACKET,
			       PACKET_ADD_MEMBERSHIP, &mr_8021as,
			       sizeof(mr_8021as));
		if (err == -1) {
			XPTPD_ERROR
			    ("Unable to add PTP multicast addresses to port id: %u",
			     ifindex);
			return false;
		}

		memset(&ifsock_addr, 0, sizeof(ifsock_addr));
		ifsock_addr.sll_family = AF_PACKET;
		ifsock_addr.sll_ifindex = ifindex;
		ifsock_addr.sll_protocol = htons(PTP_ETHERTYPE);
		err = bind(net_iface_l->sd_event, (sockaddr *) & ifsock_addr,
			 sizeof(ifsock_addr));
		if (err == -1) {
			XPTPD_ERROR("Call to bind() failed: %s",
				    strerror(errno));
			return false;
		}

		net_iface_l->timestamper = dynamic_cast < LinuxTimestamper * >(timestamper);
		if (net_iface_l->timestamper == NULL) {
			XPTPD_ERROR("timestamper == NULL\n");
			return false;
		}
		net_iface_l->timestamper->setSocketDescriptor(net_iface_l->sd_event);
		if (!net_iface_l->timestamper->post_init(ifindex)) {
			XPTPD_ERROR("post_init failed\n");
			return false;
		}
		*net_iface = net_iface_l;

		return true;
	}
};

class LinuxSimpleIPC:public OS_IPC {
 public:
	LinuxSimpleIPC() {};
	~LinuxSimpleIPC() {}
	virtual bool init() {
		return true;
	}
	virtual bool update(int64_t ml_phoffset, int64_t ls_phoffset,
			    int32_t ml_freqoffset, int32_t ls_freqoffset,
			    uint64_t local_time) {
#ifdef DEBUG
		fprintf(stderr,
			"Master-Local Phase Offset: %ld\n"
			"Master-Local Frequency Offset: %d\n"
			"Local-System Phase Offset: %ld\n"
			"Local-System Frequency Offset: %d\n"
			"Local Time: %lu\n", ml_phoffset, ml_freqoffset,
			ls_phoffset, ls_freqoffset, local_time);
#endif
		return true;
	}
};

#endif
