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

#include <linux_hal_generic.hpp>
#include <linux_hal_generic_tsprivate.hpp>
#include <sys/select.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <errno.h>
#include <linux/ethtool.h>
#include <net/if.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/net_tstamp.h>
#include <linux/ptp_clock.h>
#include <syscall.h>
#include <limits.h>
#include <sys/timex.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define TX_PHY_TIME 184
#define RX_PHY_TIME 382

net_result gLockErrorStatus = net_succeed;

class ALockKeeper
{
public:
	ALockKeeper(TicketingLock *l) : fStatus(net_succeed), fLock(l) 
	{
		bool gotLock;
		if (!l->lock(&gotLock)) 
		{
	      GPTP_LOG_ERROR("ALockKeeper Failed to lock mutex");
	      fStatus = net_fatal;
	    }

		if (!gotLock)
		{
			//GPTP_LOG_ERROR("A Failed to get a lock mutex");
			fStatus = net_trfail;
		}
	}
	~ALockKeeper()
	{
		if (!fLock->unlock())
		{
			GPTP_LOG_ERROR("ALockKeeper Failed to unlock");
			gLockErrorStatus = net_fatal;
		}
		else
		{
			gLockErrorStatus = net_succeed;
		}
	}

	net_result GetStatus() const
	{
		return fStatus;
	}

private:
	net_result fStatus;
	TicketingLock *fLock;
};

