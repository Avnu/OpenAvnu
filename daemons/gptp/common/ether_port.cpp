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

#include <ether_port.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>
#include <ether_tstamper.hpp>

#include <gptp_log.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

LinkLayerAddress EtherPort::other_multicast(OTHER_MULTICAST);
LinkLayerAddress EtherPort::pdelay_multicast(PDELAY_MULTICAST);
LinkLayerAddress EtherPort::test_status_multicast
( TEST_STATUS_MULTICAST );

OSThreadExitCode watchNetLinkWrapper(void *arg)
{
	EtherPort *port;

	port = (EtherPort *) arg;
	if (port->watchNetLink() == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

OSThreadExitCode openPortWrapper(void *arg)
{
	EtherPort *port;

	port = (EtherPort *) arg;
	if (port->openPort(port) == NULL)
		return osthread_ok;
	else
		return osthread_error;
}

EtherPort::~EtherPort()
{
	delete port_ready_condition;
}

EtherPort::EtherPort( PortInit_t *portInit ) :
	CommonPort( portInit )
{
	automotive_profile = portInit->automotive_profile;
	linkUp = portInit->linkUp;
	setTestMode( portInit->testMode );

	pdelay_sequence_id = 0;

	pdelay_started = false;
	pdelay_halted = false;
	sync_rate_interval_timer_started = false;

	duplicate_resp_counter = 0;
	last_invalid_seqid = 0;

	initialLogPdelayReqInterval = portInit->initialLogPdelayReqInterval;
	operLogPdelayReqInterval = portInit->operLogPdelayReqInterval;
	operLogSyncInterval = portInit->operLogSyncInterval;

	if (automotive_profile) {
		setAsCapable( true );

		if (getInitSyncInterval() == LOG2_INTERVAL_INVALID)
			setInitSyncInterval( -5 );     // 31.25 ms
		if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			initialLogPdelayReqInterval = 0;  // 1 second
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = 0;      // 1 second
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = 0;           // 1 second
	}
	else {
		setAsCapable( false );

		if ( getInitSyncInterval() == LOG2_INTERVAL_INVALID )
			setInitSyncInterval( -3 );       // 125 ms
		if (initialLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			initialLogPdelayReqInterval = 0;   // 1 second
		if (operLogPdelayReqInterval == LOG2_INTERVAL_INVALID)
			operLogPdelayReqInterval = 0;      // 1 second
		if (operLogSyncInterval == LOG2_INTERVAL_INVALID)
			operLogSyncInterval = 0;           // 1 second
	}

	/*TODO: Add intervals below to a config interface*/
	log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;

	last_sync = NULL;
	last_pdelay_req = NULL;
	last_pdelay_resp = NULL;
	last_pdelay_resp_fwup = NULL;

	setPdelayCount(0);
	setSyncCount(0);

	if (automotive_profile) {
		if (isGM) {
			avbSyncState = 1;
		}
		else {
			avbSyncState = 2;
		}
		if (getTestMode())
		{
			linkUpCount = 1;  // TODO : really should check the current linkup status http://stackoverflow.com/questions/15723061/how-to-check-if-interface-is-up
			linkDownCount = 0;
		}
		setStationState(STATION_STATE_RESERVED);
	}
}

bool EtherPort::_init_port( void )
{
	pdelay_rx_lock = lock_factory->createLock(oslock_recursive);
	port_tx_lock = lock_factory->createLock(oslock_recursive);

	pDelayIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	port_ready_condition = condition_factory->createCondition();

	return true;
}

void EtherPort::startPDelay()
{
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

void EtherPort::stopPDelay()
{
	haltPdelay(true);
	pdelay_started = false;
	clock->deleteEventTimerLocked( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
}

void EtherPort::startSyncRateIntervalTimer()
{
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

void EtherPort::processMessage
( char *buf, int length, LinkLayerAddress *remote, uint32_t link_speed )
{
	GPTP_LOG_VERBOSE("Processing network buffer");

	PTPMessageCommon *msg =
		buildPTPMessage( buf, (int)length, remote, this );

	if (msg == NULL)
	{
		GPTP_LOG_ERROR("Discarding invalid message");
		return;
	}
	GPTP_LOG_VERBOSE("Processing message");

	if( msg->isEvent() )
	{
		Timestamp rx_timestamp = msg->getTimestamp();
		Timestamp phy_compensation = getRxPhyDelay( link_speed );
		GPTP_LOG_DEBUG( "RX PHY compensation: %s sec",
			 phy_compensation.toString().c_str() );
		phy_compensation._version = rx_timestamp._version;
		rx_timestamp = rx_timestamp - phy_compensation;
		msg->setTimestamp( rx_timestamp );
	}

	msg->processMessage(this);
	if (msg->garbage())
		delete msg;
}

void *EtherPort::openPort( EtherPort *port )
{
	port_ready_condition->signal();

	while (1) {
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);
		uint32_t link_speed;

		if ( ( rrecv = recv( &remote, buf, length, link_speed ))
		     == net_succeed )
		{
			processMessage
				((char *)buf, (int)length, &remote, link_speed );
		} else if (rrecv == net_fatal) {
			GPTP_LOG_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			break;
		}
	}

	return NULL;
}

net_result EtherPort::port_send
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity *destIdentity, bool timestamp )
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

	return send(&dest, etherType, (uint8_t *) buf, size, timestamp);
}

void EtherPort::sendEventPort
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity *destIdentity, uint32_t *link_speed )
{
	net_result rtx = port_send
		( etherType, buf, size, mcast_type, destIdentity, true );
	if( rtx != net_succeed )
	{
		GPTP_LOG_ERROR("sendEventPort(): failure");
		return;
	}

	*link_speed = this->getLinkSpeed();

	return;
}

void EtherPort::sendGeneralPort
( uint16_t etherType, uint8_t *buf, int size, MulticastType mcast_type,
  PortIdentity * destIdentity )
{
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, false);
	if (rtx != net_succeed) {
		GPTP_LOG_ERROR("sendGeneralPort(): failure");
	}

	return;
}

bool EtherPort::_processEvent( Event e )
{
	bool ret;

	switch (e) {
	case POWERUP:
	case INITIALIZE:
		if (!automotive_profile) {
			if ( getPortState() != PTP_SLAVE &&
			     getPortState() != PTP_MASTER )
			{
				GPTP_LOG_STATUS("Starting PDelay");
				startPDelay();
			}
		}
		else {
			startPDelay();
		}

		port_ready_condition->wait_prelock();

		if( !linkWatch(watchNetLinkWrapper, (void *)this) )
		{
			GPTP_LOG_ERROR("Error creating port link thread");
			ret = false;
			break;
		}

		if( !linkOpen(openPortWrapper, (void *)this) )
		{
			GPTP_LOG_ERROR("Error creating port thread");
			ret = false;
			break;
		}

		port_ready_condition->wait();

		if (automotive_profile) {
			setStationState(STATION_STATE_ETHERNET_READY);
			if (getTestMode())
			{
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
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, NULL);
					delete sigMsg;
				}

				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}
		}

		ret = true;
		break;
	case STATE_CHANGE_EVENT:
		// If the automotive profile is enabled, handle the event by
		// doing nothing and returning true, preventing the default
		// action from executing
		if( automotive_profile )
			ret = true;
		else
			ret = false;

		break;
	case LINKUP:
		haltPdelay(false);
		startPDelay();
		if (automotive_profile) {
			GPTP_LOG_EXCEPTION("LINKUP");
		}
		else {
			GPTP_LOG_STATUS("LINKUP");
		}

		if( clock->getPriority1() == 255 || getPortState() == PTP_SLAVE ) {
			becomeSlave( true );
		} else if( getPortState() == PTP_MASTER ) {
			becomeMaster( true );
		} else {
			clock->addEventTimerLocked(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
				ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * pow(2.0, getAnnounceInterval()) * 1000000000.0);
		}

		if (automotive_profile) {
			setAsCapable( true );

			setStationState(STATION_STATE_ETHERNET_READY);
			if (getTestMode())
			{
				APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
				if (testStatusMsg) {
					testStatusMsg->sendPort(this);
					delete testStatusMsg;
				}
			}

			resetInitSyncInterval();
			setAnnounceInterval( 0 );
			log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;

			if (!isGM) {
				// Send an initial signaling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoSend);
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

			if (getTestMode())
			{
				linkUpCount++;
			}
		}
		this->timestamper_reset();

		ret = true;
		break;
	case LINKDOWN:
		stopPDelay();
		if (automotive_profile) {
			GPTP_LOG_EXCEPTION("LINK DOWN");
		}
		else {
			setAsCapable(false);
			GPTP_LOG_STATUS("LINK DOWN");
		}
		if (getTestMode())
		{
			linkDownCount++;
		}

		ret = true;
		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		if( !automotive_profile )
		{
			ret = false;
			break;
		}

		// Automotive Profile specific action
		if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
			GPTP_LOG_EXCEPTION("SYNC receipt timeout");

			startSyncReceiptTimer((unsigned long long)
					      (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					       ((double) pow((double)2, getSyncInterval()) *
						1000000000.0)));
		}
		ret = true;
		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
		{
			Timestamp req_timestamp;

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
			GPTP_LOG_DEBUG("*** Sent PDelay Request message");
			putTxLock();

			{
				long long timeout;
				long long interval;

				timeout = PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER *
					((long long)
					 (pow((double)2,getPDelayInterval())*1000000000.0));

				timeout = timeout > EVENT_TIMER_GRANULARITY ?
					timeout : EVENT_TIMER_GRANULARITY;
				clock->addEventTimerLocked
					(this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, timeout );
				GPTP_LOG_DEBUG("Schedule PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, "
					"PDelay interval %d, timeout %lld",
					getPDelayInterval(), timeout);

				interval =
					((long long)
					 (pow((double)2,getPDelayInterval())*1000000000.0));
				interval = interval > EVENT_TIMER_GRANULARITY ?
					interval : EVENT_TIMER_GRANULARITY;
				startPDelayIntervalTimer(interval);
			}
		}
		break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		{
			/* Set offset from master to zero, update device vs
			   system time offset */

			// Send a sync message and then a followup to broadcast
			PTPMessageSync *sync = new PTPMessageSync(this);
			PortIdentity dest_id;
			bool tx_succeed;
			getPortIdentity(dest_id);
			sync->setPortIdentity(&dest_id);
			getTxLock();
			tx_succeed = sync->sendPort(this, NULL);
			GPTP_LOG_DEBUG("Sent SYNC message");

			if ( automotive_profile &&
			     getPortState() == PTP_MASTER )
			{
				if (avbSyncState > 0) {
					avbSyncState--;
					if (avbSyncState == 0) {
						// Send Avnu Automotive Profile status message
						setStationState(STATION_STATE_AVB_SYNC);
						if (getTestMode()) {
							APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
							if (testStatusMsg) {
								testStatusMsg->sendPort(this);
								delete testStatusMsg;
							}
						}
					}
				}
			}
			putTxLock();

			if ( tx_succeed )
			{
				Timestamp sync_timestamp = sync->getTimestamp();

				GPTP_LOG_VERBOSE("Successful Sync timestamp");
				GPTP_LOG_VERBOSE("Seconds: %u",
						 sync_timestamp.seconds_ls);
				GPTP_LOG_VERBOSE("Nanoseconds: %u",
						 sync_timestamp.nanoseconds);

				PTPMessageFollowUp *follow_up = new PTPMessageFollowUp(this);
				PortIdentity dest_id;
				getPortIdentity(dest_id);

				follow_up->setClockSourceTime(getClock()->getFUPInfo());
				follow_up->setPortIdentity(&dest_id);
				follow_up->setSequenceId(sync->getSequenceId());
				follow_up->setPreciseOriginTimestamp
					(sync_timestamp);
				follow_up->sendPort(this, NULL);
				delete follow_up;
			} else {
				GPTP_LOG_ERROR
					("*** Unsuccessful Sync timestamp");
			}
			delete sync;
		}
		break;
	case FAULT_DETECTED:
		GPTP_LOG_ERROR("Received FAULT_DETECTED event");
		if (!automotive_profile) {
			setAsCapable(false);
		}
		break;
	case PDELAY_DEFERRED_PROCESSING:
		GPTP_LOG_DEBUG("PDELAY_DEFERRED_PROCESSING occured");
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
		setPdelayCount( 0 );
		break;

	case PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES:
		GPTP_LOG_EXCEPTION("PDelay Resp Peer Misbehaving timeout expired! Restarting PDelay");

		haltPdelay(false);
		if( getPortState() != PTP_SLAVE &&
		    getPortState() != PTP_MASTER )
		{
			GPTP_LOG_STATUS("Starting PDelay" );
			startPDelay();
		}
		break;
	case SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED:
		{
			GPTP_LOG_INFO("SYNC_RATE_INTERVAL_TIMEOUT_EXPIRED occured");

			sync_rate_interval_timer_started = false;

			bool sendSignalMessage = false;
			if ( getSyncInterval() != operLogSyncInterval )
			{
				setSyncInterval( operLogSyncInterval );
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
							sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoChange, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoChange);
						else
							sigMsg->setintervals(log_min_mean_pdelay_req_interval, getSyncInterval(), PTPMessageSignalling::sigMsgInterval_NoChange);
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
		  ( "Unhandled event type in "
		    "EtherPort::processEvent(), %d", e );
		ret = false;
		break;
	}

	return ret;
}

