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
}

IEEE1588Port::IEEE1588Port(IEEE1588Clock * clock, uint16_t index,
			   bool forceSlave, HWTimestamper * timestamper,
			   bool syntonize, int32_t offset,
			   InterfaceLabel * net_label,
			   OSConditionFactory * condition_factory,
			   OSThreadFactory * thread_factory,
			   OSTimerFactory * timer_factory,
			   OSLockFactory * lock_factory)
{
	sync_sequence_id = 0;
	last_pdelay_req = NULL;

	clock->registerPort(this, index);
	this->clock = clock;
	ifindex = index;

	this->forceSlave = forceSlave;

	asCapable = false;

	announce_sequence_id = 0;
	sync_sequence_id = 0;
	pdelay_sequence_id = 0;

	sync_sequence_id = 0;

	log_mean_sync_interval = -3;
	log_mean_announce_interval = 0;
	log_min_mean_pdelay_req_interval = 0;

	_current_clock_offset = _initial_clock_offset = offset;

	rate_offset_array = NULL;

	_hw_timestamper = timestamper;

	if (_hw_timestamper != NULL) {
		if (!_hw_timestamper->HWTimestamper_init(net_label)) {
			XPTPD_ERROR
			    ("Failed to initialize hardware timestamper, falling back to software timestamping");
			_hw_timestamper = NULL;
		}
	}

	_syntonize = syntonize;
	_master_local_freq_offset_init = false;
	_local_system_freq_offset_init = false;

	one_way_delay = 3600000000000;

	_peer_rate_offset = 0;
	_master_rate_offset = 0;

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	this->net_label = net_label;

	this->timer_factory = timer_factory;
	this->thread_factory = thread_factory;

	this->condition_factory = condition_factory;
	this->lock_factory = lock_factory;
}

bool IEEE1588Port::init_port()
{
	if (!OSNetworkInterfaceFactory::buildInterface
	    (&net_iface, factory_name_t("default"), net_label, _hw_timestamper))
		return false;

	this->net_iface = net_iface;
	this->net_iface->getLinkLayerAddress(&local_addr);
	clock->setClockIdentity(&local_addr);

	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	port_tx_lock = lock_factory->createLock(oslock_recursive);

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&ifindex);

	port_ready_condition = condition_factory->createCondition();

	return true;
}