net_result LinuxNetworkInterface::receive(LinkLayerAddress *addr, uint8_t *payload, 
	size_t &length, uint16_t portNum, Timestamp& ingressTime)
{
	//GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  length:%d   portNum:%d", length, portNum);
	fd_set readfds;
	int err;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct {
		struct cmsghdr cm;
		char control[256];
	} control;

	struct sockaddr_storage remoteAddress;
	size_t remoteSize = sizeof(struct sockaddr_storage);

	struct iovec sgentry;
	net_result ret = net_succeed;

	int fileDesc = 0;
	TicketingLock *netLock = nullptr;
	if (EVENT_PORT == portNum)
	{
		fileDesc = sd_event;
		netLock = &fNetLockEvent;
	}
	else if (GENERAL_PORT == portNum)
	{
		fileDesc = sd_general;
		netLock = &fNetLockGeneral;
	}
	else
	{
		GPTP_LOG_ERROR("Invalid port number %d must be %d or %d.", portNum,
			EVENT_PORT, GENERAL_PORT);
    	return net_fatal;
    }

	std::shared_ptr<LinuxTimestamperGeneric> gtimestamper;

	struct timeval timeout = { 0, 16000 }; // 16 ms

	{
		ALockKeeper keeper(netLock);
		ret = keeper.GetStatus();
		if (net_succeed == ret)
		{
			FD_ZERO(&readfds);
			FD_SET(fileDesc, &readfds);

			err = select(fileDesc+1, &readfds, NULL, NULL, &timeout);
			if (err == 0) 
			{
				// GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  err == 0 after select"
				//  "   fileDesc:%d  timeout.tv_sec: %d timeout.tv_usec: %d", fileDesc, 
				//  timeout.tv_sec, timeout.tv_usec);
				ret = net_trfail;
			}
			else if (err == -1) 
			{
				if (errno == EINTR)
				{
					// Caught signal
					GPTP_LOG_ERROR("select() recv signal");
					ret = net_trfail;
				} 
				else
				{
					GPTP_LOG_ERROR("select() failed");
					ret = net_fatal;
		    	}
			} 
			else if (!FD_ISSET(fileDesc, &readfds)) 
			{
				GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  FD_ISSET  failed");
				ret = net_trfail;
			}
			else
			{
				memset(&msg, 0, sizeof(msg));

				msg.msg_iov = &sgentry;
				msg.msg_iovlen = 1;

				sgentry.iov_base = payload;
				sgentry.iov_len = length;

				memset(&remoteAddress, 0, remoteSize);
				msg.msg_name = &remoteAddress;
				msg.msg_namelen = remoteSize;

				msg.msg_control = &control;
				msg.msg_controllen = sizeof(control);

				err = recvmsg(fileDesc, &msg, 0);
				GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive After call to recvmsg err:%d", err);
				if (err < 0) 
				{
					if (errno == ENOMSG)
					{
						GPTP_LOG_ERROR("Got ENOMSG: %s:%d", __FILE__, __LINE__);
						ret = net_trfail;
					}
					else
					{
						GPTP_LOG_ERROR("LinuxNetworkInterface::receive recvmsg() failed: %s", strerror(errno));
						ret = net_fatal;
					}
				}
				else
				{
					if (AF_INET == remoteAddress.ss_family)
					{
						GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive   remoteAddress  IPV4");
						sockaddr_in* address = reinterpret_cast<sockaddr_in*>(&remoteAddress);
						*addr = LinkLayerAddress(*address);
					}
					else
					{
						GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive   remoteAddress  IPV6");
						sockaddr_in6* address = reinterpret_cast<sockaddr_in6*>(&remoteAddress);
						*addr = LinkLayerAddress(*address);
					}

					struct timespec ts;
					clock_gettime(CLOCK_REALTIME, &ts);
					ingressTime = Timestamp(ts);

					gtimestamper = std::dynamic_pointer_cast<LinuxTimestamperGeneric>(timestamper);
					if (err > 0 && !(payload[0] & 0x8) && gtimestamper != NULL)
					{
						GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  getting timestamp");

						/* Retrieve the timestamp */
						cmsg = CMSG_FIRSTHDR(&msg);
						if (nullptr == cmsg)
						{
#ifndef APTP
							// There are no headers to process so we need to set the ingress time
							Timestamp device = ingressTime;
							gtimestamper->pushRXTimestamp(&device);
#endif
						}
						else
						{
							while (cmsg != nullptr)
							{
								GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  cmsg->cmsg_level:%d  cmsg->cmsg_type:%d", cmsg->cmsg_level, cmsg->cmsg_type);
								if (cmsg->cmsg_level == SOL_SOCKET &&
								 cmsg->cmsg_type == SO_TIMESTAMPING) 
								{
									struct timespec *ts_device, *ts_system;
									Timestamp device, system;
									ts_system = ((struct timespec *) CMSG_DATA(cmsg)) + 1;
									system = tsToTimestamp( ts_system );
									GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  ts_system.tv_sec:%d  ts_system.tv_nsec:%d", ts_system->tv_sec, ts_system->tv_nsec);
									ts_device = ts_system + 1; 
									device = tsToTimestamp( ts_device );
									ingressTime = device;
									GPTP_LOG_VERBOSE("LinuxNetworkInterface::receive  ts_device.tv_sec:%d  ts_device.tv_nsec:%d", ts_device->tv_sec, ts_device->tv_nsec);
#ifndef APTP
									gtimestamper->pushRXTimestamp( &device );
#endif
									break;
								}
								cmsg = CMSG_NXTHDR(&msg,cmsg);
							}
						}
					}
				}
			}
		}
	}

	length = err;

	if (gLockErrorStatus != net_succeed)
	{
		ret = gLockErrorStatus;
	}

	return ret;	
}


// Providing an implementation for this but it should never be invoked for the
// LinuxNetworkInterface.
net_result LinuxNetworkInterface::nrecv(LinkLayerAddress *addr, uint8_t *payload, 
 size_t &length) 
{
	Timestamp ingressTime;
	net_result result = receive(addr, payload, length, 319, ingressTime);
	result = Filter(result, addr, 0);
	return result;
}

net_result LinuxNetworkInterface::nrecvEvent(LinkLayerAddress *addr,
 uint8_t *payload, size_t &length, Timestamp& ingressTime, EtherPort* port) 
{
	net_result result = receive(addr, payload, length, 319, ingressTime);
	result = Filter(result, addr, port);
	return result;
}

net_result LinuxNetworkInterface::nrecvGeneral(LinkLayerAddress *addr, 
 uint8_t *payload, size_t &length, Timestamp& ingressTime, EtherPort* port) 
{
	net_result result = receive(addr, payload, length, 320, ingressTime);
	result = Filter(result, addr, port);
	return result;
}

