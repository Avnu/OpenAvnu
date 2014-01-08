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

#include <ieee1588.hpp>

#include <avbts_port.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>
#include <time.h>

LinkLayerAddress IEEE1588Port::other_multicast(OTHER_MULTICAST);
LinkLayerAddress IEEE1588Port::pdelay_multicast(PDELAY_MULTICAST);

OSThreadExitCode openPortWrapper(void *arg)
{
	IEEE1588Port *port;

	port = (IEEE1588Port *) arg;
	if (port->openPort() == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

IEEE1588Port::~IEEE1588Port()
{
	delete port_ready_condition;
	delete [] rate_offset_array;
	if( qualified_announce != NULL ) delete qualified_announce;
}

IEEE1588Port::IEEE1588Port
(IEEE1588Clock * clock, uint16_t index, bool forceSlave,
 int accelerated_sync_count, HWTimestamper * timestamper, int32_t offset,
 InterfaceLabel * net_label, OSConditionFactory * condition_factory,
 OSThreadFactory * thread_factory, OSTimerFactory * timer_factory,
 OSLockFactory * lock_factory)
{
	sync_sequence_id = 0;

	clock->registerPort(this, index);
	this->clock = clock;
	ifindex = index;

	this->forceSlave = forceSlave;
	port_state = PTP_INITIALIZING;

	asCapable = false;

	announce_sequence_id = 0;
	sync_sequence_id = 0;
	pdelay_sequence_id = 0;

	sync_sequence_id = 0;

	pdelay_started = false;

	log_mean_sync_interval = -3;
	_accelerated_sync_count = accelerated_sync_count;
	log_mean_announce_interval = 0;
	log_min_mean_pdelay_req_interval = 0;

	_current_clock_offset = _initial_clock_offset = offset;

	rate_offset_array = NULL;

	_hw_timestamper = timestamper;

	one_way_delay = 3600000000000;

	_peer_rate_offset = 1.0;

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	qualified_announce = NULL;

	this->net_label = net_label;

	this->timer_factory = timer_factory;
	this->thread_factory = thread_factory;

	this->condition_factory = condition_factory;
	this->lock_factory = lock_factory;

	pdelay_count = 0;
	sync_count = 0;
}

bool IEEE1588Port::init_port()
{
	if (!OSNetworkInterfaceFactory::buildInterface
	    (&net_iface, factory_name_t("default"), net_label, _hw_timestamper))
		return false;

	this->net_iface = net_iface;
	this->net_iface->getLinkLayerAddress(&local_addr);
	clock->setClockIdentity(&local_addr);

	if( _hw_timestamper != NULL ) {
		if( !_hw_timestamper->HWTimestamper_init( net_label, net_iface )) {
			XPTPD_ERROR
				( "Failed to initialize hardware timestamper, "
				  "falling back to software timestamping" );
			_hw_timestamper = NULL;
		}
	}

	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	port_tx_lock = lock_factory->createLock(oslock_recursive);

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&ifindex);

	port_ready_condition = condition_factory->createCondition();

	return true;
}

void IEEE1588Port::startPDelay() {
	pdelay_started = true;
	clock->addEventTimer( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, 32000000 );
}

void IEEE1588Port::startAnnounce() {
	clock->addEventTimer( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
}

bool IEEE1588Port::serializeState( void *buf, off_t *count ) {
	bool ret = true;
	
	if( buf == NULL ) {
		*count = sizeof(port_state)+sizeof(_peer_rate_offset)+
			sizeof(asCapable)+sizeof(one_way_delay);
		return true;
	}

  if( port_state != PTP_MASTER && port_state != PTP_SLAVE ) {
    *count = 0;
    ret = false;
    goto bail;
  }

  /* asCapable */
  if( ret && *count >= (off_t) sizeof( asCapable )) {
    memcpy( buf, &asCapable, sizeof( asCapable ));
    *count -= sizeof( asCapable );
    buf = ((char *)buf) + sizeof( asCapable );
  } else if( ret == false ) {
    *count += sizeof( asCapable );
  } else {
    *count = sizeof( asCapable )-*count;
    ret = false;
  }

  /* Port State */
  if( ret && *count >= (off_t) sizeof( port_state )) {
    memcpy( buf, &port_state, sizeof( port_state ));
    *count -= sizeof( port_state );
    buf = ((char *)buf) + sizeof( port_state );
  } else if( ret == false ) {
    *count += sizeof( port_state );
  } else {
    *count = sizeof( port_state )-*count;
    ret = false;
  }

  /* Link Delay */
  if( ret && *count >= (off_t) sizeof( one_way_delay )) {
    memcpy( buf, &one_way_delay, sizeof( one_way_delay ));
    *count -= sizeof( one_way_delay );
    buf = ((char *)buf) + sizeof( one_way_delay );
  } else if( ret == false ) {
    *count += sizeof( one_way_delay );
  } else {
    *count = sizeof( one_way_delay )-*count;
    ret = false;
  }

  /* Neighbor Rate Ratio */
  if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
    memcpy( buf, &_peer_rate_offset, sizeof( _peer_rate_offset ));
    *count -= sizeof( _peer_rate_offset );
    buf = ((char *)buf) + sizeof( _peer_rate_offset );
  } else if( ret == false ) {
    *count += sizeof( _peer_rate_offset );
  } else {
    *count = sizeof( _peer_rate_offset )-*count;
    ret = false;
  }

 bail:
  return ret;
}

bool IEEE1588Port::restoreSerializedState( void *buf, off_t *count ) {
  bool ret = true;
  
  /* asCapable */
  if( ret && *count >= (off_t) sizeof( asCapable )) {
    memcpy( &asCapable, buf, sizeof( asCapable ));
    *count -= sizeof( asCapable );
    buf = ((char *)buf) + sizeof( asCapable );
  } else if( ret == false ) {
    *count += sizeof( asCapable );
  } else {
    *count = sizeof( asCapable )-*count;
    ret = false;
  }

  /* Port State */
  if( ret && *count >= (off_t) sizeof( port_state )) {
    memcpy( &port_state, buf, sizeof( port_state ));
    *count -= sizeof( port_state );
    buf = ((char *)buf) + sizeof( port_state );
  } else if( ret == false ) {
    *count += sizeof( port_state );
  } else {
    *count = sizeof( port_state )-*count;
    ret = false;
  }

  /* Link Delay */
  if( ret && *count >= (off_t) sizeof( one_way_delay )) {
    memcpy( &one_way_delay, buf, sizeof( one_way_delay ));
    *count -= sizeof( one_way_delay );
    buf = ((char *)buf) + sizeof( one_way_delay );
  } else if( ret == false ) {
    *count += sizeof( one_way_delay );
  } else {
    *count = sizeof( one_way_delay )-*count;
    ret = false;
  }

  /* Neighbor Rate Ratio */
  if( ret && *count >= (off_t) sizeof( _peer_rate_offset )) {
    memcpy( &_peer_rate_offset, buf, sizeof( _peer_rate_offset ));
    *count -= sizeof( _peer_rate_offset );
    buf = ((char *)buf) + sizeof( _peer_rate_offset );
  } else if( ret == false ) {
    *count += sizeof( _peer_rate_offset );
  } else {
    *count = sizeof( _peer_rate_offset )-*count;
    ret = false;
  }

  return ret;
}

void *IEEE1588Port::openPort(void)
{
	port_ready_condition->signal();

	while (1) {
		PTPMessageCommon *msg;
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);

		if ((rrecv = net_iface->recv(&remote, buf, length)) == net_succeed) {
			XPTPD_INFO("Processing network buffer");
			msg = buildPTPMessage((char *)buf, (int)length, &remote,
					    this);
			if (msg != NULL) {
				XPTPD_INFO("Processing message");
				msg->processMessage(this);
				if (msg->garbage()) {
					delete msg;
				}
			} else {
				XPTPD_ERROR("Discarding invalid message");
			}
		} else if (rrecv == net_fatal) {
			XPTPD_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			break;
		}
	}

	return NULL;
}

