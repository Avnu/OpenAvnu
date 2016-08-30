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

#include <gptp_log.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

LinkLayerAddress IEEE1588Port::other_multicast(OTHER_MULTICAST);
LinkLayerAddress IEEE1588Port::pdelay_multicast(PDELAY_MULTICAST);
LinkLayerAddress IEEE1588Port::test_status_multicast(TEST_STATUS_MULTICAST);

OSThreadExitCode watchNetLinkWrapper(void *arg)
{
	IEEE1588Port *port;

	port = (IEEE1588Port *) arg;
	if (port->watchNetLink() == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

OSThreadExitCode openPortWrapper(void *arg)
{
	IEEE1588Port *port;

	port = (IEEE1588Port *) arg;
	if (port->openPort(port) == NULL)
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

IEEE1588Port::IEEE1588Port(IEEE1588PortInit_t *portInit)
{
	sync_sequence_id = 0;

	clock = portInit->clock;
	ifindex = portInit->index;
	clock->registerPort(this, ifindex);

	forceSlave = portInit->forceSlave;
	port_state = PTP_INITIALIZING;

	automotive_profile = portInit->automotive_profile;
	isGM = portInit->isGM;
	testMode = portInit->testMode;
	initialLogSyncInterval = portInit->initialLogSyncInterval;
	initialLogPdelayReqInterval = portInit->initialLogPdelayReqInterval;
	operLogPdelayReqInterval = portInit->operLogPdelayReqInterval;
	operLogSyncInterval = portInit->operLogSyncInterval;

	if (automotive_profile) {
		asCapable = true;

		if (initialLogSyncInterval == LOG2_INTERVAL_INVALID)
			initialLogSyncInterval = -5;     // 31.25 ms
		if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			initialLogPdelayReqInterval = 0;  // 1 second
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = 0;      // 1 second
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = 0;           // 1 second
	}
	else {
		asCapable = false;

		if (initialLogSyncInterval == LOG2_INTERVAL_INVALID)
			initialLogSyncInterval = -3;       // 125 ms
		if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			initialLogPdelayReqInterval = 0;   // 1 second
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = 0;      // 1 second
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = 0;           // 1 second
	}

	announce_sequence_id = 0;
	signal_sequence_id = 0;
	sync_sequence_id = 0;
	pdelay_sequence_id = 0;

	pdelay_started = false;
	pdelay_halted = false;
	sync_rate_interval_timer_started = false;

	duplicate_resp_counter = 0;
	last_invalid_seqid = 0;

	/*TODO: Add intervals below to a config interface*/
	log_mean_sync_interval = initialLogSyncInterval;
	_accelerated_sync_count = portInit->accelerated_sync_count;
	log_mean_announce_interval = 0;
	log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;

	_current_clock_offset = _initial_clock_offset = portInit->offset;

	rate_offset_array = NULL;

	_hw_timestamper = portInit->timestamper;

	one_way_delay = ONE_WAY_DELAY_DEFAULT;
	neighbor_prop_delay_thresh = NEIGHBOR_PROP_DELAY_THRESH;
	sync_receipt_thresh = DEFAULT_SYNC_RECEIPT_THRESH;
	wrongSeqIDCounter = 0;

	_peer_rate_offset = 1.0;

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	qualified_announce = NULL;

	this->net_label = portInit->net_label;

	this->timer_factory = portInit->timer_factory;
	this->thread_factory = portInit->thread_factory;

	this->condition_factory = portInit->condition_factory;
	this->lock_factory = portInit->lock_factory;

	setPdelayCount(0);
	setSyncCount(0);
	memset(link_delay, 0, sizeof(link_delay));

	_peer_offset_init = false;

	if (automotive_profile) {
		if (isGM) {
			avbSyncState = 1;
		}
		else {
			avbSyncState = 2;
		}
		if (testMode) {
			linkUpCount = 1;  // TODO : really should check the current linkup status http://stackoverflow.com/questions/15723061/how-to-check-if-interface-is-up
			linkDownCount = 0;
		}
		setStationState(STATION_STATE_RESERVED);
	}
}

void IEEE1588Port::timestamper_init(void)
{
	if( _hw_timestamper != NULL ) {
		_hw_timestamper->init_phy_delay(this->link_delay);
		if( !_hw_timestamper->HWTimestamper_init( net_label, net_iface )) {
			GPTP_LOG_ERROR
				( "Failed to initialize hardware timestamper, "
				  "falling back to software timestamping" );
			_hw_timestamper = NULL;
			return;
		}
	}
}

void IEEE1588Port::timestamper_reset(void)
{
	if( _hw_timestamper != NULL ) {
		_hw_timestamper->init_phy_delay(this->link_delay);
		_hw_timestamper->HWTimestamper_reset();
	}
}

bool IEEE1588Port::init_port(int delay[4])
{
	if (!OSNetworkInterfaceFactory::buildInterface
	    (&net_iface, factory_name_t("default"), net_label, _hw_timestamper))
		return false;

	this->net_iface = net_iface;
	this->net_iface->getLinkLayerAddress(&local_addr);
	clock->setClockIdentity(&local_addr);
	memcpy(this->link_delay, delay, sizeof(this->link_delay));

	this->timestamper_init();

	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	port_tx_lock = lock_factory->createLock(oslock_recursive);

	syncReceiptTimerLock = lock_factory->createLock(oslock_recursive);
	syncIntervalTimerLock = lock_factory->createLock(oslock_recursive);
	announceIntervalTimerLock = lock_factory->createLock(oslock_recursive);
	pDelayIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&ifindex);

	port_ready_condition = condition_factory->createCondition();

	return true;
}

void IEEE1588Port::startPDelay() {
	if(!pdelayHalted()) {
		if (automotive_profile) {
			if (log_min_mean_pdelay_req_interval != PTPMessageSignalling::sigMsgInterval_NoSend) {
				long long unsigned int waitTime;
				waitTime = ((long long) (pow((double)2, log_min_mean_pdelay_req_interval) * 1000000000.0));
				waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
				pdelay_started = true;
				startPDelayIntervalTimer(waitTime);
			}
		}
		else {
			pdelay_started = true;
			startPDelayIntervalTimer(32000000);
		}
	}
}

void IEEE1588Port::stopPDelay() {
	haltPdelay(true);
	pdelay_started = false;
	clock->deleteEventTimerLocked( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
}

void IEEE1588Port::startSyncRateIntervalTimer() {
	if (automotive_profile) {
		sync_rate_interval_timer_started = true;
		if (isGM) {
			// GM will wait up to 8  seconds for signaling rate
			// TODO: This isn't according to spec but set because it is believed that some slave devices aren't signalling
			//  to reduce the rate
			clock->addEventTimerLocked( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED, 8000000000 );
		}
		else {
			// Slave will time out after 4 seconds
			clock->addEventTimerLocked( this, SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED, 4000000000 );
		}
	}
}

void IEEE1588Port::startAnnounce() {
	if (!automotive_profile) {
		startAnnounceIntervalTimer(16000000);
	}
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

void *IEEE1588Port::watchNetLink(void)
{
	// Should never return
	net_iface->watchNetLink(this);

	return NULL;
}

void *IEEE1588Port::openPort(IEEE1588Port *port)
{
	port_ready_condition->signal();
	struct phy_delay get_delay = { 0, 0, 0, 0 };
	if(port->_hw_timestamper)
		port->_hw_timestamper->get_phy_delay(&get_delay);

	while (1) {
		PTPMessageCommon *msg;
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);

		if ((rrecv = net_iface->nrecv(&remote, buf, length,&get_delay)) == net_succeed) {
			GPTP_LOG_VERBOSE("Processing network buffer");
			msg = buildPTPMessage((char *)buf, (int)length, &remote,
					    this);
			if (msg != NULL) {
				GPTP_LOG_VERBOSE("Processing message");
				msg->processMessage(this);
				if (msg->garbage()) {
					delete msg;
				}
			} else {
				GPTP_LOG_ERROR("Discarding invalid message");
			}
		} else if (rrecv == net_fatal) {
			GPTP_LOG_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			break;
		}
	}

	return NULL;
}

net_result IEEE1588Port::port_send(uint16_t etherType, uint8_t * buf, int size,
				   MulticastType mcast_type,
				   PortIdentity * destIdentity, bool timestamp)
{
	LinkLayerAddress dest;

	if (mcast_type != MCAST_NONE) {
		if (mcast_type == MCAST_PDELAY) {
			dest = pdelay_multicast;
		}
		else if (mcast_type == MCAST_TEST_STATUS) {
			dest = test_status_multicast;
		}
		else {
			dest = other_multicast;
		}
	} else {
		mapSocketAddr(destIdentity, &dest);
	}

	return net_iface->send(&dest, etherType, (uint8_t *) buf, size, timestamp);
}

unsigned IEEE1588Port::getPayloadOffset()
{
	return net_iface->getPayloadOffset();
}

void IEEE1588Port::sendEventPort(uint16_t etherType, uint8_t * buf, int size,
				 MulticastType mcast_type,
				 PortIdentity * destIdentity)
{
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, true);
	if (rtx != net_succeed) {
		GPTP_LOG_ERROR("sendEventPort(): failure");
	}

	return;
}

void IEEE1588Port::sendGeneralPort(uint16_t etherType, uint8_t * buf, int size,
				   MulticastType mcast_type,
				   PortIdentity * destIdentity)
{
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, false);
	if (rtx != net_succeed) {
		GPTP_LOG_ERROR("sendGeneralPort(): failure");
	}

	return;
}

void IEEE1588Port::processEvent(Event e)
{
	bool changed_external_master;

	switch (e) {
	case POWERUP:
	case INITIALIZE:
		GPTP_LOG_DEBUG("Received POWERUP/INITIALIZE event");

		{
			unsigned long long interval3;
			unsigned long long interval4;
			Event e3 = NULL_EVENT;
			Event e4 = NULL_EVENT;

			if( port_state != PTP_MASTER ) {
				_accelerated_sync_count = -1;
			}

			if (!automotive_profile) {
				if (port_state != PTP_SLAVE && port_state != PTP_MASTER) {
					GPTP_LOG_STATUS("Starting PDelay");
					startPDelay();
				}
			}
			else {
				startPDelay();
			}

			if( clock->getPriority1() == 255 || port_state == PTP_SLAVE ) {
				becomeSlave( true );
			} else if( port_state == PTP_MASTER ) {
				becomeMaster( true );
			} else {
				/*TODO: Should I remove the commented code below ?*/
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

			link_thread = thread_factory->createThread();
			if(!link_thread->start(watchNetLinkWrapper, (void *)this))
			{
				GPTP_LOG_ERROR("Error creating port link thread");
				return;
			}

			listening_thread = thread_factory->createThread();
			if (!listening_thread->
				start (openPortWrapper, (void *)this))
			{
				GPTP_LOG_ERROR("Error creating port thread");
				return;
			}
			port_ready_condition->wait();

			if (e3 != NULL_EVENT)
				clock->addEventTimerLocked(this, e3, interval3);
			if (e4 != NULL_EVENT)
				clock->addEventTimerLocked(this, e4, interval4);
		}

		if (automotive_profile) {
			setStationState(STATION_STATE_ETHERNET_READY);
			if (testMode) {
				APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
				if (testStatusMsg) {
					testStatusMsg->sendPort(this);
					delete testStatusMsg;
				}
			}
			if (!isGM) {
				// Send an initial signalling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, log_mean_sync_interval, PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, NULL);
					delete sigMsg;
				}

				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}
		}

		break;

	case STATE_CHANGE_EVENT:
		if (!automotive_profile) {       // BMCA is not active with Automotive Profile
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
					} else if (ports[j]->calculateERBest()) {
						if (ports[j]->calculateERBest()->isBetterThan(EBest)) {
							EBest = ports[j]->calculateERBest();
						}
					}
				}

				if (EBest == NULL) {
					break;
				}

				/* Check if we've changed */
				{

					uint8_t LastEBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
					clock->getLastEBestIdentity().
					getIdentityString( LastEBestClockIdentity );
					EBest->getGrandmasterIdentity( EBestClockIdentity );
					if( memcmp( EBestClockIdentity, LastEBestClockIdentity,
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
		}
		break;

	case LINKUP:
		if (automotive_profile) {
			GPTP_LOG_EXCEPTION("LINKUP");
		}
		else {
			GPTP_LOG_STATUS("LINKUP");
		}

		if (automotive_profile) {
			asCapable = true;

			setStationState(STATION_STATE_ETHERNET_READY);
			if (testMode) {
				APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
				if (testStatusMsg) {
					testStatusMsg->sendPort(this);
					delete testStatusMsg;
				}
			}

			log_mean_sync_interval = initialLogSyncInterval;
			log_mean_announce_interval = 0;
			log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;

			if (!isGM) {
				// Send an initial signaling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, log_mean_sync_interval, PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, NULL);
					delete sigMsg;
				}

				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}

			// Reset Sync count and pdelay count
			setPdelayCount(0);
			setSyncCount(0);

			// Start AVB SYNC at 2. It will decrement after each sync. When it reaches 0 the Test Status message
			// can be sent
			if (isGM) {
				avbSyncState = 1;
			}
			else {
				avbSyncState = 2;
			}

			if (testMode) {
				linkUpCount++;
			}
		}
		this->timestamper_reset();
		break;

	case LINKDOWN:
		if (automotive_profile) {
			GPTP_LOG_EXCEPTION("LINK DOWN");
		}
		else {
			GPTP_LOG_STATUS("LINK DOWN");
		}
		if (testMode) {
			linkDownCount++;
		}
		break;

	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		{
			if (e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES) {
				incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts();
			}
			else if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
				incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts();
			}
			if (!automotive_profile) {

				if( clock->getPriority1() == 255 ) {
					// Restart timer
					if( e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ) {
						clock->addEventTimerLocked
						  (this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
						   (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
						    (unsigned long long)
						    (pow((double)2,getAnnounceInterval())*
						     1000000000.0)));
					} else {
						startSyncReceiptTimer((unsigned long long)
							 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
							  ((double) pow((double)2, getSyncInterval()) *
							   1000000000.0)));
					}
				}

				if (port_state == PTP_INITIALIZING
				    || port_state == PTP_UNCALIBRATED
				    || port_state == PTP_SLAVE
				    || port_state == PTP_PRE_MASTER) {
					GPTP_LOG_STATUS(
					  "*** %s Timeout Expired - Becoming Master",
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
					if( clock->getPriority1() != 255) {
						clock->addEventTimerLocked
						  ( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
					}
					startAnnounce();
				}

			}
			else {
				// Automotive Profile
				if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
					GPTP_LOG_EXCEPTION("SYNC receipt timeout");

					startSyncReceiptTimer((unsigned long long)
						 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
						  ((double) pow((double)2, getSyncInterval()) *
						   1000000000.0)));
				}
			}

		}

		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
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
			GPTP_LOG_DEBUG("Sent PDelay Request");

			OSTimer *timer = timer_factory->createTimer();

			ts_good = getTxTimestamp
				(pdelay_req, req_timestamp, req_timestamp_counter_value, false);
			while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
				timer->sleep(req);
				wait_time += req;
				if (ts_good != GPTP_EC_EAGAIN && iter < 1)
					GPTP_LOG_ERROR(
						 "Error (TX) timestamping PDelay request "
						 "(Retrying-%d), error=%d", iter, ts_good);
				ts_good =
				    getTxTimestamp
					(pdelay_req, req_timestamp,
					 req_timestamp_counter_value, iter == 0);
				req *= 2;
			}
			delete timer;
			putTxLock();

			if (ts_good == GPTP_EC_SUCCESS) {
				pdelay_req->setTimestamp(req_timestamp);
			} else {
			  Timestamp failed = INVALID_TIMESTAMP;
			  pdelay_req->setTimestamp(failed);
			  GPTP_LOG_ERROR( "Invalid TX" );
			}

			if (ts_good != GPTP_EC_SUCCESS) {
				char msg
				    [HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
				getExtendedError(msg);
				GPTP_LOG_ERROR(
					"Error (TX) timestamping PDelay request, error=%d\t%s",
					ts_good, msg);
			}
#ifdef DEBUG
			if (ts_good == GPTP_EC_SUCCESS) {
				GPTP_LOG_DEBUG
				    ("Successful PDelay Req timestamp, %u,%u",
				     req_timestamp.seconds_ls,
				     req_timestamp.nanoseconds);
			} else {
				GPTP_LOG_DEBUG
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
				clock->addEventTimerLocked
					(this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, timeout );

				interval =
					((long long)
					 (pow((double)2,getPDelayInterval())*1000000000.0)) -
					wait_time*1000;
				interval = interval > EVENT_TIMER_GRANULARITY ?
					interval : EVENT_TIMER_GRANULARITY;
				startPDelayIntervalTimer(interval);
			}
		}
		break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("SYNC_INTERVAL_TIMEOUT_EXPIRES occured");
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
				GPTP_LOG_DEBUG("Sent SYNC message");

				if (automotive_profile && port_state == PTP_MASTER) {
					if (avbSyncState > 0) {
						avbSyncState--;
						if (avbSyncState == 0) {
							// Send Avnu Automotive Profile status message
							setStationState(STATION_STATE_AVB_SYNC);
							if (testMode) {
								APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
								if (testStatusMsg) {
									testStatusMsg->sendPort(this);
									delete testStatusMsg;
								}
							}
						}
					}
				}

				int ts_good;
				Timestamp sync_timestamp;
				unsigned sync_timestamp_counter_value;
				int iter = TX_TIMEOUT_ITER;
				long req = TX_TIMEOUT_BASE;

				OSTimer *timer = timer_factory->createTimer();

				ts_good =
					getTxTimestamp(sync, sync_timestamp,
								   sync_timestamp_counter_value,
								   false);
				while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
					timer->sleep(req);
					wait_time += req;

					if (ts_good != GPTP_EC_EAGAIN && iter < 1)
						GPTP_LOG_ERROR(
							"Error (TX) timestamping Sync (Retrying), "
							"error=%d", ts_good);
					ts_good =
					getTxTimestamp
						(sync, sync_timestamp,
						 sync_timestamp_counter_value, iter == 0);
					req *= 2;
				}
				delete timer;
				putTxLock();

				if (ts_good != GPTP_EC_SUCCESS) {
					char msg [HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
					getExtendedError(msg);
					GPTP_LOG_ERROR(
						 "Error (TX) timestamping Sync, error="
						 "%d\n%s",
						 ts_good, msg );
				}

				if (ts_good == GPTP_EC_SUCCESS) {
					GPTP_LOG_VERBOSE("Successful Sync timestamp");
					GPTP_LOG_VERBOSE("Seconds: %u",
							   sync_timestamp.seconds_ls);
					GPTP_LOG_VERBOSE("Nanoseconds: %u",
							   sync_timestamp.nanoseconds);
				} else {
					GPTP_LOG_ERROR
						("*** Unsuccessful Sync timestamp");
				}

				PTPMessageFollowUp *follow_up;
				if (ts_good == GPTP_EC_SUCCESS) {

					follow_up =
						new PTPMessageFollowUp(this);
					PortIdentity dest_id;
					getPortIdentity(dest_id);

					follow_up->setClockSourceTime(getClock()->getFUPInfo());
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

			GPTP_LOG_VERBOSE
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
			  (this, 0, device_time, 1.0, local_system_offset,
			   system_time, local_system_freq_offset, sync_count,
			   pdelay_count, port_state, asCapable );

			if (!automotive_profile) {
				/* If accelerated_sync is non-zero then start 16 ms sync
				   timer, subtract 1, for last one start PDelay also */
				if( _accelerated_sync_count > 0 ) {
					clock->addEventTimerLocked
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
					startSyncIntervalTimer(wait_time);
				}
			}
			else {
				// Automotive Profile
				syncDone();

				wait_time = ((long long) (pow((double)2, getSyncInterval()) * 1000000000.0));
				wait_time = wait_time > EVENT_TIMER_GRANULARITY ? wait_time : EVENT_TIMER_GRANULARITY;
				startSyncIntervalTimer(wait_time);
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
		startAnnounceIntervalTimer((uint64_t)(pow((double)2, getAnnounceInterval()) * 1000000000.0));
		break;
	case FAULT_DETECTED:
		GPTP_LOG_ERROR("Received FAULT_DETECTED event");
		if (!automotive_profile) {
			setAsCapable(false);
		}
		break;
	case PDELAY_DEFERRED_PROCESSING:
		pdelay_rx_lock->lock();
		if (last_pdelay_resp_fwup == NULL) {
			GPTP_LOG_ERROR("PDelay Response Followup is NULL!");
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
		if (!automotive_profile) {
			GPTP_LOG_EXCEPTION("PDelay Response Receipt Timeout");
			setAsCapable(false);
		}
		pdelay_count = 0;
		break;

	case PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES:
		GPTP_LOG_EXCEPTION("PDelay Resp Peer Misbehaving timeout expired! Restarting PDelay");

		haltPdelay(false);
		if( port_state != PTP_SLAVE && port_state != PTP_MASTER ) {
			GPTP_LOG_STATUS("Starting PDelay" );
			startPDelay();
		}
		break;
	case SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED:
		{
			GPTP_LOG_INFO("SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED occured");

			sync_rate_interval_timer_started = false;

			bool sendSignalMessage = false;
			if (log_mean_sync_interval != operLogSyncInterval) {
				log_mean_sync_interval = operLogSyncInterval;
				sendSignalMessage = true;
			}

			if (log_min_mean_pdelay_req_interval != operLogPdelayReqInterval) {
				log_min_mean_pdelay_req_interval = operLogPdelayReqInterval;
				sendSignalMessage = true;
			}

			if (sendSignalMessage) {
				if (!isGM) {
				// Send operational signalling message
					PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
					if (sigMsg) {
						if (automotive_profile)
							sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoChange, log_mean_sync_interval, PTPMessageSignalling::sigMsgInterval_NoChange);
						else 
							sigMsg->setintervals(log_min_mean_pdelay_req_interval, log_mean_sync_interval, PTPMessageSignalling::sigMsgInterval_NoChange);
						sigMsg->sendPort(this, NULL);
						delete sigMsg;
					}

					startSyncReceiptTimer((unsigned long long)
						 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
						  ((double) pow((double)2, getSyncInterval()) *
						   1000000000.0)));
				}
			}
		}

		break;
	default:
		GPTP_LOG_ERROR
		  ("Unhandled event type in IEEE1588Port::processEvent(), %d",
		   e);
		break;
	}

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
	// Stop announce receipt timeout timer
	clock->deleteEventTimerLocked( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES );

	// Stop sync receipt timeout timer
	stopSyncReceiptTimer();

	if( annc ) {
		if (!automotive_profile) {
			startAnnounce();
		}
	}
	startSyncIntervalTimer(16000000);
	GPTP_LOG_STATUS("Switching to Master" );

	clock->updateFUPInfo();

	return;
}