int findPhcIndex( InterfaceLabel *iface_label ) {
	int sd;
	InterfaceName *ifname;
	struct ethtool_ts_info info;
	struct ifreq ifr;

	if(( ifname = dynamic_cast<InterfaceName *>(iface_label)) == NULL ) {
		GPTP_LOG_ERROR("findPTPIndex requires InterfaceName");
		return -1;
	}

	sd = socket( AF_UNIX, SOCK_DGRAM, 0 );
	if( sd < 0 ) {
		GPTP_LOG_ERROR("findPTPIndex: failed to open socket");
		return -1;
	}

	memset( &ifr, 0, sizeof(ifr));
	memset( &info, 0, sizeof(info));
	info.cmd = ETHTOOL_GET_TS_INFO;
	ifname->toString( ifr.ifr_name, IFNAMSIZ-1 );
	ifr.ifr_data = (char *) &info;

	if( ioctl( sd, SIOCETHTOOL, &ifr ) < 0 ) {
		GPTP_LOG_ERROR("findPTPIndex: ioctl(SIOETHTOOL) failed");
		return -1;
	}

	close(sd);

	GPTP_LOG_INFO("findPTPIndex:  info.phc_index: %d", info.phc_index);
	return info.phc_index;
}

LinuxTimestamperGeneric::~LinuxTimestamperGeneric() {
	if( _private != NULL ) delete _private;
#ifdef WITH_IGBLIB
	if( igb_private != NULL ) delete igb_private;
#endif
}

LinuxTimestamperGeneric::LinuxTimestamperGeneric() :
	fPort(nullptr)
{
	_private = NULL;
#ifdef WITH_IGBLIB
	igb_private = NULL;
#endif
	sd = -1;
}

bool LinuxTimestamperGeneric::Adjust(const timeval& tm)
{
	bool ok = true;
	struct timeval tv, oldAdjTv;

	tv.tv_sec = tm.tv_sec;
	tv.tv_usec = tm.tv_usec;

	if (tm.tv_sec < 0)
	{
		tv.tv_sec = -tm.tv_sec;
	}
	if (tm.tv_usec < 0)
	{
		tv.tv_usec = -tm.tv_usec;
	}

	const time_t kMaxAdjust = (INT_MAX / 1000000 - 2);
	if (tv.tv_sec > kMaxAdjust)
	{
		tv.tv_sec = kMaxAdjust;
	}
	if (tv.tv_usec > kMaxAdjust)
	{
		tv.tv_usec = kMaxAdjust;
	}

	if (adjtime(&tv, &oldAdjTv) < 0) 
	{
		GPTP_LOG_ERROR("adjtime failed(%d): %s", errno, strerror(errno));
		ok = false;
	}

	return ok;
}

bool LinuxTimestamperGeneric::Adjust( void *tmx ) const
{
	if (nullptr == tmx)
	{
		GPTP_LOG_INFO("Time adjustment tmx is nullptr");
		return false;
	}
	timex* t = static_cast<timex *>(tmx);

	// This system call works
	int adjRet = adjtimex(t);
	if (-1 == adjRet)
	{
		GPTP_LOG_ERROR("Time adjustment failed: %s", strerror(errno));
	   return false;
	}
	GPTP_LOG_INFO("adjtimex adjRet: %d", adjRet);

	// This system call doesnt work on ubuntu
	// GPTP_LOG_INFO("adjtimex _private->clockid: %d", _private->clockid);
	// int adjRet = syscall(__NR_clock_adjtime, _private->clockid, tmx);
	// //int adjRet = sys_clock_adjtime(_private->clockid, tmx);
	// if (adjRet != 0) {
	// 	GPTP_LOG_ERROR("Failed to adjust PTP clock rate.  %s", strerror(errno));
	// 	return false;
	// }

	{
		struct timeval tv = t->time;
		time_t theTime;
		struct tm *theTimeTm;
		char tmbuf[64], buf[64];

		theTime = tv.tv_sec;
		theTimeTm = localtime(&theTime);
		strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", theTimeTm);
		snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);	
		GPTP_LOG_INFO("Time adjustment:%s", buf);
	}

	return true;
}

