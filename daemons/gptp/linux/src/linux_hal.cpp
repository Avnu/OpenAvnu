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

#include <linux_hal.hpp>
#include <sys/types.h>
#include <avbts_clock.hpp>
#include <avbts_port.hpp>

extern "C" {
#include <igb.h>
#include <pci/pci.h>
}

#define IGB_BIND_NAMESZ 24

#define TSSDP     0x003C // Time Sync SDP Configuration Register
#define FREQOUT0  0xB654 
#define TSAUXC    0xB640 
#define IGB_CTRL  0x0000
#define SYSTIMH   0xB604
#define TRGTTIML0 0xB644
#define TRGTTIMH0 0xB648

net_result LinuxNetworkInterface::send
( LinkLayerAddress *addr, uint8_t *payload, size_t length, bool timestamp ) {
	sockaddr_ll *remote = NULL;
	int err;
	remote = new struct sockaddr_ll;
	memset( remote, 0, sizeof( *remote ));
	remote->sll_family = AF_PACKET;
	remote->sll_protocol = htons( PTP_ETHERTYPE );
	remote->sll_ifindex = ifindex;
	remote->sll_halen = ETH_ALEN;
	addr->toOctetArray( remote->sll_addr );

	if( timestamp ) {
		net_lock.lock();
		err = sendto
			( sd_event, payload, length, 0, (sockaddr *) remote,
			  sizeof( *remote ));
	} else {
		err = sendto
			( sd_general, payload, length, 0, (sockaddr *) remote,
			  sizeof( *remote ));
  }
	delete remote;
	if( err == -1 ) {
		XPTPD_ERROR( "Failed to send: %s(%d)", strerror(errno), errno );
		return net_fatal;
	}
	return net_succeed;
}

net_result LinuxNetworkInterface::recv
( LinkLayerAddress *addr, uint8_t *payload, size_t &length ) {
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
  
	struct timeval timeout = { 0, 16000 }; // 16 ms

	if( !net_lock.lock( &got_net_lock )) {
		fprintf( stderr, "A Failed to lock mutex\n" );
		_exit(0);
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
			XPTPD_ERROR( "select() recv signal" );
			ret = net_trfail;
			goto done;
		} else {
			XPTPD_ERROR( "select() failed" );
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
			fprintf( stderr, "Got ENOMSG: %s:%d\n", __FILE__, __LINE__ );
			ret = net_trfail;
			goto done;
		}
		XPTPD_ERROR( "recvmsg() failed: %s", strerror(errno) );
		ret = net_fatal;
		goto done;
	}
	*addr = LinkLayerAddress( remote.sll_addr );
  
	if( err > 0 && !(payload[0] & 0x8) ) {
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
				timestamper->updateCrossStamp( &system, &device );
				timestamper->pushRXTimestamp( &device );
				break;    
			}
			cmsg = CMSG_NXTHDR(&msg,cmsg);
		}
	}
  
	length = err;

 done:
	if( !net_lock.unlock()) {
		fprintf( stderr, "A Failed to unlock, %d\n", err );
		_exit(0);
	}

	return ret;		
}

void LinuxNetworkInterface::disable_clear_rx_queue() {
	struct packet_mreq mr_8021as;
	int err;
	char buf[256];
	
	if( !net_lock.lock() ) {
		fprintf( stderr, "D rx lock failed\n" );
		_exit(0);
	}

	memset( &mr_8021as, 0, sizeof( mr_8021as ));
	mr_8021as.mr_ifindex = ifindex;
	mr_8021as.mr_type = PACKET_MR_MULTICAST;
	mr_8021as.mr_alen = 6;
	memcpy( mr_8021as.mr_address, P8021AS_MULTICAST, mr_8021as.mr_alen );
	err = setsockopt
		( sd_event, SOL_PACKET, PACKET_DROP_MEMBERSHIP, &mr_8021as,
		  sizeof( mr_8021as ));
	if( err == -1 ) {
		XPTPD_ERROR
			( "Unable to add PTP multicast addresses to port id: %u",
			  ifindex );
		return;
	}
  
	while( recvfrom( sd_event, buf, 256, MSG_DONTWAIT, NULL, 0 ) != -1 );
	
	return;
}

void LinuxNetworkInterface::reenable_rx_queue() {
	struct packet_mreq mr_8021as;
	int err;

	memset( &mr_8021as, 0, sizeof( mr_8021as ));
	mr_8021as.mr_ifindex = ifindex;
	mr_8021as.mr_type = PACKET_MR_MULTICAST;
	mr_8021as.mr_alen = 6;
	memcpy( mr_8021as.mr_address, P8021AS_MULTICAST, mr_8021as.mr_alen );
	err = setsockopt
		( sd_event, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr_8021as,
		  sizeof( mr_8021as ));
	if( err == -1 ) {
		XPTPD_ERROR
			( "Unable to add PTP multicast addresses to port id: %u",
			  ifindex );
		return;
	}

	if( !net_lock.unlock() ) {
		fprintf( stderr, "D failed unlock rx lock, %d\n", err );
	}
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
		    struct LinuxTimerQueueActionArg arg = iter->second;
			timerq->timerQueueMap.erase(iter);
			timerq->LinuxTimerQueueAction ( &arg );
			timer_delete(arg.timer_handle);
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

	if( pthread_create
		( &(ret->signal_thread), NULL, LinuxTimerQueueHandler, ret ) != 0 ) {
		delete ret;
		return NULL;
	}

	ret->stop = false;
	ret->key = 0;
	ret->lock = clock->timerQLock();

	return ret;
}

	