void *IEEE1588Port::openPort(void)
{
	fprintf(stderr, "openPort: thread started\n");

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
	bool changed_master;
	OSTimer *timer = timer_factory->createTimer();

	switch (e) {
	case POWERUP:
	case INITIALIZE:
		XPTPD_INFO("Received POWERUP/INITIALIZE event");
		{
			unsigned long long interval1;
			unsigned long long interval3;
			unsigned long long interval4;
			Event e1 = NULL_EVENT;
			Event e3 = NULL_EVENT;
			Event e4 = NULL_EVENT;

			if (forceSlave) {
				port_state = PTP_SLAVE;
				e1 = PDELAY_INTERVAL_TIMEOUT_EXPIRES;
				interval1 =
				    (unsigned long
				     long)(pow((double)2,
					       getPDelayInterval()) *
					   1000000000.0);
			} else {
				port_state = PTP_LISTENING;
				e1 = PDELAY_INTERVAL_TIMEOUT_EXPIRES;
				e3 = SYNC_RECEIPT_TIMEOUT_EXPIRES;
				e4 = ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES;
				interval1 =
				    (unsigned long
				     long)(pow((double)2,
					       getPDelayInterval()) *
					   1000000000.0);
				interval3 =
				    (unsigned long
				     long)(SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					   pow((double)2,
					       getSyncInterval()) *
					   1000000000.0);
				interval4 =
				    (unsigned long
				     long)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER *
					   pow((double)2,
					       getAnnounceInterval()) *
					   1000000000.0);
			}

			fprintf(stderr, "Starting port thread\n");
			port_ready_condition->wait_prelock();
			listening_thread = thread_factory->createThread();
			if (!listening_thread->start
			    (openPortWrapper, (void *)this)) {
				XPTPD_ERROR("Error creating port thread\n");
				return;
			}
			port_ready_condition->wait();

			if (e1 != NULL_EVENT)
				clock->addEventTimer(this, e1, interval1);
			if (e3 != NULL_EVENT)
				clock->addEventTimer(this, e3, interval3);
			if (e4 != NULL_EVENT)
				clock->addEventTimer(this, e4, interval4);
		}
		break;
	case STATE_CHANGE_EVENT:
		if (!forceSlave) {
			int number_ports, j;
			PTPMessageAnnounce *EBest = NULL;
			PortIdentity EBestPortIdentity;

			IEEE1588Port **ports;
			clock->getPortList(number_ports, ports);
			// If ANY ports are in PTP_INTIALIZING state, STATE_CHANGE_EVENT cannot be processed
#if 0
			for (int i = 0; i < number_ports; ++i) {
				if (ports[i]->port_state == PTP_INITIALIZING) {
					break;
				}
			}
#endif
			//fprintf( stderr, "State Change Event\n" );

			// Find EBest for all ports
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

			// Check if we've changed
			EBest->getPortIdentity( &EBestPortIdentity );
			if( EBestPortIdentity.getClockIdentity() != clock->getLastEBestIdentity() ) {
				fprintf( stderr, "Changed master!\n" );
				changed_master = true;
			} else {
				changed_master = false;
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
								 changed_master);
				} else {
					if (EBest == calculateERBest()) {
						// The "best" sync was recieved on this port
						ports[j]->recommendState
						    (PTP_SLAVE, changed_master);
					} else {
						// Otherwise we are the master because we have sync'd to a better clock 
						ports[j]->recommendState
						    (PTP_MASTER,
						     changed_master);
					}
				}
			}
			clock->setLastEBestIdentity( EBestPortIdentity.getClockIdentity() );
		}
		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		{
			if (forceSlave) {
				break;
			}
			if (port_state == PTP_LISTENING
			    || port_state == PTP_UNCALIBRATED
			    || port_state == PTP_SLAVE
			    || port_state == PTP_PRE_MASTER) {
				fprintf(stderr,
					"***Sync Timeout Expired - Becoming Master: %d\n",
					e);
				port_state = PTP_MASTER;
				Timestamp system_time;
				Timestamp device_time;
				int64_t local_system_offset;

#if 0
				Timestamp crstamp_device_time;
				int64_t external_local_offset;
				int32_t external_local_freq_offset;
#endif
				uint32_t local_clock, nominal_clock_rate;

				getDeviceTime(system_time, device_time,
					      local_clock, nominal_clock_rate);

				local_system_offset =
				    TIMESTAMP_TO_NS(system_time) -
				    TIMESTAMP_TO_NS(device_time);
#if 0
				local_system_offset = device_time.nanoseconds +
				    (((unsigned long long)device_time.seconds_ms
				      << sizeof(device_time.seconds_ls) * 8) +
				     device_time.seconds_ls) * 1000000000LL;
				local_system_offset -=
				    system_time.nanoseconds +
				    (((unsigned long long)system_time.seconds_ms
				      << sizeof(system_time.seconds_ls) * 8) +
				     system_time.seconds_ls) * 1000000000LL;
#endif

				(void)
				    calcLocalSystemClockRateDifference
				    (local_system_offset, system_time);

				//getExternalClockRate( crstamp_device_time, external_local_offset, external_local_freq_offset );

#if 0
				clock->setMasterOffset(0, device_time, 0,
						       local_system_offset,
						       system_time,
						       local_system_freq_offset,
						       nominal_clock_rate,
						       local_clock);
#endif
				if (this->doSyntonization()) {
					this->adjustClockRate(0, local_clock,
							      device_time, 0,
							      false);
				}
				// "Expire" all previously received announce messages on this port
				while (!qualified_announce.empty()) {
					delete qualified_announce.back();
					qualified_announce.pop_back();
				}

				// Add timers for Announce and Sync, this is as close to immediately as we get
				clock->addEventTimer(this,
						     ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
						     8000000);
				clock->addEventTimer(this,
						     SYNC_INTERVAL_TIMEOUT_EXPIRES,
						     8000000);
			}
			break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
			XPTPD_INFO("PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
			{
				int ts_good;
				int iter = 2;
				Timestamp req_timestamp;
				long req = 1000;	// = 1 ms
				unsigned req_timestamp_counter_value;

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

				XPTPD_INFO("Preparing to send PDelay Request");
				getTxLock();
				pdelay_req->sendPort(this, NULL);
				XPTPD_INFO("Sent PDelay Request");

				ts_good =
				    getTxTimestamp(pdelay_req, req_timestamp,
						   req_timestamp_counter_value,
						   false);
				while (ts_good != 0 && iter-- != 0) {
					timer->sleep(req);
					if (ts_good != -72 && iter < 1)
						fprintf(stderr,
							"Error (TX) timestamping PDelay request (Retrying-%d), error=%d\n",
							iter, ts_good);
					ts_good =
					    getTxTimestamp(pdelay_req,
							   req_timestamp,
							   req_timestamp_counter_value,
							   iter == 0);
					req *= 2;
				}
				putTxLock();

				//fprintf( stderr, "Sequence = %hu\n", pdelay_req->getSequenceId() );

				if (pdelay_req == NULL) {
					fprintf(stderr,
						"PDelay request is NULL!\n");
					abort();
				}

				if (ts_good == 0) {
					pdelay_req->setTimestamp(req_timestamp);
				} else {
					Timestamp failed = INVALID_TIMESTAMP;
					pdelay_req->setTimestamp(failed);
				}

				if (ts_good != 0) {
					char msg
					    [HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
					getExtendedError(msg);
					fprintf(stderr,
						"Error (TX) timestamping PDelay request, error=%d\n%s",
						ts_good, msg);
					//_exit(-1);
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

			}
			clock->addEventTimer(this,
					     PDELAY_INTERVAL_TIMEOUT_EXPIRES,
					     (unsigned long
					      long)(pow((double)2,
							getPDelayInterval()) *
						    1000000000.0));
			break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
			XPTPD_INFO("SYNC_INTERVAL_TIMEOUT_EXPIRES occured");
			// Set offset from master to zero, update device vs system time offset
			Timestamp system_time;
			Timestamp device_time;
			int32_t local_system_freq_offset;
			int64_t local_system_offset;

#if 0
			Timestamp crstamp_device_time;
			int64_t external_local_offset;
			int32_t external_local_freq_offset;
#endif
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
				int iter = 2;
				Timestamp sync_timestamp;
				unsigned sync_timestamp_counter_value;
				long req = 1000;	// = 1 ms
				ts_good =
				    getTxTimestamp(sync, sync_timestamp,
						   sync_timestamp_counter_value,
						   false);
				while (ts_good != 0 && iter-- != 0) {
					timer->sleep(req);
					if (ts_good != -72 && iter < 1)
						fprintf(stderr,
							"Error (TX) timestamping Sync (Retrying), error=%d\n",
							ts_good);
					ts_good =
					    getTxTimestamp(sync, sync_timestamp,
							   sync_timestamp_counter_value,
							   iter == 0);
					req *= 2;
				}
				putTxLock();

				if (ts_good != 0) {
					char msg
					    [HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
					getExtendedError(msg);
					fprintf(stderr,
						"Error (TX) timestamping Sync, error=%d\n%s",
						ts_good, msg);
					//_exit(-1);
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
                        // Do getDeviceTime() after transmitting sync frame causing an update to local/system timestamp
                        //fprintf( stderr, "getDeviceTime() @%llu\n", ST_rdtsc() );
                        getDeviceTime(system_time, device_time, local_clock,
                                      nominal_clock_rate);
			
                        XPTPD_INFO
			  ("port::processEvent(): System time: %u,%u Device Time: %u,%u",
			   system_time.seconds_ls, system_time.nanoseconds,
			   device_time.seconds_ls, device_time.nanoseconds);
			
                        local_system_offset =
			  TIMESTAMP_TO_NS(system_time) -
			  TIMESTAMP_TO_NS(device_time);
                        local_system_freq_offset =
			  calcLocalSystemClockRateDifference
			  (local_system_offset, system_time);
                        clock->setMasterOffset(0, device_time, 0,
                                               local_system_offset, system_time,
                                               local_system_freq_offset,
                                               nominal_clock_rate, local_clock);

                        clock->addEventTimer(this,
                                             SYNC_INTERVAL_TIMEOUT_EXPIRES,
                                             (unsigned long
                                              long)(pow((double)2,
                                                        getSyncInterval()) *
                                                    1000000000.0));
			
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
			clock->setGrandmasterPortIdentity(gmId);
			getPortIdentity(dest_id);
			annc->setPortIdentity(&dest_id);
			annc->sendPort(this, NULL);
			delete annc;
		}
		clock->addEventTimer(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
				     (unsigned)(pow
						((double)2,
						 getAnnounceInterval()) *
						1000000000.0));
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
		break;
	default:
		XPTPD_INFO
		    ("Unhandled event type in IEEE1588Port::processEvent(), %d",
		     e);
		break;
	}
	return;
}

PTPMessageAnnounce *IEEE1588Port::calculateERBest(void)
{
	if (qualified_announce.empty()) {
		return NULL;
	}
	if (qualified_announce.size() == 1) {
		return qualified_announce.front();
	}

	AnnounceList_t::iterator iter_l = qualified_announce.begin();
	PTPMessageAnnounce *best = *iter_l;
	++iter_l;
	while (iter_l != qualified_announce.end()) {
		if ((*iter_l)->isBetterThan(best))
			best = *iter_l;
		iter_l = qualified_announce.erase(iter_l);
	}

	return best;
}

void IEEE1588Port::recoverPort(void)
{
	return;
}

IEEE1588Clock *IEEE1588Port::getClock(void)
{
	return clock;
}

void IEEE1588Port::getDeviceTime(Timestamp & system_time,
				 Timestamp & device_time,
				 uint32_t & local_clock,
				 uint32_t & nominal_clock_rate)
{
	if (_hw_timestamper) {
		_hw_timestamper->HWTimestamper_gettime(&system_time,
						       &device_time,
						       &local_clock,
						       &nominal_clock_rate);
	} else {
		device_time = system_time = clock->getSystemTime();
		local_clock = nominal_clock_rate = 0;
	}
	return;
}

void IEEE1588Port::recommendState(PortState state, bool changed_master)
{
	switch (state) {
	case PTP_MASTER:
		if (port_state != PTP_MASTER) {
			port_state = PTP_MASTER;
			// Start announce receipt timeout timer
			// Start sync receipt timeout timer
			clock->addEventTimer(this,
					     ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
					     8000000);
			clock->addEventTimer(this,
					     SYNC_INTERVAL_TIMEOUT_EXPIRES,
					     8000000);
			fprintf(stderr, "Switching to Master\n");
		} else {
			port_state = PTP_MASTER;
		}
		break;
	case PTP_SLAVE:
		// Stop sending announce messages
		// Stop sending sync messages
		if (port_state != PTP_SLAVE) {
			port_state = PTP_SLAVE;
			clock->deleteEventTimer(this,
						ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES);
			clock->deleteEventTimer(this,
						SYNC_INTERVAL_TIMEOUT_EXPIRES);
			clock->addEventTimer(this, SYNC_RECEIPT_TIMEOUT_EXPIRES,
					     (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					      (unsigned long
					       long)(pow((double)2,
							 getSyncInterval()) *
						     1000000000.0)));
			clock->addEventTimer(this,
					     ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
					     (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER
					      *
					      (unsigned long
					       long)(pow((double)2,
							 getAnnounceInterval())
						     * 1000000000.0)));
			fprintf(stderr, "Switching to Slave\n");
		} else {
			if (changed_master) {
				port_state = PTP_SLAVE;
			} else {
				port_state = PTP_SLAVE;
			}
		}
		break;
	default:
		XPTPD_INFO
		    ("Invalid state change requested by call to 1588Port::recommendState()");
		break;
	}
	return;
}

#define FIR_SAMPLE_TIME 1	// Seconds

int IEEE1588Port::calcMasterLocalClockRateDifference(signed long long offset,
						     Timestamp sync_time)
{
	long long inter_sync_time;
	signed long long offset_delta;
	int ppt_offset;

	XPTPD_INFO("Calculated master to local PTP clock rate difference");

	if (!_master_local_freq_offset_init) {
		_prev_sync_time = sync_time;
		_prev_master_local_offset = offset;

		_master_local_freq_offset_init = true;

		return 0;
	}

	inter_sync_time =
	    TIMESTAMP_TO_NS(sync_time) - TIMESTAMP_TO_NS(_prev_sync_time);
	offset_delta = offset - _prev_master_local_offset;

	if (rate_offset_array == NULL) {
		rate_offset_array_size = 4;
		++rate_offset_array_size;
		rate_offset_array = new int32_t[rate_offset_array_size];
		rate_offset_count = 0;
		rate_offset_index = 0;
	}
	//fprintf( stderr, "Calculated master to local PTP clock rate difference, offset=%lld,sync_time = %lld\n", offset_delta, inter_sync_time );

	if (inter_sync_time != 0) {
		//fprintf( stderr, "Offset Delta: %lld, IST: %llu(%llu,%llu)\n", offset_delta, inter_sync_time, TIMESTAMP_TO_NS(sync_time),TIMESTAMP_TO_NS(_prev_sync_time) );
		ppt_offset =
		    int (((offset_delta * 1000000000000LL) / inter_sync_time));
	} else {
		ppt_offset = 0;
	}

	_prev_sync_time = sync_time;
	_prev_master_local_offset = offset;

	return ppt_offset;
}

int IEEE1588Port::calcLocalSystemClockRateDifference(signed long long offset,
						     Timestamp system_time)
{
	unsigned long long inter_system_time;
	signed long long offset_delta;
	int ppt_offset;

	XPTPD_INFO("Calculated local to system clock rate difference");

	if (!_local_system_freq_offset_init) {
		_prev_system_time = system_time;
		_prev_local_system_offset = offset;

		_local_system_freq_offset_init = true;

		return 0;
	}

	inter_system_time =
	    TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(_prev_system_time);
	offset_delta = offset - _prev_local_system_offset;

	if (inter_system_time != 0) {
		ppt_offset =
		    int (((offset_delta * 1000000000000LL) /
			  (int64_t) inter_system_time));
		XPTPD_INFO
		    ("IEEE1588Port::calcLocalSystemClockRateDifference() offset delta: %Ld",
		     offset_delta);
	} else {
		ppt_offset = 0;
		offset_delta = 0;
	}

	if (inter_system_time != 0) {
		XPTPD_INFO("Calculation Step: %Ld",
			   ((offset_delta * 1000000000000LL) /
			    (int64_t) inter_system_time));
	}
	XPTPD_INFO
	    ("IEEE1588Port::calcLocalSystemClockRateDifference() offset: %Ld",
	     offset);
	XPTPD_INFO
	    ("IEEE1588Port::calcLocalSystemClockRateDifference() prev offset: %ld",
	     _prev_local_system_offset);
	XPTPD_INFO
	    ("IEEE1588Port::calcLocalSystemClockRateDifference() system time: %u,%u",
	     system_time.seconds_ls, system_time.nanoseconds);
	XPTPD_INFO
	    ("IEEE1588Port::calcLocalSystemClockRateDifference() prev system time: %u,%u",
	     _prev_system_time.seconds_ls, _prev_system_time.nanoseconds);
	XPTPD_INFO
	    ("IEEE1588Port::calcLocalSystemClockRateDifference() inter-system time: %Lu",
	     inter_system_time);
	XPTPD_INFO("IEEE1588Port::calcLocalSystemClockRateDifference() PPT: %d",
		   ppt_offset);

	_prev_system_time = system_time;
	_prev_local_system_offset = offset;

	return ppt_offset;
}

void IEEE1588Port::mapSocketAddr(PortIdentity * destIdentity,
				 LinkLayerAddress * remote)
{
	*remote = identity_map[*destIdentity];
	return;
}

void IEEE1588Port::addSockAddrMap(PortIdentity * destIdentity,
				  LinkLayerAddress * remote)
{
	identity_map[*destIdentity] = *remote;
	return;
}

int IEEE1588Port::getTxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getTxTimestamp(&identity, msg->getSequenceId(), timestamp,
			      counter_value, last);
}

int IEEE1588Port::getRxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getRxTimestamp(&identity, msg->getSequenceId(), timestamp,
			      counter_value, last);
}

int IEEE1588Port::getTxTimestamp(PortIdentity * sourcePortIdentity,
				 uint16_t sequenceId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper) {
		return
		    _hw_timestamper->HWTimestamper_txtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return true;
}

int IEEE1588Port::getRxTimestamp(PortIdentity * sourcePortIdentity,
				 uint16_t sequenceId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper) {
		return
		    _hw_timestamper->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, sequenceId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return true;
}