void EtherPort::recoverPort( void )
{
	return;
}

void EtherPort::becomeMaster( bool annc ) {
	setPortState( PTP_MASTER );
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

void EtherPort::becomeSlave( bool restart_syntonization ) {
	clock->deleteEventTimerLocked( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

	setPortState( PTP_SLAVE );

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

void EtherPort::mapSocketAddr
( PortIdentity *destIdentity, LinkLayerAddress *remote )
{
	*remote = identity_map[*destIdentity];
	return;
}

void EtherPort::addSockAddrMap
( PortIdentity *destIdentity, LinkLayerAddress *remote )
{
	identity_map[*destIdentity] = *remote;
	return;
}

int EtherPort::getTxTimestamp
( PTPMessageCommon *msg, Timestamp &timestamp, unsigned &counter_value,
  bool last )
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getTxTimestamp
		(&identity, msg->getMessageId(), timestamp, counter_value, last);
}

int EtherPort::getRxTimestamp
( PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
  bool last )
{
	PortIdentity identity;
	msg->getPortIdentity(&identity);
	return getRxTimestamp
		(&identity, msg->getMessageId(), timestamp, counter_value, last);
}

int EtherPort::getTxTimestamp
(PortIdentity *sourcePortIdentity, PTPMessageId messageId,
 Timestamp &timestamp, unsigned &counter_value, bool last )
{
	EtherTimestamper *timestamper =
		dynamic_cast<EtherTimestamper *>(_hw_timestamper);
	if (timestamper)
	{
		return timestamper->HWTimestamper_txtimestamp
			( sourcePortIdentity, messageId, timestamp,
			  counter_value, last );
	}
	timestamp = clock->getSystemTime();
	return 0;
}

int EtherPort::getRxTimestamp
( PortIdentity * sourcePortIdentity, PTPMessageId messageId,
  Timestamp &timestamp, unsigned &counter_value, bool last )
{
	EtherTimestamper *timestamper =
		dynamic_cast<EtherTimestamper *>(_hw_timestamper);
	if (timestamper)
	{
		return timestamper->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, messageId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return 0;
}

void EtherPort::startPDelayIntervalTimer
( long long unsigned int waitTime )
{
	pDelayIntervalTimerLock->lock();
	clock->deleteEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked(this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	pDelayIntervalTimerLock->unlock();
}

void EtherPort::syncDone() {
	GPTP_LOG_VERBOSE("Sync complete");

	if (automotive_profile && getPortState() == PTP_SLAVE) {
		if (avbSyncState > 0) {
			avbSyncState--;
			if (avbSyncState == 0) {
				setStationState(STATION_STATE_AVB_SYNC);
				if (getTestMode()) {
					APMessageTestStatus *testStatusMsg =
						new APMessageTestStatus(this);
					if (testStatusMsg) {
						testStatusMsg->sendPort(this);
						delete testStatusMsg;
					}
				}
			}
		}
	}

	if (automotive_profile) {
		if (!sync_rate_interval_timer_started) {
			if ( getSyncInterval() != operLogSyncInterval )
			{
				startSyncRateIntervalTimer();
			}
		}
	}

	if( !pdelay_started ) {
		startPDelay();
	}
}
