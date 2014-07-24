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

#include <avbts_clock.hpp>
#include <avbts_oslock.hpp>
#include <avbts_ostimerq.hpp>
#include <avbts_message.hpp>
#include <avbts_osipc.hpp>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <string.h>

void ClockIdentity::set(LinkLayerAddress * addr)
{
	uint64_t tmp1 = 0;
	uint32_t tmp2;
	addr->toOctetArray((uint8_t *) & tmp1);
	tmp2 = tmp1 & 0xFFFFFF;
	tmp1 >>= 24;
	tmp1 <<= 16;
	tmp1 |= 0xFEFF;
	tmp1 <<= 24;
	tmp1 |= tmp2;
	memcpy(id, &tmp1, PTP_CLOCK_IDENTITY_LENGTH);
}

IEEE1588Clock::IEEE1588Clock
( bool forceOrdinarySlave, bool syntonize, uint8_t priority1,
  Timestamper *timestamper, OSTimerQueueFactory *timerq_factory,
  OSLockFactory *lock_factory, OS_IPC *ipc )
{
	this->priority1 = priority1;
	priority2 = 248;

	clock_quality.clockAccuracy = 0xfe;
	clock_quality.cq_class = 248;
	clock_quality.offsetScaledLogVariance = 16640;

	time_source = 160;

	domain_number = 0;

	this->ipc = ipc;

	glock = lock_factory->createLock( oslock_nonrecursive );

 	memset( &LastEBestIdentity, 0xFF, sizeof( LastEBestIdentity ));

	_last_sync_valid = false;

	// This should be done LAST!! to pass fully initialized clock object
	timerq = timerq_factory->createOSTimerQueue( lock_factory );

	return;
}

bool IEEE1588Clock::serializeState( void *buf, off_t *count ) {
	bool ret = true;
  
	if( lock() != oslock_ok ) {
		ret = false;
		goto done;
	}

	if( buf == NULL ) {
		*count = sizeof( _master_system_freq_offset ) +
			sizeof( LastEBestIdentity );
		return true;
	}
  
	// Master-System Frequency Offset
	if( ret && *count >= (off_t) sizeof( _master_system_freq_offset )) {
		memcpy
			( buf, &_master_system_freq_offset,
			  sizeof( _master_system_freq_offset ));
		*count -= sizeof( _master_system_freq_offset );
		buf = ((char *)buf) + sizeof( _master_system_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _master_system_freq_offset );
	} else {
		*count = sizeof( _master_system_freq_offset )-*count;
		ret = false;
	}
  
	// LastEBestIdentity
	if( ret && *count >= (off_t) sizeof( LastEBestIdentity )) {
		memcpy( buf, &LastEBestIdentity, (off_t) sizeof( LastEBestIdentity ));
		*count -= sizeof( LastEBestIdentity );
		buf = ((char *)buf) + sizeof( LastEBestIdentity );
	} else if( ret == false ) {
		*count += sizeof( LastEBestIdentity );
	} else {
		*count = sizeof( LastEBestIdentity )-*count;
		ret = false;
	}

	if( unlock() != oslock_ok ) {
		ret = false;
		goto done;
	}

done:
	return ret;
}

bool IEEE1588Clock::restoreSerializedState( void *buf, off_t *count ) {
	bool ret = true;
  
	/* Master-Local Frequency Offset */
	if( ret && *count >= (off_t) sizeof( _master_system_freq_offset )) {
		memcpy
			( &_master_system_freq_offset, buf, 
			  sizeof( _master_system_freq_offset ));
		*count -= sizeof( _master_system_freq_offset );
		buf = ((char *)buf) + sizeof( _master_system_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _master_system_freq_offset );
	} else {
		*count = sizeof( _master_system_freq_offset )-*count;
		ret = false;
	}
	
	/* LastEBestIdentity */
	if( ret && *count >= (off_t) sizeof( LastEBestIdentity )) {
		memcpy( &LastEBestIdentity, buf, sizeof( LastEBestIdentity ));
		*count -= sizeof( LastEBestIdentity );
		buf = ((char *)buf) + sizeof( LastEBestIdentity );
	} else if( ret == false ) {
		*count += sizeof( LastEBestIdentity );
	} else {
		*count = sizeof( LastEBestIdentity )-*count;
		ret = false;
	}

	return ret;
}

void timerq_handler(void *arg)
{

	event_descriptor_t *event_descriptor = (event_descriptor_t *) arg;
	event_descriptor->port->processEvent(event_descriptor->event);
}

bool IEEE1588Clock::addEventTimer
( MediaIndependentPort * target, Event e, unsigned long long time_ns)
{
	bool ret;
	event_descriptor_t *event_descriptor = new event_descriptor_t();
	event_descriptor->event = e;
	event_descriptor->port = target;
	event_descriptor->timerq = timerq;
	ret = timerq->addEvent
		((unsigned)(time_ns / 1000), (int)e, timerq_handler, event_descriptor,
		 true, NULL);
	return ret;
}

bool IEEE1588Clock::deleteEventTimer(MediaIndependentPort *target, Event event)
{
	bool ret = timerq->cancelEvent( (int)event, target );
	return ret;
}

void IEEE1588Clock::setMasterOffset( clock_offset_t *offset ) 
{
	_master_system_freq_offset = offset->ml_freqoffset*offset->ls_freqoffset;

	if( ipc != NULL ) ipc->update( offset );

	return;
}

/* Get current time from system clock */
Timestamp IEEE1588Clock::getTime(void) const
{
	return getSystemTime();
}

/* Get timestamp from hardware */
Timestamp IEEE1588Clock::getPreciseTime(void) const
{
	return getSystemTime();
}

Timestamp IEEE1588Clock::getSystemTime(void)
{
	return (Timestamp(0, 0, 0));
}



bool IEEE1588Clock::isBetterThan( PTPMessageAnnounce * msg ) const
{
	unsigned char this1[14];
	unsigned char that1[14];
	uint16_t tmp;

	this1[0] = priority1;
	that1[0] = msg->getGrandmasterPriority1();

	this1[1] = clock_quality.cq_class;
	that1[1] = msg->getGrandmasterClockQuality()->cq_class;

	this1[2] = clock_quality.clockAccuracy;
	that1[2] = msg->getGrandmasterClockQuality()->clockAccuracy;

	tmp = clock_quality.offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(this1 + 3, &tmp, sizeof(tmp));
	tmp = msg->getGrandmasterClockQuality()->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(that1 + 3, &tmp, sizeof(tmp));
	
	this1[5] = priority2;
	that1[5] = msg->getGrandmasterPriority2();
	
	clock_identity.getIdentityString(this1 + 6);
	msg->getGrandmasterIdentity((char *)that1 + 6);

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}

IEEE1588Clock::~IEEE1588Clock(void)
{
	// Do nothing
}
