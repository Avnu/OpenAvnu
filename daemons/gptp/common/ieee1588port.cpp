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

#include <inttypes.h>

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
	peer_identity = std::make_shared<PortIdentity>();
	port_identity = std::make_shared<PortIdentity>();

	last_sync = nullptr;

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

	smoothRateChange = portInit->smoothRateChange;
	fLastFilteredRateRatio = 1;

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

	if (iniParser.hasIniValues())
	{
		GPTP_LOG_VERBOSE("iniParser.hasIniValues");
		log_mean_sync_interval = iniParser.getInitialLogSyncInterval();
		log_mean_announce_interval = iniParser.getInitialLogAnnounceInterval();
		fUnicastSendNodeList = iniParser.UnicastSendNodes();
		fUnicastReceiveNodeList = iniParser.UnicastReceiveNodes();
	}
	else
	{
		GPTP_LOG_VERBOSE("NOT iniParser.hasIniValues");
		log_mean_sync_interval = initialLogSyncInterval;
		log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;
		log_mean_announce_interval = 0;
	}
	_current_clock_offset = _initial_clock_offset = portInit->offset;

	rate_offset_array = NULL;

	_hw_timestamper = portInit->timestamper;
	fUseHardwareTimestamp = true;

	fMeanPathDelay = 0.0;

	fFollowupAhead = false;
	fHaveFollowup = false;
	fHaveDelayResp = false;


	one_way_delay = 0; //ONE_WAY_DELAY_DEFAULT;
	neighbor_prop_delay_thresh = NEIGHBOR_PROP_DELAY_THRESH;
	sync_receipt_thresh = DEFAULT_SYNC_RECEIPT_THRESH;
	wrongSeqIDCounter = 0;

	_peer_rate_offset = 1.0;
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
			//_hw_timestamper = NULL;
			fUseHardwareTimestamp = false;
			return;
		}
	}
}