void LinuxTimestamperGeneric::logCurrentTime(const char * msg)
{
	struct timeval tv;
	time_t nowtime;
	struct tm *nowtm;
	char tmbuf[64], buf[64];

	gettimeofday(&tv, NULL);
	nowtime = tv.tv_sec;
	nowtm = localtime(&nowtime);
	strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);
	snprintf(buf, sizeof buf, "%s.%06ld", tmbuf, tv.tv_usec);
	GPTP_LOG_INFO("Current time(%s):%s", msg, buf);
}		


bool LinuxTimestamperGeneric::HWTimestamper_init(InterfaceLabel *iface_label,
 OSNetworkInterface *iface, EtherPort *port)
{
	cross_stamp_good = false;
	int phc_index;
	char ptp_device[] = PTP_DEVICE;
#ifdef PTP_HW_CROSSTSTAMP
	struct ptp_clock_caps ptp_capability;
#endif
	_private = new LinuxTimestamperGenericPrivate;

	fPort = port;

	pthread_mutex_init( &_private->cross_stamp_lock, NULL );

	// Determine the correct PTP clock interface
	phc_index = findPhcIndex( iface_label );
	if( phc_index < 0 ) {
		GPTP_LOG_ERROR("Failed to find PTP device index");
		return false;
	}

	snprintf
		( ptp_device+PTP_DEVICE_IDX_OFFS,
		  sizeof(ptp_device)-PTP_DEVICE_IDX_OFFS, "%d", phc_index );
	GPTP_LOG_ERROR("Using clock device: %s", ptp_device);
	phc_fd = open( ptp_device, O_RDWR );
	if( phc_fd == -1 || (_private->clockid = FD_TO_CLOCKID(phc_fd)) == -1 ) {
		GPTP_LOG_ERROR("Failed to open PTP clock device");
		return false;
	}

#ifdef PTP_HW_CROSSTSTAMP
	// Query PTP stack for availability of HW cross-timestamp
	if( ioctl( phc_fd, PTP_CLOCK_GETCAPS, &ptp_capability ) == -1 )
	{
		GPTP_LOG_ERROR("Failed to query PTP clock capabilities");
		return false;
	}
	precise_timestamp_enabled = ptp_capability.cross_timestamping;
#endif

	if( !resetFrequencyAdjustment() ) {
		GPTP_LOG_ERROR("Failed to reset (zero) frequency adjustment");
		return false;
	}

	if( dynamic_cast<LinuxNetworkInterface *>(iface) != NULL ) {
		iface_list.push_front
			( (dynamic_cast<LinuxNetworkInterface *>(iface)) );
	}

	return true;
}

void LinuxTimestamperGeneric::HWTimestamper_reset()
{
	if( !resetFrequencyAdjustment() ) {
		GPTP_LOG_ERROR("Failed to reset (zero) frequency adjustment");
	}
}

int LinuxTimestamperGeneric::HWTimestamper_txtimestamp
( std::shared_ptr<PortIdentity> identity, PTPMessageId messageId, Timestamp &timestamp,
  unsigned &clock_value, bool last ) {
	int err;
	int ret = GPTP_EC_EAGAIN;
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
			ret = GPTP_EC_EAGAIN;
			goto done;
		}
		else {
			ret = GPTP_EC_FAILURE;
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
			timestamp = device;
			ret = 0;
			break;
		}
		cmsg = CMSG_NXTHDR(&msg,cmsg);
	}

	if( ret != 0 ) {
		GPTP_LOG_ERROR("Received a error message, but didn't find a valid timestamp");
	}

 done:
	if( ret == 0 || last ) {
		net_lock->unlock();
	}

	return ret;
}