net_result IEEE1588Port::port_send(uint8_t * buf, int size,
				   MulticastType mcast_type,
				   PortIdentity * destIdentity, bool timestamp)
{
	LinkLayerAddress dest;

	if (mcast_type != MCAST_NONE) {
		if (mcast_type == MCAST_PDELAY) {
			dest = pdelay_multicast;
		} else {
			dest = other_multicast;
		}
	} else {
		mapSocketAddr(destIdentity, &dest);
	}

	return net_iface->send(&dest, (uint8_t *) buf, size, timestamp);
}

unsigned IEEE1588Port::getPayloadOffset()
{
	return net_iface->getPayloadOffset();
}

void IEEE1588Port::sendEventPort(uint8_t * buf, int size,
				 MulticastType mcast_type,
				 PortIdentity * destIdentity)
{
	net_result rtx = port_send(buf, size, mcast_type, destIdentity, true);
	if (rtx != net_succeed) {
		XPTPD_ERROR("sendEventPort(): failure");
	}

	return;
}

void IEEE1588Port::sendGeneralPort(uint8_t * buf, int size,
				   MulticastType mcast_type,
				   PortIdentity * destIdentity)
{
	net_result rtx = port_send(buf, size, mcast_type, destIdentity, false);
	if (rtx != net_succeed) {
		XPTPD_ERROR("sendGeneralPort(): failure");
	}

	return;
}

