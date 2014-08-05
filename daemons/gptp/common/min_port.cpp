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

#include <min_port.hpp>
#include <avbts_message.hpp>
#include <avbts_clock.hpp>

#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_oscondition.hpp>

#include <stdio.h>

#include <math.h>

#include <stdlib.h>

#include <utility>

#ifdef WIN32
#include <Windows.h>
#endif

LinkLayerAddress MediaIndependentPort::other_multicast(OTHER_MULTICAST);
uint16_t MediaIndependentPort::port_index = 1;

void MediaIndependentPort::addQualifiedAnnounce(PTPMessageAnnounce *msg) {
	if( qualified_announce != NULL ) delete qualified_announce;
	qualified_announce = msg;
}

bool MediaIndependentPort::openPort(void)
{
	return inferior->openPort( port_ready_condition );
}
bool MediaIndependentPort::recoverPort() {
	return inferior->recoverPort();
}

MediaIndependentPort::
~MediaIndependentPort()
{
	if( qualified_announce != NULL ) delete qualified_announce;

	delete port_ready_condition;
}

MediaIndependentPort::
MediaIndependentPort
(
 IEEE1588Clock * clock, int accelerated_sync_count, bool syntonize,
 OSConditionFactory * condition_factory,
 OSThreadFactory * thread_factory, OSTimerFactory * timer_factory,
 OSLockFactory * lock_factory )
{
	sync_sequence_id = 0;
	last_pdelay_req = NULL;

	clock->registerPort(this);
	this->clock = clock;

	port_state = PTP_INITIALIZING;

	asCapable = false;

	announce_sequence_id = 0;
	sync_sequence_id = 0;
	pdelay_sequence_id = 0;

	_new_syntonization_set_point = false;
	_ppm = 0;
	_syntonize = syntonize;
	_local_system_freq_offset_init = false;

	pdelay_started = false;

	_accelerated_sync_count = accelerated_sync_count;
	log_mean_sync_interval = -3;
	log_mean_announce_interval = 0;
	log_mean_pdelay_req_interval = 0;

	qualified_announce = NULL;

	_peer_rate_offset = 1.0;

	this->timer_factory = timer_factory;
	this->thread_factory = thread_factory;

	this->condition_factory = condition_factory;
	this->lock_factory = lock_factory;

	pdelay_count = 0;
	sync_count = 0;

	glock = lock_factory->createLock( oslock_nonrecursive );
}

bool MediaIndependentPort::init_port()
{
	if( !inferior->init_port( this )) return false;

	inferior->getLinkLayerAddress(&local_addr);
	clock->setClockIdentity(&local_addr);

	port_identity.setClockIdentity(clock->getClockIdentity());
	port_identity.setPortNumber(&port_index); ++port_index;

	port_ready_condition = condition_factory->createCondition();

	return true;
}

bool MediaIndependentPort::startPDelay() {
	bool ret;
	pdelay_started = true;
	ret = clock->addEventTimer
		( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, 32000000 );
	return ret;
}

bool MediaIndependentPort::startAnnounce() {
	XPTPD_INFOL
		(ANNOUNCE_DEBUG,"Starting announce on port %s",
		 inferior->getNetLabel()->toString() );
	return clock->addEventTimer
		( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
}

void MediaIndependentPort::setAsCapable() {
	if (asCapable == false) {
		fprintf( stderr, "AsCapable: Enabled\n" );
	}
	asCapable = true;
	clock->addEventTimer
		(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
		  (unsigned long long)
		  (pow((double)2,getAnnounceInterval())*1000000000.0)));
}

void MediaIndependentPort::stopSyncReceiptTimeout() {
	clock->deleteEventTimer( this, SYNC_RECEIPT_TIMEOUT_EXPIRES);
}

void MediaIndependentPort::startSyncReceiptTimeout() {
	clock->addEventTimer
		( this, SYNC_RECEIPT_TIMEOUT_EXPIRES,
		  (SYNC_RECEIPT_TIMEOUT_MULTIPLIER* (unsigned long long)
		   (pow((double)2,getSyncInterval())*1000000000.0)));
}


bool MediaIndependentPort::serializeState( void *buf, off_t *count ) {
	bool ret = true;
	
	if( buf == NULL ) {
		*count = sizeof(port_state)+sizeof(_peer_rate_offset)+
			sizeof(asCapable);
		return true;
	}

	if( lock() != oslock_ok ) return false;

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

	ret = inferior->serializeState( buf, count );

 bail:
	if( unlock() != oslock_ok ) return false;
	return ret;
}