void IEEE1588Port::timestamper_reset(void)
{
	if( _hw_timestamper != NULL && fUseHardwareTimestamp) {
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

	port_identity->setClockIdentity(clock->getClockIdentity());
	port_identity->setPortNumber(&ifindex);

	GPTP_LOG_VERBOSE("IEEE1588Port::init_port   clockId:%s", port_identity->getClockIdentity().getIdentityString().c_str());

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
	if (port->_hw_timestamper && port->fUseHardwareTimestamp)
	{
		port->_hw_timestamper->get_phy_delay(&get_delay);
	}

	PTPMessageSync *sync = nullptr;
	uint16_t syncSeqId = 0;
	uint16_t fupSeqId = 0;
	uint16_t delRespSeqId = 0;

	int safetyNet;
	const int kSafetyStop = 20;
	std::shared_ptr<PortIdentity> delayRespPortId;
	std::shared_ptr<PortIdentity> fwupPortId;
	std::shared_ptr<PortIdentity> syncPortId;
	std::shared_ptr<PortIdentity> self = port->getPortIdentity();
	while (1)
	{
		// Check for event and then general messages and process them
		if (maybeProcessMessage(true, get_delay) != net_fatal)
		{
			sync = port->getLastSync();
			if (sync != nullptr)
			{
				syncPortId = sync->getPortIdentity();

				if (*syncPortId != *self)
				{
					syncSeqId = sync->getSequenceId();

					// Provide a safety net to prevent spinning on general messages
					// if a message is missed.
					safetyNet = 0;
					while (safetyNet < kSafetyStop && sync && 
					 (!port->HaveFollowup() || !port->HaveDelayResp()))
					{
						maybeProcessMessage(false, get_delay);
						++safetyNet;
						sync = port->getLastSync();
						syncSeqId = sync ? sync->getSequenceId() : 0;

						// Ensure that all sequence Ids match for all messages, if they don't
						// look for another sync message. Also check for messages from
						// self and ignore them. This is not a very robust mechanism - if
						// multiple clocks are present on the same network stange behaviors
						// will result.
						delRespSeqId = port->last_delay_resp.getSequenceId();
						fupSeqId = port->last_fwup.getSequenceId();									
						fwupPortId = port->last_fwup.getPortIdentity();
						delayRespPortId = port->last_delay_resp.getPortIdentity();
						syncPortId = sync ? sync->getPortIdentity() : std::make_shared<PortIdentity>();
						
						if (syncPortId && *syncPortId == *self)
						{
							// We got a sync message from ourself get the next one
							GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> SYNC from self detected...getting next sync.");
							maybeProcessMessage(true, get_delay);
							continue;
						}

						if ((fwupPortId && *fwupPortId == *self) ||
						 (delayRespPortId && *delayRespPortId == *self))
						{
							GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> fwup or delayresp clocks from self.");
							continue;
						}

						if ((sync != nullptr && sync->getSequenceId() != syncSeqId) ||
							(port->HaveFollowup() && fupSeqId != syncSeqId) ||
							(port->HaveDelayResp() && delRespSeqId != syncSeqId))
						{
							GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Non matching sequence ids detected!");
							GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> syncSeqId:%d  fupSeqId:%d  delRespSeqId:%d", syncSeqId, fupSeqId, delRespSeqId);
							GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> syncClockId:%s  fwupClockId:%s  lastDealyRespClockId:%s", syncPortId->getClockIdentity().getIdentityString().c_str(), fwupPortId->getClockIdentity().getIdentityString().c_str(), delayRespPortId->getClockIdentity().getIdentityString().c_str());
							delete sync;
							port->setLastSync(NULL);

							if (port->HaveFollowup() && syncSeqId < fupSeqId)
							{
								GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Breaking general loop  syncSeqId < fupSeqId");
								port->HaveDelayResp(false);
								port->FollowupAhead(true);
								maybeProcessMessage(true, get_delay);
								//break;
							}
							else if (port->HaveFollowup() && fupSeqId < syncSeqId)
							{
								GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Getting next General message fupSeqId < syncSeqId");
								// The general messages got behind the event messages
								port->HaveFollowup(false);
							}
							//syncSeqId:74  fupSeqId:75  delRespSeqId:73
							else if (port->HaveDelayResp() && delRespSeqId < syncSeqId)
							{
								GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Getting next General message delRespSeqId < syncSeqId");
								// The general messages got behind the event messages
								port->HaveDelayResp(false);
							}
							else if ((port->HaveFollowup() && fupSeqId > syncSeqId) ||
							 (port->HaveDelayResp() && delRespSeqId > syncSeqId))
							{
								GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Getting next sequence");
								// We got the next followup before the next sync or the next
								// delay response before the next sync. We need to get the next
								// sync.
								maybeProcessMessage(true, get_delay);
							}
							else
							{
								GPTP_LOG_ERROR(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Stopping search for General messages");
								port->HaveFollowup(false);
								port->HaveDelayResp(false);
								break;
							}
						}
					}

				}
				if (kSafetyStop == safetyNet)
				{
					//GPTP_LOG_ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! SafetyNet !!!!!!!!!!!!!");
				}
			}
		}
		else
		{
			GPTP_LOG_ERROR("net fatal checking for event messages.");
			break;
		}
	}

	GPTP_LOG_INFO("IEEE1588Port::openPort  DONE");

	return NULL;
}

net_result IEEE1588Port::maybeProcessMessage(bool checkEventMessage, phy_delay& delay)
{
	PTPMessageCommon *msg;
	uint8_t buf[128];
	LinkLayerAddress remote;
	net_result rrecv;
	size_t length = sizeof(buf);
	Timestamp ingressTime;
	
	rrecv = checkEventMessage
	 ? net_iface->nrecvEvent(&remote, buf, length, &delay, ingressTime, this)
	 : net_iface->nrecvGeneral(&remote, buf, length,&delay, ingressTime, this);

	if (net_succeed == rrecv)
	{
		GPTP_LOG_VERBOSE("Processing network buffer");
		msg = buildPTPMessage((char *)buf, (int)length, &remote,
				    this, ingressTime);
		if (msg != NULL)
		{
			GPTP_LOG_VERBOSE("Processing message");
			GPTP_LOG_VERBOSE("IEEE1588Port::maybeProcessMessage   clockId:%s", port_identity->getClockIdentity().getIdentityString().c_str());
			msg->processMessage(this);
			if (msg->garbage())
			{
				delete msg;
			}
		} 
		else
		{
			GPTP_LOG_ERROR("Discarding invalid message");
		}
	}
	else if (rrecv == net_fatal)
	{
		GPTP_LOG_ERROR("read from network interface failed");
		this->processEvent(FAULT_DETECTED);
	}
	return rrecv;
}

net_result IEEE1588Port::port_send(uint16_t etherType, uint8_t * buf, int size,
 MulticastType mcast_type, std::shared_ptr<PortIdentity> destIdentity, 
 bool timestamp, uint16_t port)
{
	net_result ok;
	LinkLayerAddress dest;

	if (mcast_type != MCAST_NONE)
	{
		if (mcast_type == MCAST_PDELAY) {
			dest = pdelay_multicast;
		}
		else if (mcast_type == MCAST_TEST_STATUS) {
			dest = test_status_multicast;
		}
		else {
			dest = other_multicast;
		}
	}
	else
	{
		if (!destIdentity && !fUnicastSendNodeList.empty())
		{
			ok = net_succeed;

			// Use the unicast send node list if it is present
			std::for_each(fUnicastSendNodeList.begin(), fUnicastSendNodeList.end(), 
			 [&](const std::string& address)
			 {
			 	 GPTP_LOG_VERBOSE("IEEE1588Port::port_send   address: %s, port:%d", address.c_str(), port);
				 LinkLayerAddress dest(address, port);
				 net_result status = net_iface->send(&dest, etherType, buf, size,
				  timestamp);
				 ok = status != net_succeed ? status : ok;
			 });
		}
		else
		{
			mapSocketAddr(destIdentity, &dest);
			dest.Port(port);
			uint8_t debugOctets[6];
			dest.toOctetArray(debugOctets);
			GPTP_LOG_VERBOSE("dest: %d %d %d %d %d %d", debugOctets[0],debugOctets[1],debugOctets[2],debugOctets[3],
				debugOctets[4],debugOctets[5]);			
			ok = net_iface->send(&dest, etherType, (uint8_t *) buf, size, timestamp);
		}
	}

	return ok;
}

unsigned IEEE1588Port::getPayloadOffset()
{
	return net_iface->getPayloadOffset();
}

void IEEE1588Port::sendEventPort(uint16_t etherType, uint8_t * buf, int size,
 MulticastType mcast_type, std::shared_ptr<PortIdentity> destIdentity)
{	
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, true, 319);
	if (rtx != net_succeed) {
		GPTP_LOG_ERROR("sendEventPort(): failure");
	}

	return;
}

void IEEE1588Port::sendGeneralPort(uint16_t etherType, uint8_t * buf, int size,
 MulticastType mcast_type, std::shared_ptr<PortIdentity> destIdentity)
{
	net_result rtx = port_send(etherType, buf, size, mcast_type, destIdentity, false, 320);
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

			GPTP_LOG_VERBOSE("Starting port thread");
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
			{
				       GPTP_LOG_VERBOSE("IEEE1588Port::processEvent (powerup/initialize)  adding "
        "ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES  announceInterval:%d", getAnnounceInterval());

				clock->addEventTimerLocked(this, e4, interval4);
			}
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
				GPTP_LOG_VERBOSE("Sending initial signalling message");
				// Send an initial signalling message
				PTPMessageSignalling *sigMsg = new PTPMessageSignalling(this);
				if (sigMsg) {
					sigMsg->setintervals(PTPMessageSignalling::sigMsgInterval_NoSend, log_mean_sync_interval, PTPMessageSignalling::sigMsgInterval_NoSend);
					sigMsg->sendPort(this, nullptr);
					delete sigMsg;
				}

				GPTP_LOG_VERBOSE("POWERUP/INITIALIZE   getSyncInterval:%d", getSyncInterval());
				startSyncReceiptTimer((unsigned long long)
					 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
					  ((double) pow((double)2, getSyncInterval()) *
					   1000000000.0)));
			}
		}

		break;

	case STATE_CHANGE_EVENT:
		GPTP_LOG_DEBUG("Received STATE CHANGE event");

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
						GPTP_LOG_VERBOSE("AFTER recommendState  port_state:%d", port_state);
					} else {
						if( EBest == ports[j]->calculateERBest() ) {
							// The "best" Announce was recieved on this port
							ClockIdentity clock_identity;
							unsigned char priority1;
							unsigned char priority2;
							ClockQuality *clock_quality;

							ports[j]->recommendState
							  ( PTP_SLAVE, changed_external_master );
							GPTP_LOG_VERBOSE("AFTER recommendState  port_state:%d", port_state);

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
							GPTP_LOG_VERBOSE("AFTER recommendState  port_state:%d", port_state);
						}
					}
				}
			}
		}
		break;

	case LINKUP:
		GPTP_LOG_DEBUG("Received LINKUP event");

		haltPdelay(false);
		startPDelay();
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
					sigMsg->sendPort(this, nullptr);
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
		GPTP_LOG_DEBUG("Received LINKDOWN event");
		stopPDelay();
		if (automotive_profile) {
			GPTP_LOG_EXCEPTION("LINK DOWN");
		}
		else {
			setAsCapable(false);
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
				GPTP_LOG_DEBUG("Received ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES event");
				incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts();
			}
			else if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
				GPTP_LOG_DEBUG("Received SYNC_RECEIPT_TIMEOUT_EXPIRES event");
				incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts();
			}
			if (!automotive_profile) {

				if( clock->getPriority1() == 255 ) {
					// Restart timer
					       GPTP_LOG_VERBOSE("IEEE1588Port::processEvent (ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES/SYNC) adding "
        "ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES  announceInterval:%d", getAnnounceInterval());

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
					GPTP_LOG_VERBOSE("port_state SET to MASTER!!!!!");
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
#ifndef APTP
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
			std::shared_ptr<PortIdentity> dest_id;
			getPortIdentity(dest_id);
			pdelay_req->setPortIdentity(dest_id);

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
				GPTP_LOG_DEBUG(
					"PDelay Request message, Timestamp %u (sec) %u (ns), seqID %u",
					req_timestamp.seconds_ls, req_timestamp.nanoseconds,
					pdelay_req->getSequenceId());
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
				GPTP_LOG_DEBUG("Schedule PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, "
					"PDelay interval %d, wait_time %lld, timeout %lld",
					getPDelayInterval(), wait_time, timeout);

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
#else
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
	break;
#endif // ifndef APTP
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
#ifndef APTP
			// The aPTP profile can't support asCapable
			if (asCapable)
#endif
			{
				PTPMessageSync sync(this);
				std::shared_ptr<PortIdentity> dest_id = std::make_shared<PortIdentity>();
				getPortIdentity(dest_id);
				sync.setPortIdentity(dest_id);
				getTxLock();
				sync.sendPort(this, nullptr);
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
					getTxTimestamp(&sync, sync_timestamp,
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
						(&sync, sync_timestamp,
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
		
				if (ts_good == GPTP_EC_SUCCESS)
				{

					PTPMessageFollowUp follow_up(this);
					std::shared_ptr<PortIdentity> dest_id = std::make_shared<PortIdentity>();
					getPortIdentity(dest_id);

					follow_up.setClockSourceTime(getClock()->getFUPInfo());
					follow_up.setPortIdentity(dest_id);
					follow_up.setSequenceId(sync.getSequenceId());
					follow_up.setPreciseOriginTimestamp(sync_timestamp);
					follow_up.sendPort(this, nullptr);
				} else {
				}
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
			   pdelay_count, port_state, asCapable, fAdrRegSocketIp,
			   fAdrRegSocketPort);

			syncDone();

			wait_time = ((long long) (pow((double)2, getSyncInterval()) * 1000000000.0));
#ifndef APTP			
			wait_time = wait_time > EVENT_TIMER_GRANULARITY ? wait_time : EVENT_TIMER_GRANULARITY;
#endif			
			GPTP_LOG_VERBOSE("Before startSyncReceiptTimer  wait_time:%" PRIu64 "syncInterval:%d", wait_time, getSyncInterval());
			startSyncIntervalTimer(wait_time);

		}
		break;
	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES occured  port_state:%d", port_state);
#ifndef APTP
		if (asCapable)
#else
		if (port_state != PTP_SLAVE)
#endif
		{
			// Send an announce message
			PTPMessageAnnounce annc = PTPMessageAnnounce(this);
			std::shared_ptr<PortIdentity> dest_id = getPortIdentity();
			annc.setPortIdentity(dest_id);
			annc.sendPort(this, nullptr);
			//!!! Add aPTP TLV signal !!!
#ifdef APTP	
    		startAnnounceIntervalTimer(1000000000);
#endif		        	
		}

#ifndef APTP	
		startAnnounceIntervalTimer((uint64_t)(pow((double)2, getAnnounceInterval()) * 1000000000.0));
#endif		
		break;
	case FAULT_DETECTED:
		GPTP_LOG_ERROR("Received FAULT_DETECTED event");
		if (!automotive_profile) {
			setAsCapable(false);
		}
		break;
#ifndef APTP
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
		GPTP_LOG_DEBUG("Received PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES event");
		if (!automotive_profile) {
			GPTP_LOG_EXCEPTION("PDelay Response Receipt Timeout");
			setAsCapable(false);
		}
		pdelay_count = 0;
		break;

	case PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("Received PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES event");
		GPTP_LOG_EXCEPTION("PDelay Resp Peer Misbehaving timeout expired! Restarting PDelay");

		haltPdelay(false);
		if( port_state != PTP_SLAVE && port_state != PTP_MASTER ) {
			GPTP_LOG_STATUS("Starting PDelay" );
			startPDelay();
		}
		break;
#endif // ifndef APTP
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
						sigMsg->sendPort(this, nullptr);
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
	if (_hw_timestamper && fUseHardwareTimestamp) {
		_hw_timestamper->HWTimestamper_gettime
			(&system_time, &device_time, &local_clock, &nominal_clock_rate);
	} else {
		device_time = system_time = clock->getSystemTime();
		local_clock = nominal_clock_rate = 0;
	}
	return;
}

void IEEE1588Port::becomeMaster( bool annc ) {
	GPTP_LOG_VERBOSE("#####~~~~####~~~~####~~~~####~~~~####  IEEE1588Port::becomeMaster");
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
	int64_t interval = 
#ifdef APTP
	125000000;
#else
	16000000;
#endif
	startSyncIntervalTimer(interval);
	GPTP_LOG_STATUS("Switching to Master" );

	clock->updateFUPInfo();

	return;
}

void IEEE1588Port::becomeSlave( bool restart_syntonization ) {
	clock->deleteEventTimerLocked( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

	port_state = PTP_SLAVE;

	if (!automotive_profile) {
       GPTP_LOG_VERBOSE("IEEE1588Port::becomeSlave  adding "
        "ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES  announceInterval:%d", getAnnounceInterval());

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
   GPTP_LOG_VERBOSE("IEEE1588Port::recommendState   port_state:%d", port_state);
 }

void IEEE1588Port::mapSocketAddr
(std::shared_ptr<PortIdentity> destIdentity, LinkLayerAddress * remote)
{
	*remote = identity_map[destIdentity];
	return;
}

void IEEE1588Port::addSockAddrMap
(std::shared_ptr<PortIdentity> destIdentity, LinkLayerAddress * remote)
{
	identity_map[destIdentity] = *remote;
	return;
}

int IEEE1588Port::getTxTimestamp
(PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
 bool last)
{
	std::shared_ptr<PortIdentity> identity;
	msg->getPortIdentity(identity);
	return getTxTimestamp
		(identity, msg->getMessageId(), timestamp, counter_value, last);
}

int IEEE1588Port::getRxTimestamp(PTPMessageCommon * msg, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	std::shared_ptr<PortIdentity> identity;
	msg->getPortIdentity(identity);
	return getRxTimestamp
		(identity, msg->getMessageId(), timestamp, counter_value, last);
}

int IEEE1588Port::getTxTimestamp(std::shared_ptr<PortIdentity> sourcePortIdentity,
				 PTPMessageId messageId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper && fUseHardwareTimestamp) {
		return _hw_timestamper->HWTimestamper_txtimestamp
			(sourcePortIdentity, messageId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	return 0;
}

int IEEE1588Port::getRxTimestamp(std::shared_ptr<PortIdentity> sourcePortIdentity,
				 PTPMessageId messageId, Timestamp & timestamp,
				 unsigned &counter_value, bool last)
{
	if (_hw_timestamper && fUseHardwareTimestamp) {
		return _hw_timestamper->HWTimestamper_rxtimestamp
		    (sourcePortIdentity, messageId, timestamp, counter_value,
		     last);
	}
	timestamp = clock->getSystemTime();
	GPTP_LOG_VERBOSE("getRxTimestamp timestamp = %d,%d,%d", timestamp.seconds_ms, timestamp.seconds_ls, timestamp.nanoseconds );
	return 0;
}

void IEEE1588Port::startSyncReceiptTimer(long long unsigned int waitTime) {
	GPTP_LOG_VERBOSE("IEEE1588Port::startSyncReceiptTimer   waitTime:%d", waitTime);
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
	GPTP_LOG_VERBOSE("IEEE1588Port::startSyncIntervalTimer   waitTime:%" PRIu64, waitTime);
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