void IEEE1588Port::processEvent(Event e)
{
	bool changed_external_master;
	OSTimer *timer = timer_factory->createTimer();
	
	switch (e) {
	case POWERUP:
	case INITIALIZE:
		XPTPD_INFO("Received POWERUP/INITIALIZE event");
		clock->getTimerQLock();

		{
			unsigned long long interval3;
			unsigned long long interval4;
			Event e3 = NULL_EVENT;
			Event e4 = NULL_EVENT;
			
			if( port_state != PTP_MASTER ) {
				_accelerated_sync_count = -1;
			}
			
			if( port_state != PTP_SLAVE && port_state != PTP_MASTER ) {
				fprintf( stderr, "Starting PDelay\n" );
				startPDelay();
			}

			if( clock->getPriority1() == 255 || port_state == PTP_SLAVE ) {
				becomeSlave( false );
			} else if( port_state == PTP_MASTER ) {
				becomeMaster( true );
			} else {
				//e3 = SYNC_RECEIPT_TIMEOUT_EXPIRES;
				e4 = ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES;
				interval3 = (unsigned long long)
					(SYNC_RECEIPT_TIMEOUT_MULTIPLIER*
					 pow((double)2,getSyncInterval())*1000000000.0);
				interval4 = (unsigned long long)
					(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
					 pow((double)2,getAnnounceInterval())*1000000000.0);
			}
      
			port_ready_condition->wait_prelock();
			listening_thread = thread_factory->createThread();
			if (!listening_thread->
				start (openPortWrapper, (void *)this))
			{
				XPTPD_ERROR("Error creating port thread");
				return;
			}
			port_ready_condition->wait();
			
			if (e3 != NULL_EVENT)
				clock->addEventTimer(this, e3, interval3);
			if (e4 != NULL_EVENT)
				clock->addEventTimer(this, e4, interval4);
		}
		
		clock->putTimerQLock();

		break;
	case STATE_CHANGE_EVENT:
		if ( clock->getPriority1() != 255 ) {
			int number_ports, j;
			PTPMessageAnnounce *EBest = NULL;
			char EBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];

			IEEE1588Port **ports;
			clock->getPortList(number_ports, ports);

			

			/* Find EBest for all ports */
			j = 0;
			for (int i = 0; i < number_ports; ++i) {
				while (ports[j] == NULL)
					++j;
				if (ports[j]->port_state == PTP_DISABLED
				    || ports[j]->port_state == PTP_FAULTY) {
					continue;
				}
				if (EBest == NULL) {
					EBest = ports[j]->calculateERBest();
				} else {
					if (ports[j]->calculateERBest()->isBetterThan(EBest)) {
						EBest = ports[j]->calculateERBest();
					}
				}
			}

			/* Check if we've changed */
			{
			  
			  uint8_t LastEBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
			  clock->getLastEBestIdentity().
				  getIdentityString( LastEBestClockIdentity );
			  EBest->getGrandmasterIdentity( EBestClockIdentity );
			  if( memcmp
				  ( EBestClockIdentity, LastEBestClockIdentity,
					PTP_CLOCK_IDENTITY_LENGTH ) != 0 )
			  {
				  ClockIdentity newGM;
				  changed_external_master = true;
				  newGM.set((uint8_t *) EBestClockIdentity );
				  clock->setLastEBestIdentity( newGM );
			  } else {
				  changed_external_master = false;
			  }
			}
			
			if( clock->isBetterThan( EBest )) {
				// We're Grandmaster, set grandmaster info to me
				ClockIdentity clock_identity;
				unsigned char priority1;
				unsigned char priority2;
				ClockQuality clock_quality;
			  
				clock_identity = getClock()->getClockIdentity();
				getClock()->setGrandmasterClockIdentity( clock_identity );
				priority1 = getClock()->getPriority1();
				getClock()->setGrandmasterPriority1( priority1 );
				priority2 = getClock()->getPriority2();
				getClock()->setGrandmasterPriority2( priority2 );
				clock_quality = getClock()->getClockQuality();
				getClock()->setGrandmasterClockQuality( clock_quality );
			}

			j = 0;
			for (int i = 0; i < number_ports; ++i) {
				while (ports[j] == NULL)
					++j;
				if (ports[j]->port_state == PTP_DISABLED
				    || ports[j]->port_state == PTP_FAULTY) {
					continue;
				}
				if (clock->isBetterThan(EBest)) {
					// We are the GrandMaster, all ports are master
					EBest = NULL;	// EBest == NULL : we were grandmaster
					ports[j]->recommendState(PTP_MASTER,
								 changed_external_master);
				} else {
					if( EBest == ports[j]->calculateERBest() ) {
						// The "best" Announce was recieved on this port
						ClockIdentity clock_identity;
						unsigned char priority1;
						unsigned char priority2;
						ClockQuality *clock_quality;
						
						ports[j]->recommendState
							( PTP_SLAVE, changed_external_master );
						
						clock_identity = EBest->getGrandmasterClockIdentity();
						getClock()->setGrandmasterClockIdentity(clock_identity);
						priority1 = EBest->getGrandmasterPriority1();
						getClock()->setGrandmasterPriority1( priority1 );
						priority2 = EBest->getGrandmasterPriority2();
						getClock()->setGrandmasterPriority2( priority2 );
						clock_quality = EBest->getGrandmasterClockQuality();
						getClock()->setGrandmasterClockQuality(*clock_quality);
					} else {
						/* Otherwise we are the master because we have 
						   sync'd to a better clock */
						ports[j]->recommendState
							(PTP_MASTER, changed_external_master);
					}
				}
			}
		}
		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		{
			if( clock->getPriority1() == 255 ) {
				// Restart timer
				if( e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ) {
					clock->addEventTimer
						(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
						 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
						  (unsigned long long)
						  (pow((double)2,getAnnounceInterval())*
						   1000000000.0)));
				} else {
					clock->addEventTimer
						(this, SYNC_RECEIPT_TIMEOUT_EXPIRES,
						 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER*
						  (unsigned long long)
						  (pow((double)2,getSyncInterval())*
						   1000000000.0)));
				}
				return;
			}
			if (port_state == PTP_INITIALIZING
			    || port_state == PTP_UNCALIBRATED
			    || port_state == PTP_SLAVE
			    || port_state == PTP_PRE_MASTER) {
				fprintf
					(stderr,
					 "*** %s Timeout Expired - Becoming Master\n", 
					 e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ? "Announce" :
					 "Sync" );
				{
				  // We're Grandmaster, set grandmaster info to me
				  ClockIdentity clock_identity;
				  unsigned char priority1;
				  unsigned char priority2;
				  ClockQuality clock_quality;
				  
				  clock_identity = getClock()->getClockIdentity();
				  getClock()->setGrandmasterClockIdentity( clock_identity );
				  priority1 = getClock()->getPriority1();
				  getClock()->setGrandmasterPriority1( priority1 );
				  priority2 = getClock()->getPriority2();
				  getClock()->setGrandmasterPriority2( priority2 );
				  clock_quality = getClock()->getClockQuality();
				  getClock()->setGrandmasterClockQuality( clock_quality );
				}
				port_state = PTP_MASTER;
				Timestamp system_time;
				Timestamp device_time;

				uint32_t local_clock, nominal_clock_rate;

				getDeviceTime(system_time, device_time,
					      local_clock, nominal_clock_rate);

				(void) clock->calcLocalSystemClockRateDifference
				  ( device_time, system_time );

				delete qualified_announce;
				qualified_announce = NULL;

				// Add timers for Announce and Sync, this is as close to immediately as we get
				clock->addEventTimer
					( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
				startAnnounce();
					// 
			}
		}

		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
			XPTPD_INFO("PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
			{
				int ts_good;
				Timestamp req_timestamp;
				int iter = TX_TIMEOUT_ITER;
				long req = TX_TIMEOUT_BASE;
				unsigned req_timestamp_counter_value;
				long long wait_time = 0;

				PTPMessagePathDelayReq *pdelay_req =
				    new PTPMessagePathDelayReq(this);
				PortIdentity dest_id;
				getPortIdentity(dest_id);
				pdelay_req->setPortIdentity(&dest_id);
				
				{
					Timestamp pending =
					    PDELAY_PENDING_TIMESTAMP;
					pdelay_req->setTimestamp(pending);
				}

				if (last_pdelay_req != NULL) {
					delete last_pdelay_req;
				}
				setLastPDelayReq(pdelay_req);

				getTxLock();
				pdelay_req->sendPort(this, NULL);
				XPTPD_INFO("Sent PDelay Request");

				ts_good =
				    getTxTimestamp
					(pdelay_req, req_timestamp, req_timestamp_counter_value,
					 false);
				while (ts_good != 0 && iter-- != 0) {
					timer->sleep(req);
					wait_time += req;
					if (ts_good != -72 && iter < 1)
						fprintf
							(stderr,
							 "Error (TX) timestamping PDelay request "
							 "(Retrying-%d), error=%d\n", iter, ts_good);
					ts_good =
					    getTxTimestamp
						(pdelay_req, req_timestamp,
						 req_timestamp_counter_value, iter == 0);
					req *= 2;
				}
				putTxLock();

				if (ts_good == 0) {
					pdelay_req->setTimestamp(req_timestamp);
				} else {
				  Timestamp failed = INVALID_TIMESTAMP;
				  pdelay_req->setTimestamp(failed);
				 fprintf( stderr, "Invalid TX\n" );
				}

				if (ts_good != 0) {
					char msg
					    [HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
					getExtendedError(msg);
					XPTPD_ERROR(
						"Error (TX) timestamping PDelay request, error=%d\t%s",
						ts_good, msg);
				}
#ifdef DEBUG
				if (ts_good == 0) {
					XPTPD_INFO
					    ("Successful PDelay Req timestamp, %u,%u",
					     req_timestamp.seconds_ls,
					     req_timestamp.nanoseconds);
				} else {
					XPTPD_INFO
					    ("*** Unsuccessful PDelay Req timestamp");
				}
#endif

				{
					long long timeout;
					long long interval;

					timeout = PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER *
						((long long)
						 (pow((double)2,getPDelayInterval())*1000000000.0)) -
						wait_time*1000;
					timeout = timeout > EVENT_TIMER_GRANULARITY ?
						timeout : EVENT_TIMER_GRANULARITY;
					clock->addEventTimer
						(this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, timeout );
					
					interval = 
						((long long)
						 (pow((double)2,getPDelayInterval())*1000000000.0)) -
						wait_time*1000;
					interval = interval > EVENT_TIMER_GRANULARITY ?
						interval : EVENT_TIMER_GRANULARITY;
					clock->addEventTimer
						(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, interval );
				}
			}
			break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
			XPTPD_INFO("SYNC_INTERVAL_TIMEOUT_EXPIRES occured");
			{
				/* Set offset from master to zero, update device vs
				   system time offset */
				Timestamp system_time;
				Timestamp device_time;
				FrequencyRatio local_system_freq_offset;
				int64_t local_system_offset;
				long long wait_time = 0;
				
				uint32_t local_clock, nominal_clock_rate;

				// Send a sync message and then a followup to broadcast
				if (asCapable) {
					PTPMessageSync *sync = new PTPMessageSync(this);
					PortIdentity dest_id;
					getPortIdentity(dest_id);
					sync->setPortIdentity(&dest_id);
					getTxLock();
					sync->sendPort(this, NULL);
					XPTPD_INFO("Sent SYNC message");
					
					int ts_good;
					Timestamp sync_timestamp;
					unsigned sync_timestamp_counter_value;
					int iter = TX_TIMEOUT_ITER;
					long req = TX_TIMEOUT_BASE;
					ts_good =
						getTxTimestamp(sync, sync_timestamp,
									   sync_timestamp_counter_value,
									   false);
					while (ts_good != 0 && iter-- != 0) {
						timer->sleep(req);
						wait_time += req;

						if (ts_good != -72 && iter < 1)
							XPTPD_ERROR(
								"Error (TX) timestamping Sync (Retrying), "
								"error=%d", ts_good);
						ts_good =
							getTxTimestamp
							(sync, sync_timestamp, 
							 sync_timestamp_counter_value, iter == 0);
						req *= 2;
					}
					putTxLock();
					
					if (ts_good != 0) {
						char msg
							[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
						getExtendedError(msg);
						fprintf
							(stderr,
							 "Error (TX) timestamping Sync, error="
							 "%d\n%s",
							 ts_good, msg );
 					}

					if (ts_good == 0) {
						XPTPD_INFO("Successful Sync timestamp");
						XPTPD_INFO("Seconds: %u",
								   sync_timestamp.seconds_ls);
						XPTPD_INFO("Nanoseconds: %u",
								   sync_timestamp.nanoseconds);
					} else {
						XPTPD_INFO
							("*** Unsuccessful Sync timestamp");
					}
					
					PTPMessageFollowUp *follow_up;
					if (ts_good == 0) {
						follow_up =
							new PTPMessageFollowUp(this);
						PortIdentity dest_id;
						getPortIdentity(dest_id);
						follow_up->setPortIdentity(&dest_id);
						follow_up->setSequenceId(sync->getSequenceId());
						follow_up->setPreciseOriginTimestamp(sync_timestamp);
						follow_up->sendPort(this, NULL);
						delete follow_up;
					} else {
					}
					delete sync;
				}
				/* Do getDeviceTime() after transmitting sync frame
				   causing an update to local/system timestamp */
				getDeviceTime
					(system_time, device_time, local_clock, nominal_clock_rate);
				
				XPTPD_INFO
					("port::processEvent(): System time: %u,%u Device Time: %u,%u",
					 system_time.seconds_ls, system_time.nanoseconds,
					 device_time.seconds_ls, device_time.nanoseconds);
				
				local_system_offset =
			    TIMESTAMP_TO_NS(system_time) -
			    TIMESTAMP_TO_NS(device_time);
			  local_system_freq_offset =
			    clock->calcLocalSystemClockRateDifference
			    ( device_time, system_time );
			  clock->setMasterOffset
				  (0, device_time, 1.0, local_system_offset,
				   system_time, local_system_freq_offset, sync_count,
				   pdelay_count, port_state );

			  /* If accelerated_sync is non-zero then start 16 ms sync
				 timer, subtract 1, for last one start PDelay also */
			  if( _accelerated_sync_count > 0 ) {
				  clock->addEventTimer
					  ( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 8000000 );
				  --_accelerated_sync_count;
			  } else {
				  syncDone();
				  if( _accelerated_sync_count == 0 ) {
					  --_accelerated_sync_count;
				  }
				  wait_time *= 1000; // to ns
				  wait_time =
					  ((long long)
					   (pow((double)2,getSyncInterval())*1000000000.0)) -
					  wait_time;
				  wait_time = wait_time > EVENT_TIMER_GRANULARITY ? wait_time :
					  EVENT_TIMER_GRANULARITY;
				  clock->addEventTimer
					  ( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, wait_time );
			  }
			  
			}
			break;
	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		if (asCapable) {
			// Send an announce message
			PTPMessageAnnounce *annc = new PTPMessageAnnounce(this);
			PortIdentity dest_id;
			PortIdentity gmId;
			ClockIdentity clock_id = clock->getClockIdentity();
			gmId.setClockIdentity(clock_id);
			getPortIdentity(dest_id);
			annc->setPortIdentity(&dest_id);
			annc->sendPort(this, NULL);
			delete annc;
		}
		clock->addEventTimer
			(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
			 (unsigned)
			 (pow ((double)2, getAnnounceInterval()) * 1000000000.0));
		break;
	case FAULT_DETECTED:
		XPTPD_INFO("Received FAULT_DETECTED event");
		break;
	case PDELAY_DEFERRED_PROCESSING:
		pdelay_rx_lock->lock();
		if (last_pdelay_resp_fwup == NULL) {
			fprintf(stderr, "PDelay Response Followup is NULL!\n");
			abort();
		}
		last_pdelay_resp_fwup->processMessage(this);
		if (last_pdelay_resp_fwup->garbage()) {
			delete last_pdelay_resp_fwup;
			this->setLastPDelayRespFollowUp(NULL);
		}
		pdelay_rx_lock->unlock();
		break;
	case PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES:
		setAsCapable(false);
		pdelay_count = 0;
		break;
	default:
		XPTPD_INFO
		    ("Unhandled event type in IEEE1588Port::processEvent(), %d",
		     e);
		break;
	}

	delete timer;
	return;
}

PTPMessageAnnounce *IEEE1588Port::calculateERBest(void)
{
	return qualified_announce;
}

void IEEE1588Port::recoverPort(void)
{
	return;
}

IEEE1588Clock *IEEE1588Port::getClock(void)
{
	return clock;
}

void IEEE1588Port::getDeviceTime
(Timestamp & system_time, Timestamp & device_time,
 uint32_t & local_clock, uint32_t & nominal_clock_rate)
{
	if (_hw_timestamper) {
		_hw_timestamper->HWTimestamper_gettime
			(&system_time, &device_time, &local_clock, &nominal_clock_rate);
	} else {
		device_time = system_time = clock->getSystemTime();
		local_clock = nominal_clock_rate = 0;
	}
	return;
}

void IEEE1588Port::becomeMaster( bool annc ) {
  port_state = PTP_MASTER;
  // Start announce receipt timeout timer
  // Start sync receipt timeout timer
  clock->deleteEventTimer( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES );
  clock->deleteEventTimer( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
  if( annc ) {
    startAnnounce();
  }
  clock->addEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
  fprintf( stderr, "Switching to Master\n" );

  return;
}

void IEEE1588Port::becomeSlave( bool restart_syntonization ) {
  clock->deleteEventTimer( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
  clock->deleteEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

  port_state = PTP_SLAVE;

  /*clock->addEventTimer
	  ( this, SYNC_RECEIPT_TIMEOUT_EXPIRES,
		(SYNC_RECEIPT_TIMEOUT_MULTIPLIER*
		 (unsigned long long)(pow((double)2,getSyncInterval())*1000000000.0)));*/
  clock->addEventTimer
	  (this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
	   (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
		(unsigned long long)
		(pow((double)2,getAnnounceInterval())*1000000000.0)));
  fprintf( stderr, "Switching to Slave\n" );
  if( restart_syntonization ) clock->newSyntonizationSetPoint();
  
  return;
}

void IEEE1588Port::recommendState
( PortState state, bool changed_external_master )
{
	bool reset_sync = false;
	switch (state) {
	case PTP_MASTER:
		if (port_state != PTP_MASTER) {
			port_state = PTP_MASTER;
			// Start announce receipt timeout timer
			// Start sync receipt timeout timer
			becomeMaster( true );
			reset_sync = true;
		}
		break;
	case PTP_SLAVE:
		if (port_state != PTP_SLAVE) {
			becomeSlave( true );
			reset_sync = true;
		} else {
		  if( changed_external_master ) {
		    fprintf( stderr, "Changed master!\n" );
		    clock->newSyntonizationSetPoint();
			reset_sync = true;
		  }
		}
		break;
	default:
		XPTPD_INFO
		    ("Invalid state change requested by call to "
			 "1588Port::recommendState()");
		break;
	}
	if( reset_sync ) sync_count = 0;
	return;
 }

void IEEE1588Port::mapSocketAddr
(PortIdentity * destIdentity, LinkLayerAddress * remote)
{
	*remote = identity_map[*destIdentity];
	return;
}

void IEEE1588Port::addSockAddrMap
(PortIdentity * destIdentity, LinkLayerAddress * remote)
{
	identity_map[*destIdentity] = *remote;
	return;
}

int IEEE1588Port::getTxTimestamp
(PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
 bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getTxTimestamp
		(&identity, msg->getSequenceId(), timestamp, counter_value, last);
}

int IEEE1588Port::getRxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getRxTimestamp
		(&identity, msg->getSequenceId(), timestamp, counter_value, last);
}

int IEEE1588Port::getTxTimestamp(PortIdentity * sourcePortIdentity,
				 uint16_t sequenceId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper) {
		return _hw_timestamper->HWTimestamper_txtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return 0;
}

int IEEE1588Port::getRxTimestamp(PortIdentity * sourcePortIdentity,
				 uint16_t sequenceId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper) {
		return _hw_timestamper->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return 0;
}