bool LinuxTimestamperGeneric::post_init( int ifindex, int sd, TicketingLock *lock ) {
	int timestamp_flags = 0;
	struct ifreq device;
	struct hwtstamp_config hwconfig;
	int err;

	this->sd = sd;
	this->net_lock = lock;

	memset(&device, 0, sizeof(device));
	device.ifr_ifindex = ifindex;
	err = ioctl( sd, SIOCGIFNAME, &device );
	if (-1 == err)
	{
		GPTP_LOG_ERROR("Failed to get interface name: %s", strerror(errno));
		return false;
	}

	device.ifr_data = (char *) &hwconfig;
	memset( &hwconfig, 0, sizeof( hwconfig ));
	hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
	hwconfig.tx_type = HWTSTAMP_TX_ON;
	err = ioctl( sd, SIOCSHWTSTAMP, &device );
	if (-1 == err) 
	{
		GPTP_LOG_ERROR("Failed to configure hardware timestamping: %s",
		 strerror(errno));
		GPTP_LOG_ERROR("Attempting to configure software timestamping");
		timestamp_flags |= SOF_TIMESTAMPING_RX_SOFTWARE;
		timestamp_flags |= SOF_TIMESTAMPING_TX_SOFTWARE;

		err = setsockopt(sd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
			  sizeof(timestamp_flags) );
		if (-1 == err)
		{
			GPTP_LOG_ERROR("Failed to configure software timestamping on socket: %s",
			 strerror(errno));
			return false;
		}

	}

	timestamp_flags |= SOF_TIMESTAMPING_TX_HARDWARE;
	timestamp_flags |= SOF_TIMESTAMPING_RX_HARDWARE;
	timestamp_flags |= SOF_TIMESTAMPING_SYS_HARDWARE;
	timestamp_flags |= SOF_TIMESTAMPING_RAW_HARDWARE;
	err = setsockopt(sd, SOL_SOCKET, SO_TIMESTAMPING, &timestamp_flags,
	 sizeof(timestamp_flags));
	if (-1 == err)
	{
		GPTP_LOG_ERROR("Failed to configure hardware timestamping on socket: %s",
		 strerror(errno));
		return false;
	}

	return true;
}

#define MAX_NSEC 1000000000

/* Return *a - *b */
static inline ptp_clock_time pct_diff
( struct ptp_clock_time *a, struct ptp_clock_time *b ) {
	ptp_clock_time result;
	if( a->nsec >= b->nsec ) {
		result.nsec = a->nsec - b->nsec;
	} else {
		--a->sec;
		result.nsec = (MAX_NSEC - b->nsec) + a->nsec;
	}
	result.sec = a->sec - b->sec;

	return result;
}

static inline int64_t pctns(struct ptp_clock_time t)
{
	return t.sec * 1000000000LL + t.nsec;
}

static inline Timestamp pctTimestamp( struct ptp_clock_time *t ) {
	Timestamp result;

	result.seconds_ls = t->sec & 0xFFFFFFFF;
	result.seconds_ms = t->sec >> sizeof(result.seconds_ls)*8;
	result.nanoseconds = t->nsec;

	return result;
}

// Use HW cross-timestamp if available
bool LinuxTimestamperGeneric::HWTimestamper_gettime
( Timestamp *system_time, Timestamp *device_time, uint32_t *local_clock,
  uint32_t *nominal_clock_rate ) const
{
	if( phc_fd == -1 )
		return false;

#ifdef PTP_HW_CROSSTSTAMP
	if( precise_timestamp_enabled )
	{
		struct ptp_sys_offset_precise offset;
		memset( &offset, 0, sizeof(offset));
		if( ioctl( phc_fd, PTP_SYS_OFFSET_PRECISE, &offset ) == 0 )
		{
			*device_time = pctTimestamp( &offset.device );
			*system_time = pctTimestamp( &offset.sys_realtime );

			return true;
		}
	}
#endif

	{
		unsigned i;
		struct ptp_clock_time *pct;
		struct ptp_clock_time *system_time_l = NULL, *device_time_l = NULL;
		int64_t interval = LLONG_MAX;
		struct ptp_sys_offset offset;

		memset( &offset, 0, sizeof(offset));
		offset.n_samples = PTP_MAX_SAMPLES;
		if( ioctl( phc_fd, PTP_SYS_OFFSET, &offset ) == -1 )
			return false;

		pct = &offset.ts[0];
		for( i = 0; i < offset.n_samples; ++i ) {
			int64_t interval_t;
			interval_t = pctns(pct_diff( pct+2*i+2, pct+2*i ));
			if( interval_t < interval ) {
				system_time_l = pct+2*i;
				device_time_l = pct+2*i+1;
				interval = interval_t;
			}
		}

		if (device_time_l)
			*device_time = pctTimestamp( device_time_l );
		if (system_time_l)
			*system_time = pctTimestamp( system_time_l );
	}

	return true;
}