void IEEE1588Port::becomeSlave( bool restart_syntonization ) {
	clock->deleteEventTimerLocked( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

	port_state = PTP_SLAVE;

	if (!automotive_profile) {
		clock->addEventTimerLocked
		  (this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		   (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
			(unsigned long long)
			(pow((double)2,getAnnounceInterval())*1000000000.0)));
	}

	GPTP_LOG_STATUS("Switching to Slave" );
	if( restart_syntonization ) clock->newSyntonizationSetPoint();

	getClock()->updateFUPInfo();

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
				GPTP_LOG_STATUS("Changed master!" );
				clock->newSyntonizationSetPoint();
				getClock()->updateFUPInfo();
				reset_sync = true;
			}
		}
		break;
	default:
		GPTP_LOG_ERROR
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

void IEEE1588Port::startSyncReceiptTimer(long long unsigned int waitTime) {
	syncReceiptTimerLock->lock();
	clock->deleteEventTimerLocked(this, SYNC_RECEIPT_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, SYNC_RECEIPT_TIMEOUT_EXPIRES, waitTime);
	syncReceiptTimerLock->unlock();
}

void IEEE1588Port::stopSyncReceiptTimer(void)
{
	syncReceiptTimerLock->lock();
	clock->deleteEventTimerLocked(this, SYNC_RECEIPT_TIMEOUT_EXPIRES);
	syncReceiptTimerLock->unlock();
}

void IEEE1588Port::startSyncIntervalTimer(long long unsigned int waitTime)
{
	syncIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, SYNC_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, SYNC_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	syncIntervalTimerLock->unlock();
}

void IEEE1588Port::startAnnounceIntervalTimer(long long unsigned int waitTime)
{
	announceIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	announceIntervalTimerLock->unlock();
}

void IEEE1588Port::startPDelayIntervalTimer(long long unsigned int waitTime)
{
	pDelayIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	pDelayIntervalTimerLock->unlock();
}

