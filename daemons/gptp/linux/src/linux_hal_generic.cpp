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
#include <platform.hpp>
#include <avbts_message.hpp>
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

#define TX_PHY_TIME 184
#define RX_PHY_TIME 382

net_result LinuxNetworkInterface::nrecv
( LinkLayerAddress *addr, uint8_t *payload, size_t &length )
{
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
	net_result ret = net_succeed;
	bool got_net_lock;

	LinuxTimestamperGeneric *gtimestamper;

	struct timeval timeout = { 0, 16000 }; // 16 ms

	if( !net_lock.lock( &got_net_lock )) {
		GPTP_LOG_ERROR("A Failed to lock mutex");
		return net_fatal;
	}
	if( !got_net_lock ) {
		return net_trfail;
	}

	FD_ZERO( &readfds );
	FD_SET( sd_event, &readfds );

	err = select( sd_event+1, &readfds, NULL, NULL, &timeout );
	if( err == 0 ) {
		ret = net_trfail;
		goto done;
	} else if( err == -1 ) {
		if( err == EINTR ) {
			// Caught signal
			GPTP_LOG_ERROR("select() recv signal");
			ret = net_trfail;
			goto done;
		} else {
			GPTP_LOG_ERROR("select() failed");
			ret = net_fatal;
			goto done;
    }
	} else if( !FD_ISSET( sd_event, &readfds )) {
		ret = net_trfail;
		goto done;
	}

	memset( &msg, 0, sizeof( msg ));

	msg.msg_iov = &sgentry;
	msg.msg_iovlen = 1;

	sgentry.iov_base = payload;
	sgentry.iov_len = length;

	memset( &remote, 0, sizeof(remote));
	msg.msg_name = (caddr_t) &remote;
	msg.msg_namelen = sizeof( remote );
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	err = recvmsg( sd_event, &msg, 0 );
	if( err < 0 ) {
		if( errno == ENOMSG ) {
			GPTP_LOG_ERROR("Got ENOMSG: %s:%d", __FILE__, __LINE__);
			ret = net_trfail;
			goto done;
		}
		GPTP_LOG_ERROR("recvmsg() failed: %s", strerror(errno));
		ret = net_fatal;
		goto done;
	}
	*addr = LinkLayerAddress( remote.sll_addr );

	gtimestamper = dynamic_cast<LinuxTimestamperGeneric *>(timestamper);
	if( err > 0 && !(payload[0] & 0x8) && gtimestamper != NULL ) {
		/* Retrieve the timestamp */
		cmsg = CMSG_FIRSTHDR(&msg);
		while( cmsg != NULL ) {
			if
				( cmsg->cmsg_level == SOL_SOCKET &&
				  cmsg->cmsg_type == SO_TIMESTAMPING ) {
				struct timespec *ts_device, *ts_system;
				Timestamp device, system;
				ts_system = ((struct timespec *) CMSG_DATA(cmsg)) + 1;
				system = tsToTimestamp( ts_system );
				ts_device = ts_system + 1; device = tsToTimestamp( ts_device );
				gtimestamper->pushRXTimestamp( &device );
				break;
			}
			cmsg = CMSG_NXTHDR(&msg,cmsg);
		}
	}

	length = err;

 done:
	if( !net_lock.unlock()) {
		GPTP_LOG_ERROR("A Failed to unlock, %d", err);
		return net_fatal;
	}

	return ret;
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

	return info.phc_index;
}

LinuxTimestamperGeneric::~LinuxTimestamperGeneric() {
	if( _private != NULL ) delete _private;
#ifdef WITH_IGBLIB
	if( igb_private != NULL ) delete igb_private;
#endif
}

LinuxTimestamperGeneric::LinuxTimestamperGeneric() {
	_private = NULL;
#ifdef WITH_IGBLIB
	igb_private = NULL;
#endif
	sd = -1;
}

bool LinuxTimestamperGeneric::Adjust( void *tmx ) const {
	if( syscall(__NR_clock_adjtime, _private->clockid, tmx ) != 0 ) {
		GPTP_LOG_ERROR("Failed to adjust PTP clock rate");
		return false;
	}
	return true;
}

bool LinuxTimestamperGeneric::HWTimestamper_init
( InterfaceLabel *iface_label, OSNetworkInterface *iface ) {
	cross_stamp_good = false;
	int phc_index;
	char ptp_device[] = PTP_DEVICE;
#ifdef PTP_HW_CROSSTSTAMP
	struct ptp_clock_caps ptp_capability;
#endif
	_private = new LinuxTimestamperGenericPrivate;

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
( PortIdentity *identity, PTPMessageId messageId, Timestamp &timestamp,
  unsigned &clock_value, bool last )
{
	int err;
	int ret = GPTP_EC_EAGAIN;
	struct msghdr msg;
	struct cmsghdr *cmsg;
	struct sockaddr_ll remote;
	struct iovec sgentry;
	PTPMessageId reflectedMessageId;
	uint8_t reflected_bytes[ETHER_HDR_LEN + PTP_COMMON_HDR_LENGTH];
	uint8_t *gptpCommonHeader;
	uint16_t sequenceId;
	struct {
		struct cmsghdr cm;
		char control[256];
	} control;

    if( sd == -1 ) return -1;
	memset( &msg, 0, sizeof( msg ));

	msg.msg_iov = &sgentry;
	msg.msg_iovlen = 1;

	sgentry.iov_base = reflected_bytes;
	sgentry.iov_len = sizeof(reflected_bytes);

	gptpCommonHeader = reflected_bytes + ETHER_HDR_LEN;

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
	sequenceId = PLAT_ntohs(*((uint16_t*)(PTP_COMMON_HDR_SEQUENCE_ID(gptpCommonHeader))));
	reflectedMessageId.setSequenceId(sequenceId);
	reflectedMessageId.setMessageType((MessageType)(*PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(gptpCommonHeader) & 0xF));
	if (messageId != reflectedMessageId) {
		GPTP_LOG_WARNING("Timestamp discarded due to wrong message id");
		ret = GPTP_EC_EAGAIN;
		goto done;
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

	memset( &device, 0, sizeof(device));
	device.ifr_ifindex = ifindex;
	err = ioctl( sd, SIOCGIFNAME, &device );
	if( err == -1 ) {
		GPTP_LOG_ERROR
			("Failed to get interface name: %s", strerror(errno));
		return false;
	}

	device.ifr_data = (char *) &hwconfig;
	memset( &hwconfig, 0, sizeof( hwconfig ));
	hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
	hwconfig.tx_type = HWTSTAMP_TX_ON;
	err = ioctl( sd, SIOCSHWTSTAMP, &device );
	if( err == -1 ) {
		GPTP_LOG_ERROR
			("Failed to configure timestamping: %s", strerror(errno));
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
		GPTP_LOG_ERROR
			("Failed to configure timestamping on socket: %s",
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
