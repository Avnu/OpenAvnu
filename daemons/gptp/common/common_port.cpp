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
#include <inttypes.h>

#include "common_port.hpp"
#include "avbts_clock.hpp"
#include "common_tstamper.hpp"
#include "gptp_cfg.hpp"


CommonPort::CommonPort( PortInit_t *portInit ) :
	thread_factory( portInit->thread_factory ),
	timer_factory( portInit->timer_factory ),
	lock_factory( portInit->lock_factory ),
	condition_factory( portInit->condition_factory ),
	_hw_timestamper( portInit->timestamper ),
	clock( portInit->clock ),
	isGM( portInit->isGM ),
	phy_delay( portInit->phy_delay )
{
	port_identity = std::make_shared<PortIdentity>();

	one_way_delay = ONE_WAY_DELAY_DEFAULT;
	neighbor_prop_delay_thresh = portInit->neighborPropDelayThreshold;
	net_label = portInit->net_label;
	link_thread = thread_factory->createThread();
	listening_thread = thread_factory->createThread();
	sync_receipt_thresh = portInit->syncReceiptThreshold;
	wrongSeqIDCounter = 0;
	_peer_rate_offset = 1.0;
	_peer_offset_init = false;
	ifindex = portInit->index;
	testMode = false;
	port_state = PTP_INITIALIZING;
	if (clock != nullptr)
	{
		clock->registerPort(this, ifindex);
	}
	announce_sequence_id = 0;
	signal_sequence_id = 0;
	sync_sequence_id = 0;
	initialLogSyncInterval = portInit->initialLogSyncInterval;
	log_mean_announce_interval = 0;
	pdelay_count = 0;
	asCapable = false;
	link_speed = INVALID_LINKSPEED;
	fUseHardwareTimestamp = true;
	fSyncIntervalTimeoutExpireCount = 0;
}

CommonPort::~CommonPort()
{
	delete link_thread;
	delete listening_thread;
	delete syncReceiptTimerLock;
	delete syncIntervalTimerLock;
	delete announceIntervalTimerLock;
}

bool CommonPort::init_port( void )
{
	log_mean_sync_interval = initialLogSyncInterval;

	if (!OSNetworkInterfaceFactory::buildInterface
	    ( &net_iface, factory_name_t("default"), net_label,
	      _hw_timestamper))
		return false;

	if (clock != nullptr)
	{
		this->net_iface->getMacAddress(&local_addr);
		clock->setClockIdentity(local_addr);
		port_identity->setClockIdentity(clock->getClockIdentity());
	}

	this->timestamper_init();

	port_identity->setPortNumber(&ifindex);

	syncReceiptTimerLock = lock_factory->createLock(oslock_recursive);
	syncIntervalTimerLock = lock_factory->createLock(oslock_recursive);
	announceIntervalTimerLock = lock_factory->createLock(oslock_recursive);

	return _init_port();
}

void CommonPort::setClock(IEEE1588Clock * otherClock)
{
	clock = otherClock;
	if (clock != nullptr)
	{
		clock->registerPort(this, ifindex);
		net_iface->getMacAddress(&local_addr);
		clock->setClockIdentity(local_addr);
		port_identity->setClockIdentity(clock->getClockIdentity());
	}
}

void CommonPort::timestamper_init()
{
	if (_hw_timestamper != nullptr && 
	 !_hw_timestamper->HWTimestamper_init(net_label, net_iface))
	{
		GPTP_LOG_ERROR("Failed to initialize hardware timestamper, "
		 "falling back to software timestamping");
	}
}

void CommonPort::timestamper_reset( void )
{
	if (_hw_timestamper != nullptr && fUseHardwareTimestamp)
	{
		_hw_timestamper->HWTimestamper_reset();
	}
}