LinuxTimerQueue::LinuxTimerQueue() {
}

LinuxTimerQueue::~LinuxTimerQueue() {
	pthread_join(signal_thread,NULL);
}

bool LinuxTimerQueue::addEvent
( unsigned long micros, int type, ostimerq_handler func,
  event_descriptor_t * arg, bool rm, unsigned *event) {
	LinuxTimerQueueActionArg outer_arg;
	int err;
	LinuxTimerQueueMap_t::iterator iter;


	outer_arg.inner_arg = arg;
	outer_arg.rm = rm;
	outer_arg.func = func;
	outer_arg.type = type;
	outer_arg.rm = rm;

	// Find key that we can use
	while( timerQueueMap.find( key ) != timerQueueMap.end() ) {
		++key;
	}
		
	{
		struct itimerspec its;
		memset(&(outer_arg.sevp), 0, sizeof(outer_arg.sevp));
		outer_arg.sevp.sigev_notify = SIGEV_SIGNAL;
		outer_arg.sevp.sigev_signo  = SIGUSR1;
		outer_arg.sevp.sigev_value.sival_int = key;
		if ( timer_create
			 (CLOCK_MONOTONIC, &outer_arg.sevp, &outer_arg.timer_handle)
			 == -1) {
			XPTPD_ERROR("timer_create failed - %s", strerror(errno));
			exit(0);
		}
		timerQueueMap[key] = outer_arg;

		memset(&its, 0, sizeof(its));
		its.it_value.tv_sec = micros / 1000000;
		its.it_value.tv_nsec = (micros % 1000000) * 1000;
		err = timer_settime( outer_arg.timer_handle, 0, &its, NULL );
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
		if( (iter->second).type == type ) {
			// Delete element
			if( (iter->second).rm ) {
				delete (iter->second).inner_arg;
			}
			timer_delete(iter->second.timer_handle);
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

static int
pci_connect( device_t *igb_dev )
{
	struct  pci_access      *pacc;
	struct  pci_dev *dev;
	int             err;
	char    devpath[IGB_BIND_NAMESZ];

	memset( igb_dev, 0, sizeof(device_t));

	pacc    =       pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	for     (dev=pacc->devices;     dev;    dev=dev->next)
    {
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
      
		igb_dev->pci_vendor_id = dev->vendor_id;
		igb_dev->pci_device_id = dev->device_id;
		igb_dev->domain = dev->domain;
		igb_dev->bus = dev->bus;
		igb_dev->dev = dev->dev;
		igb_dev->func = dev->func;
		
		snprintf
			(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d", dev->domain,
			 dev->bus, dev->dev, dev->func );

		err = igb_probe( igb_dev );
      
		if (err) {
			continue;
		}

		printf ("attaching to %s\n", devpath);
		err = igb_attach( devpath, igb_dev );
      
		if (err) {
			printf ("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		goto out;
    }

	pci_cleanup(pacc);
	return  ENXIO;
  
out:
	pci_cleanup(pacc);
	return  0;
}


bool LinuxTimestamper::HWTimestamper_PPS_start( ) {
	unsigned tssdp;
	unsigned freqout;
	unsigned ctrl;
	unsigned tsauxc;
	unsigned trgttimh;

	if( pci_connect( &igb_dev ) != 0 ) {
		return false;
	}

	if( igb_init( &igb_dev ) != 0 ) {
		return false;
	}

	igb_initd = true;

	// Edges must be second aligned
	igb_readreg( &igb_dev, SYSTIMH, &trgttimh );
	trgttimh += 2;  // First edge in 1-2 seconds
	igb_writereg(&igb_dev, TRGTTIMH0, trgttimh );
	igb_writereg(&igb_dev, TRGTTIML0, 0 );

	freqout = 500000000;
	igb_writereg(&igb_dev, FREQOUT0, freqout );

	igb_readreg(&igb_dev, IGB_CTRL, &ctrl );
	ctrl |= 0x400000; // set bit 22 SDP0 enabling output
	igb_writereg(&igb_dev, IGB_CTRL, ctrl );
  
	igb_readreg( &igb_dev, TSAUXC, &tsauxc );
	tsauxc |= 0x14;
	igb_writereg( &igb_dev, TSAUXC, tsauxc );

	igb_readreg(&igb_dev, TSSDP, &tssdp);
	tssdp &= ~0x40; // Set SDP0 output to freq clock 0
	tssdp |= 0x80; 
	igb_writereg(&igb_dev, TSSDP, tssdp);
  
	igb_readreg(&igb_dev, TSSDP, &tssdp);
	tssdp |= 0x100; // set bit 8 -> SDP0 Time Sync Output
	igb_writereg(&igb_dev, TSSDP, tssdp);
  
	return true;
}

bool LinuxTimestamper::HWTimestamper_PPS_stop() {
	unsigned tssdp;
	
	if( !igb_initd ) return false;
	
	igb_readreg(&igb_dev, TSSDP, &tssdp);
	tssdp &= ~0x100; // set bit 8 -> SDP0 Time Sync Output
	igb_writereg(&igb_dev, TSSDP, tssdp);
	
	return true;
}