bool MediaIndependentPort::restoreSerializedState( void *buf, off_t *count ) {
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

	ret = inferior->restoreSerializedState( buf, count );

	return ret;
}

bool MediaIndependentPort::processEvent(Event e)
{
	bool changed_external_master;
	
	switch (e) {
	case POWERUP:
	case INITIALIZE:
		lock();
		inferior->lock();

		XPTPD_INFO("Received POWERUP/INITIALIZE event");
		{
			unsigned long long interval3 = 0;
			unsigned long long interval4 = 0;
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
				/*
				  startSyncReceiptTimeout();
				  e4 = ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES;
				  interval4 = (unsigned long long)
				  (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
				  pow((double)2,getAnnounceInterval())*1000000000.0);
				*/
			}
      
			port_ready_condition->wait_prelock();
			inferior->openPort( port_ready_condition );
			port_ready_condition->wait();
			
			if (e3 != NULL_EVENT)
				clock->addEventTimer(this, e3, interval3);
			if (e4 != NULL_EVENT)
				clock->addEventTimer(this, e4, interval4);
		}
		
		inferior->unlock();
		unlock();

		break;
	case STATE_CHANGE_EVENT:
		clock->lock();
		lock();
		inferior->lock();
		
		{
			PTPMessageAnnounce *EBest = NULL;
			char EBestClockIdentity[PTP_CLOCK_IDENTITY_LENGTH];
			MinPortListIterator curr = clock->getPortListBegin();
			MinPortListIterator end = clock->getPortListEnd();
		
			/* Find EBest for all ports */
			for ( ;curr < end; ++curr) {
				PTPMessageAnnounce *annc;
				if
					((*curr)->port_state == PTP_DISABLED ||
					 (*curr)->port_state == PTP_FAULTY)
					{
						continue;
					}
				annc = (*curr)->calculateERBest();
				if (EBest == NULL) {
					EBest = annc;
				} else if( annc != NULL ) {
					if( annc->isBetterThan(EBest) ) {
						EBest = (*curr)->calculateERBest();
					}
				}
				XPTPD_INFOL
					(BMCA_DEBUG,"EBest=%p", EBest );
			}
			
			XPTPD_INFOL
				(BMCA_DEBUG,"EBest.grandmasterPriority1=%hhu",
				 EBest->getGrandmasterPriority1());
		
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
			
			XPTPD_INFOL
				(BMCA_DEBUG,"changed_master=%s",
				 changed_external_master ? "true" : "false" );
		
			if( clock->isBetterThan( EBest ) ){
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
			
				getClock()->setGrandmaster();
			} else {
				getClock()->clearGrandmaster();
			}

			curr = clock->getPortListBegin();
			for ( ;curr < end; ++curr) {
				if
					((*curr)->port_state == PTP_DISABLED ||
					 (*curr)->port_state == PTP_FAULTY) {
					continue;
				}
				if( clock->isBetterThan(EBest) ) {
					// We are the GrandMaster, all ports are master
					(*curr)->recommendState
						(PTP_MASTER, false);
				} else {
					XPTPD_INFOL
						(BMCA_DEBUG,"EBest=%p;ERBest=%p", EBest,
						 (*curr)->calculateERBest() );
					if( EBest == (*curr)->calculateERBest() ) {
						// The "best" Announce was recieved on this port
						ClockIdentity clock_identity;
						unsigned char priority1;
						unsigned char priority2;
						ClockQuality *clock_quality;
					
						(*curr)->recommendState
							( PTP_SLAVE, changed_external_master );
					
						XPTPD_INFOL
							(BMCA_DEBUG,"Setting port %s to slave",
							 (*curr)->inferior->getNetLabel()->toString() );
						clock_identity = EBest->getGrandmasterClockIdentity();
						getClock()->setGrandmasterClockIdentity
							(clock_identity);
						priority1 = EBest->getGrandmasterPriority1();
						getClock()->setGrandmasterPriority1( priority1 );
						priority2 = EBest->getGrandmasterPriority2();
						getClock()->setGrandmasterPriority2( priority2 );
						clock_quality = EBest->getGrandmasterClockQuality();
						getClock()->setGrandmasterClockQuality(*clock_quality);
					} else {
						/* Otherwise we are the master because we have 
						   sync'd to a better clock */
						XPTPD_INFOL
							(BMCA_DEBUG,"Setting port %s to master",
							 (*curr)->inferior->getNetLabel()->toString() );
					
						(*curr)->recommendState
							(PTP_MASTER, changed_external_master);
					}
				}
			}
		}

		inferior->unlock();
		unlock();
		clock->unlock();

		break;
	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		clock->lock();
		lock();
		inferior->lock();
		
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
				goto receipt_timeout_bail;
			}

			if (port_state == PTP_INITIALIZING
			    || port_state == PTP_UNCALIBRATED
			    || port_state == PTP_SLAVE
			    || port_state == PTP_PRE_MASTER)
				{
					fprintf
						(stderr,
						 "*** %s Timeout Expired - Becoming Master\n",
						 e == SYNC_RECEIPT_TIMEOUT_EXPIRES ?
						 "Sync" : "Announce" );
					{
						// We're Grandmaster, set grandmaster info to me
						ClockIdentity clock_identity;
						unsigned char priority1;
						unsigned char priority2;
						ClockQuality clock_quality;
					
						clock_identity = getClock()->getClockIdentity();
						getClock()->setGrandmasterClockIdentity
							( clock_identity );
						priority1 = getClock()->getPriority1();
						getClock()->setGrandmasterPriority1( priority1 );
						priority2 = getClock()->getPriority2();
						getClock()->setGrandmasterPriority2( priority2 );
						clock_quality = getClock()->getClockQuality();
						getClock()->setGrandmasterClockQuality
							( clock_quality );
					
						getClock()->setGrandmaster();
					}

					port_state = PTP_MASTER;
					Timestamp system_time;
					Timestamp device_time;
				
					inferior->getDeviceTime( system_time, device_time );
				
					/* "Expire" all previously received announce messages */
				   
					if( qualified_announce != NULL ) {
						delete qualified_announce;
						qualified_announce = NULL;
					}
				
					// Add timers for Announce and Sync,
					// this is as close to immediately as we get
					clock->addEventTimer
						( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
					startAnnounce();
				}
		}

	receipt_timeout_bail:
		inferior->unlock();
		unlock();
		clock->unlock();
				
		break;
	case PDELAY_INTERVAL_TIMEOUT_EXPIRES:
		XPTPD_INFOL(PDELAY_DEBUG,"PDELAY_INTERVAL_TIMEOUT_EXPIRES occured");
		
		lock();
		inferior->lock();

		{
			long elapsed_time;
			unsigned long long interval;

			inferior->processPDelay( getNextPDelaySequenceId(), &elapsed_time );

			XPTPD_INFO("Re-add PDelay Event Timer");
			interval = (uint64_t) (PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER*
						(pow((double)2,getPDelayInterval())*1000000000.0));
			interval -= elapsed_time*1000ULL;
			interval = interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY : interval;
			clock->addEventTimer( this, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES, interval );
			interval = (uint64_t) (pow((double)2,getPDelayInterval())*1000000000.0);
			interval -= elapsed_time*1000ULL;
			interval = interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY : interval;
			clock->addEventTimer( this, PDELAY_INTERVAL_TIMEOUT_EXPIRES, interval );
		}
		
		inferior->unlock();
		unlock();

		break;
	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
		clock->lock();
		lock();
		inferior->lock();

		{
			Timestamp system_time;
			Timestamp device_time;
			unsigned long long interval;
			long elapsed_time = 0;
			bool grandmaster = getClock()->getGrandmaster();
			XPTPD_INFOL
				( SYNC_DEBUG,
				  "SYNC_INTERVAL_TIMEOUT_EXPIRES occured(%d,%d,%d)", asCapable,
				  grandmaster, getClock()->getLastSyncValid() );
	
			if( asCapable && (grandmaster || getClock()->getLastSyncValid())) {
#ifdef WIN32
				SYSTEMTIME systime;
				GetSystemTime(&systime);
				printf("processSync:%hu:%hu:%hu.%03hu\n", systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
#endif
				inferior->processSync(getNextSyncSequenceId(), grandmaster, &elapsed_time);
			}

			/* If accelerated_sync is non-zero then start 16 ms sync
			   timer, subtract 1, for last one start PDelay also */
			if( _accelerated_sync_count > 0 ) {
				interval = EVENT_TIMER_GRANULARITY;
				--_accelerated_sync_count;
			} else {
				syncDone();
				if( _accelerated_sync_count == 0 ) {
					startAnnounce();
					--_accelerated_sync_count;
				}
				interval = (uint64_t) (pow((double)2,getSyncInterval())*1000000000.0);
				interval -= elapsed_time*1000ULL;
				interval =
					interval < EVENT_TIMER_GRANULARITY ? EVENT_TIMER_GRANULARITY : interval;
			}
			clock->addEventTimer
				( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, interval );
			
		}

		inferior->unlock();
		unlock();
		clock->unlock();

		break;
	case ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES:
		XPTPD_INFOL
			(ANNOUNCE_DEBUG,"Announce interval expired on port %s "
			 "(AsCapable=%s)", inferior->getNetLabel()->toString(),
			 asCapable ? "true" : "false" );

		clock->lock();
		lock();

		if (asCapable) {
			// Send an announce message
			PTPMessageAnnounce *annc = new PTPMessageAnnounce(this);
			PortIdentity dest_id;
			PortIdentity gmId;
			ClockIdentity clock_id = clock->getClockIdentity();
			gmId.setClockIdentity(clock_id);
			getPortIdentity(dest_id);
			annc->setPortIdentity(&dest_id);
			annc->sendPort(inferior);
			XPTPD_INFOL
				(ANNOUNCE_DEBUG,"Sent announce message on port %s",
				 inferior->getNetLabel()->toString() );
			delete annc;
		}
		clock->addEventTimer
			(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES,
			 (unsigned)
			 (pow ((double)2, getAnnounceInterval()) * 1000000000.0));

		unlock();
		clock->unlock();

		break;
	case FAULT_DETECTED:
		XPTPD_INFO("Received FAULT_DETECTED event");
		break;
	case PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES:
		lock();
		pdelay_count = 0;
		unsetAsCapable();
		unlock();
		break;
	default:
		if( !inferior->processEvent( e )) {
			XPTPD_INFO
				("Unhandled event type in "
				 "MediaIndependentPort::processEvent(), %d", e );
			return false;
		}
		break;
	}

	return true;
}