void CommonPort::recommendState
( PortState state, bool changed_external_master )
{
	bool reset_sync = false;
	switch (state) {
	case PTP_MASTER:
		if ( getPortState() != PTP_MASTER )
		{
			setPortState( PTP_MASTER );
			// Start announce receipt timeout timer
			// Start sync receipt timeout timer
			becomeMaster( true );
			reset_sync = true;
		}
		break;
	case PTP_SLAVE:
		if ( getPortState() != PTP_SLAVE )
		{
			becomeSlave( true );
			reset_sync = true;
		} else {
			if( changed_external_master ) {
				GPTP_LOG_STATUS("Changed master!" );
				clock->newSyntonizationSetPoint();
				clock->updateFUPInfo();
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

bool CommonPort::serializeState( void *buf, off_t *count )
{
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

bool CommonPort::restoreSerializedState
( void *buf, off_t *count )
{
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

void CommonPort::startSyncReceiptTimer
( long long unsigned int waitTime )
{
	syncReceiptTimerLock->lock();
	clock->deleteEventTimerLocked( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
	clock->addEventTimerLocked
		( this, SYNC_RECEIPT_TIMEOUT_EXPIRES, waitTime );
	syncReceiptTimerLock->unlock();
}

void CommonPort::stopSyncReceiptTimer( void )
{
	syncReceiptTimerLock->lock();
	clock->deleteEventTimerLocked( this, SYNC_RECEIPT_TIMEOUT_EXPIRES );
	syncReceiptTimerLock->unlock();
}

void CommonPort::startSyncIntervalTimer
( long long unsigned int waitTime )
{
	if( syncIntervalTimerLock->trylock() == oslock_fail ) return;
	clock->deleteEventTimerLocked(this, SYNC_INTERVAL_TIMEOUT_EXPIRES);
	clock->addEventTimerLocked
		(this, SYNC_INTERVAL_TIMEOUT_EXPIRES, waitTime);
	syncIntervalTimerLock->unlock();
}

void CommonPort::startAnnounceIntervalTimer
( long long unsigned int waitTime )
{
	announceIntervalTimerLock->lock();
	clock->deleteEventTimerLocked
		( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->addEventTimerLocked
		( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES, waitTime );
	announceIntervalTimerLock->unlock();
}

bool CommonPort::processStateChange( Event e )
{
	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	 clock->priority1:%d  clock->clockIdentity:%s",
	 clock->getPriority1(), clock->getClockIdentity().getString().c_str());

// In APTP mode we need to update clockid if the master clock changes
#ifndef APTP
	// Nothing to do if we are slave only
	if (clock->getPriority1() == 255)
		return true;
#endif

	int number_ports, j;
	PTPMessageAnnounce *EBest = NULL;
	char EBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];

	CommonPort **ports;
	clock->getPortList(number_ports, ports);

		/* Find EBest for all ports */
	for (j = 0; j < number_ports; ++j) 
	{
		while (ports[j] == NULL && j < number_ports)
			++j;
		if (number_ports == j)
		{
			break;
		}
		if (ports[j]->getPortState() == PTP_DISABLED ||
		 ports[j]->getPortState() == PTP_FAULTY)
		{
			continue;
		}
		if (EBest == NULL)
		{
			EBest = ports[j]->calculateERBest();
		}
		else if (ports[j]->calculateERBest())
		{
			if (ports[j]->calculateERBest()->isBetterThan(EBest))
			{
				EBest = ports[j]->calculateERBest();
			}
		}
	}

	if (EBest == NULL)
	{
		return true;
	}

	bool changed_external_master = false;

	/* Check if we've changed */
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
	}
	else
	{
		changed_external_master = false;
	}
	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	changed_external_master:%s",(changed_external_master ? "true" : "false"));

	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	EBest(announce)  START");
	EBest->VerboseLog();
	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	EBest(announce)  END");

	if( clock->isBetterThan( EBest ))
	{
		// We're Grandmaster, set grandmaster info to me
		ClockIdentity clock_identity;
		unsigned char priority1;
		unsigned char priority2;
		ClockQuality clock_quality;

		clock_identity = getClock()->getClockIdentity();
		getClock()->setGrandmasterClockIdentity( clock_identity );
		GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	we are master!!!");
		priority1 = getClock()->getPriority1();
		getClock()->setGrandmasterPriority1( priority1 );
		priority2 = getClock()->getPriority2();
		getClock()->setGrandmasterPriority2( priority2 );
		clock_quality = getClock()->getClockQuality();
		getClock()->setGrandmasterClockQuality( clock_quality );
	}

	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	number_ports:%d",number_ports);

	for (int j = 0; j < number_ports; ++j)
	{
		while (nullptr == ports[j] && j < number_ports)
			++j;
		if (number_ports == j)
		{
			break;
		}
		if (ports[j]->getPortState() == PTP_DISABLED ||
		 ports[j]->getPortState() == PTP_FAULTY)
		{
			continue;
		}
		if (clock->isBetterThan(EBest))
		{
			// We are the GrandMaster, all ports are master
			EBest = NULL;	// EBest == NULL : we were grandmaster
			ports[j]->recommendState(PTP_MASTER, changed_external_master);
			GPTP_LOG_VERBOSE("AFTER recommendState 1 port_state:%d", port_state);
		}
		else
		{
			if( EBest == ports[j]->calculateERBest() )
			{
				// The "best" Announce was recieved on this port
				ClockIdentity clock_identity;
				unsigned char priority1;
				unsigned char priority2;
				ClockQuality *clock_quality;

				ports[j]->recommendState(PTP_SLAVE, changed_external_master);
				GPTP_LOG_VERBOSE("AFTER recommendState  2 port_state:%d", port_state);

				clock_identity = EBest->getGrandmasterClockIdentity();
				getClock()->setGrandmasterClockIdentity(clock_identity);
				priority1 = EBest->getGrandmasterPriority1();
				getClock()->setGrandmasterPriority1( priority1 );
				priority2 = EBest->getGrandmasterPriority2();
				getClock()->setGrandmasterPriority2( priority2 );
				clock_quality = EBest->getGrandmasterClockQuality();
				getClock()->setGrandmasterClockQuality(*clock_quality);
			}
			else
			{
				/* Otherwise we are the master because we have sync'd to a better clock */
				ports[j]->recommendState(PTP_MASTER, changed_external_master);
				GPTP_LOG_VERBOSE("AFTER recommendState  3 port_state:%d", port_state);
			}
		}
	}
	GPTP_LOG_VERBOSE("STATE_CHANGE_EVENT	END");

	return true;
}


bool CommonPort::processSyncAnnounceTimeout( Event e )
{
	// We're Grandmaster, set grandmaster info to me
	ClockIdentity clock_identity;
	unsigned char priority1;
	unsigned char priority2;
	ClockQuality clock_quality;

	Timestamp system_time;
	Timestamp device_time;
	uint32_t local_clock, nominal_clock_rate;

	// Nothing to do
	if( clock->getPriority1() == 255 )
		return true;

	// Restart timer
	if( e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ) {
		clock->addEventTimerLocked
			(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
			 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
			  (unsigned long long)
			  (pow((double)2,getAnnounceInterval())*
			   1000000000.0)));
	} else {
		startSyncReceiptTimer
			((unsigned long long)
			 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
			  ((double) pow((double)2, getSyncInterval()) *
			   1000000000.0)));
	}

	if( getPortState() == PTP_MASTER )
		return true;

	GPTP_LOG_STATUS(
		"*** %s Timeout Expired - Becoming Master",
		e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES ? "Announce" :
		"Sync" );

	clock_identity = getClock()->getClockIdentity();
	getClock()->setGrandmasterClockIdentity( clock_identity );
	priority1 = getClock()->getPriority1();
	getClock()->setGrandmasterPriority1( priority1 );
	priority2 = getClock()->getPriority2();
	getClock()->setGrandmasterPriority2( priority2 );
	clock_quality = getClock()->getClockQuality();
	getClock()->setGrandmasterClockQuality( clock_quality );

	setPortState( PTP_MASTER );

	getDeviceTime( system_time, device_time,
		       local_clock, nominal_clock_rate );

	(void) clock->calcLocalSystemClockRateDifference
		( device_time, system_time );

	setQualifiedAnnounce(nullptr);

	clock->addEventTimerLocked
		( this, SYNC_INTERVAL_TIMEOUT_EXPIRES,
		  16000000 );

	startAnnounce();

	return true;
}

void CommonPort::MasterOffset(FrequencyRatio offset)
{
	fMasterOffset = offset;
	if (_hw_timestamper)
	{
		_hw_timestamper->MasterOffset(offset);
	}
	else
	{
		GPTP_LOG_ERROR("_hw_timestamper IS NULL!!!!!!!!!!!!!!!!!!!!!!!");
	}
}

void CommonPort::AdjustedTime(FrequencyRatio t)
{
	if (_hw_timestamper)
	{
		_hw_timestamper->AdjustedTime(t);
	}
	else
	{
		GPTP_LOG_ERROR("_hw_timestamper IS NULL!!!!!!!!!!!!!!!!!!!!!!!");
	}
}

bool CommonPort::processEvent( Event e )
{
	bool ret;

	switch( e )
	{
	default:
		// Unhandled event
		ret = _processEvent( e );
		break;

	case POWERUP:
	case INITIALIZE:
		GPTP_LOG_DEBUG("Received POWERUP/INITIALIZE event");

		// If port has been configured as master or slave, run media
		// specific configuration. If it hasn't been configured
		// start listening for announce messages
		if( clock->getPriority1() == 255 ||
		    port_state == PTP_SLAVE )
		{
			becomeSlave( true );
		}
		else if( port_state == PTP_MASTER )
		{
			becomeMaster( true );
		}
		else
		{
			clock->addEventTimerLocked(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
				ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * pow(2.0, getAnnounceInterval()) * 1000000000.0);
		}

		// Do any media specific initialization
		ret = _processEvent( e );
		break;

	case STATE_CHANGE_EVENT:
		ret = _processEvent( e );

		// If this event wasn't handled in a media specific way, call
		// the default action
		if( !ret )
			ret = processStateChange( e );
		break;

	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		if (e == ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES) {
			GPTP_LOG_DEBUG("Received ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES event");
			incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts();
		}
		else if (e == SYNC_RECEIPT_TIMEOUT_EXPIRES) {
			GPTP_LOG_DEBUG("Received SYNC_RECEIPT_TIMEOUT_EXPIRES event");
			incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts();
		}

		ret = _processEvent( e );

		// If this event wasn't handled in a media specific way, call
		// the default action
		if( !ret )
			ret = processSyncAnnounceTimeout( e );
		break;

	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		GPTP_LOG_DEBUG("ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES occured");

		ret = true;
#ifndef APTP
		if (asCapable)
#else
		if (port_state != PTP_SLAVE)
#endif
		{
			ret = _processEvent(e);
#ifdef APTP	
    		startAnnounceIntervalTimer(1000000000);
#endif		
		}

#ifndef APTP
		startAnnounceIntervalTimer
			((uint64_t)( pow((double)2, getAnnounceInterval()) *
				     1000000000.0 ));
#endif
		break;

	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		// If asCapable is true attempt some media specific action
		ret = true;

#ifndef APTP
		if( asCapable )
			ret = _processEvent( e );
#endif
		/* Do getDeviceTime() after transmitting sync frame
		   causing an update to local/system timestamp */
#ifdef APTP
		GPTP_LOG_DEBUG("SYNC_INTERVAL_TIMEOUT_EXPIRES occured  "
		 "fSyncIntervalTimeoutExpireCount:%d", fSyncIntervalTimeoutExpireCount);
		if (fSyncIntervalTimeoutExpireCount > 2)
#endif
		{
#ifdef APTP
			ret = _processEvent( e );
#endif
			Timestamp system_time;
			Timestamp device_time;
			long long wait_time = 0;
			uint32_t local_clock, nominal_clock_rate;

			getDeviceTime
				( system_time, device_time,
				  local_clock, nominal_clock_rate );

			GPTP_LOG_VERBOSE
				( "port::processEvent(): System time: %u,%u "
				  "Device Time: %u,%u",
				  system_time.seconds_ls,
				  system_time.nanoseconds,
				  device_time.seconds_ls,
				  device_time.nanoseconds );

#ifndef APTP			
			FrequencyRatio local_system_freq_offset;
			int64_t local_system_offset;

			local_system_offset =
				TIMESTAMP_TO_NS(system_time) -
				TIMESTAMP_TO_NS(device_time);
			local_system_freq_offset =
				clock->calcLocalSystemClockRateDifference
				( device_time, system_time );

			std::string clockId = PTP_MASTER == getPortState()
			 ? getPortIdentity()->getClockIdentity().getString()
			 : getClock()->getGrandmasterClockIdentity().getString();
			
			uint64_t grandMasterId = clockId.empty() ? 0 : std::stoull(clockId, 0, 16);

			GPTP_LOG_DEBUG("SYNC_INTERVAL_TIMEOUT_EXPIRES    grandMasterId:%s", clockId.c_str());
			
			clock->setMasterOffset(this, 0, device_time, 1.0, local_system_offset,
			 system_time, local_system_freq_offset, sync_count,
			 pdelay_count, port_state, asCapable, fAdrRegSocketPort,
			 grandMasterId);
#endif
			syncDone();

			wait_time = ((long long) (pow((double)2, getSyncInterval()) * 1000000000.0));
			GPTP_LOG_VERBOSE("Before startSyncReceiptTimer  wait_time:%" PRIu64 "syncInterval:%d", wait_time, getSyncInterval());
			startSyncIntervalTimer(wait_time);
		}
#ifdef APTP
		else
		{
			++fSyncIntervalTimeoutExpireCount;
			long long wait_time = ((long long) (pow((double)2, getSyncInterval()) * 1000000000.0));
			startSyncIntervalTimer(wait_time);
		}
#endif
		break;
	}

	return ret;
}

void CommonPort::getDeviceTime(Timestamp &system_time, Timestamp &device_time,
  uint32_t &local_clock, uint32_t &nominal_clock_rate)
{
	if (_hw_timestamper && fUseHardwareTimestamp)
	{
		_hw_timestamper->HWTimestamper_gettime(&system_time, &device_time,
		 &local_clock, &nominal_clock_rate);
	}
	else
	{
		device_time = system_time = clock->getSystemTime();
		local_clock = nominal_clock_rate = 0;
	}
}

void CommonPort::startAnnounce()
{
#ifdef APTP	
	startAnnounceIntervalTimer(1000000000);
#else
	startAnnounceIntervalTimer(16000000);
#endif
}

int CommonPort::getTimestampVersion()
{
	return _hw_timestamper->getVersion();
}

bool CommonPort::_adjustClockRate( FrequencyRatio freq_offset )
{
	if( _hw_timestamper )
	{
		return _hw_timestamper->HWTimestamper_adjclockrate
			((float) freq_offset );
	}

	return false;
}

void CommonPort::getExtendedError( char *msg )
{
	if (_hw_timestamper)
	{
		_hw_timestamper->HWTimestamper_get_extderror(msg);
		return;
	}

	*msg = '\0';
}

bool CommonPort::adjustClockPhase( int64_t phase_adjust )
{
	if( _hw_timestamper )
		return _hw_timestamper->
			HWTimestamper_adjclockphase( phase_adjust );

	return false;
}

Timestamp CommonPort::getTxPhyDelay( uint32_t link_speed ) const
{
	if( phy_delay->count( link_speed ) != 0 )
		return Timestamp
			( phy_delay->at(link_speed).get_tx_delay(), 0, 0 );

	return Timestamp(0, 0, 0);
}

Timestamp CommonPort::getRxPhyDelay( uint32_t link_speed ) const
{
	if( phy_delay->count( link_speed ) != 0 )
		return Timestamp
			( phy_delay->at(link_speed).get_rx_delay(), 0, 0 );

	return Timestamp(0, 0, 0);
}