PTPMessageAnnounce *MediaIndependentPort::calculateERBest(void)
{
	return qualified_announce;
}


bool MediaIndependentPort::adjustPhaseError
( int64_t master_local_offset, FrequencyRatio master_local_freq_offset ) 
{
	long double phase_error;
	if( _new_syntonization_set_point ) {
		bool result;
		if( !inferior->suspendTransmissionAll() ) goto bail;
		result = inferior->adjustClockPhase( -master_local_offset );
		if( !inferior->resumeTransmissionAll() || !result ) goto bail;
		_new_syntonization_set_point = false;
		master_local_offset = 0;
	}

	// Adjust for frequency offset
	phase_error = (long double) -master_local_offset;
	_ppm += (float) (INTEGRAL*phase_error +
					 PROPORTIONAL*((master_local_freq_offset-1.0)*1000000));
	if( _ppm < LOWER_FREQ_LIMIT ) _ppm = LOWER_FREQ_LIMIT;
	if( _ppm > UPPER_FREQ_LIMIT ) _ppm = UPPER_FREQ_LIMIT;
	if( !inferior->adjustClockRate( _ppm )) {
		XPTPD_ERROR( "Failed to adjust clock rate" );
		return false;
	}

	return true;

 bail:
	return false;
}

void MediaIndependentPort::becomeMaster( bool annc ) {
	port_state = PTP_MASTER;
	// Start announce receipt timeout timer
	// Start sync receipt timeout timer
	clock->deleteEventTimer( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES );
	stopSyncReceiptTimeout();
	if( annc ) {
		startAnnounce();
	}
	clock->addEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES, 16000000 );
	printf("Switching to Master\n" );

	return;
}

void MediaIndependentPort::becomeSlave( bool restart_syntonization ) {
	port_state = PTP_SLAVE;
	clock->deleteEventTimer( this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES );
	clock->deleteEventTimer( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );
	//startSyncReceiptTimeout();
	clock->addEventTimer
		(this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER*
		  (unsigned long long)
		  (pow((double)2,getAnnounceInterval())*1000000000.0)));
	printf("Switching to Slave\n" );
	if( restart_syntonization ) newSyntonizationSetPoint();
  
	return;
}

void MediaIndependentPort::recommendState
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
				newSyntonizationSetPoint();
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

bool MediaIndependentPort::stop() {
	bool ret;
	glock->lock();
	ret = clock->stop_port(this);
	glock->unlock();
	return ret;
}


