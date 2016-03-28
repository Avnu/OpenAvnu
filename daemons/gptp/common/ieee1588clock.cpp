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

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <string.h>

std::string ClockIdentity::getIdentityString()
{
	uint8_t cid[PTP_CLOCK_IDENTITY_LENGTH];
	getIdentityString(cid);
	char scid[PTP_CLOCK_IDENTITY_LENGTH * 3 + 1];
	char* pscid = scid;
	for (unsigned i = 0; i < PTP_CLOCK_IDENTITY_LENGTH; ++i) {
		unsigned byte = cid[i];
		PLAT_snprintf(pscid, 4, "%2.2X:", byte);
		pscid += 3;
	}
	scid[PTP_CLOCK_IDENTITY_LENGTH * 3 - 1] = '\0';

	return std::string(scid);
}

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
  HWTimestamper *timestamper, OSTimerQueueFactory *timerq_factory,
  OS_IPC *ipc, OSLockFactory *lock_factory )
{
	this->priority1 = priority1;
	priority2 = 248;

	number_ports = 0;

	this->forceOrdinarySlave = forceOrdinarySlave;

    /*TODO: Make the values below configurable*/
	clock_quality.clockAccuracy = 0x22;
	clock_quality.cq_class = 248;
	clock_quality.offsetScaledLogVariance = 0x436A;

	time_source = 160;

	domain_number = 0;

	_syntonize = syntonize;
	_new_syntonization_set_point = false;
	_ppm = 0;

	_master_local_freq_offset_init = false;
	_local_system_freq_offset_init = false;
	_timestamper = timestamper;

	this->ipc = ipc;

 	memset( &LastEBestIdentity, 0xFF, sizeof( LastEBestIdentity ));

	timerq_lock = lock_factory->createLock( oslock_nonrecursive );

	// This should be done LAST!! to pass fully initialized clock object
	timerq = timerq_factory->createOSTimerQueue( this );

    fup_info = new FollowUpTLV();
    fup_status = new FollowUpTLV();

	return;
}

bool IEEE1588Clock::serializeState( void *buf, off_t *count ) {
  bool ret = true;

  if( buf == NULL ) {
    *count = sizeof( _master_local_freq_offset ) + sizeof( _local_system_freq_offset ) + sizeof( LastEBestIdentity );
    return true;
  }

  // Master-Local Frequency Offset
  if( ret && *count >= (off_t) sizeof( _master_local_freq_offset )) {
	  memcpy
		  ( buf, &_master_local_freq_offset,
			sizeof( _master_local_freq_offset ));
	  *count -= sizeof( _master_local_freq_offset );
	  buf = ((char *)buf) + sizeof( _master_local_freq_offset );
  } else if( ret == false ) {
	  *count += sizeof( _master_local_freq_offset );
  } else {
	  *count = sizeof( _master_local_freq_offset )-*count;
	  ret = false;
  }

  // Local-System Frequency Offset
  if( ret && *count >= (off_t) sizeof( _local_system_freq_offset )) {
	  memcpy
		  ( buf, &_local_system_freq_offset,(off_t)
			sizeof( _local_system_freq_offset ));
	  *count -= sizeof( _local_system_freq_offset );
	  buf = ((char *)buf) + sizeof( _local_system_freq_offset );
  } else if( ret == false ) {
	  *count += sizeof( _local_system_freq_offset );
  } else {
	  *count = sizeof( _local_system_freq_offset )-*count;
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

  return ret;
}

bool IEEE1588Clock::restoreSerializedState( void *buf, off_t *count ) {
	bool ret = true;

	/* Master-Local Frequency Offset */
	if( ret && *count >= (off_t) sizeof( _master_local_freq_offset )) {
		memcpy
			( &_master_local_freq_offset, buf,
			  sizeof( _master_local_freq_offset ));
		*count -= sizeof( _master_local_freq_offset );
		buf = ((char *)buf) + sizeof( _master_local_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _master_local_freq_offset );
	} else {
		*count = sizeof( _master_local_freq_offset )-*count;
		ret = false;
	}

	/* Local-System Frequency Offset */
	if( ret && *count >= (off_t) sizeof( _local_system_freq_offset )) {
		memcpy
			( &_local_system_freq_offset, buf,
			  sizeof( _local_system_freq_offset ));
		*count -= sizeof( _local_system_freq_offset );
		buf = ((char *)buf) + sizeof( _local_system_freq_offset );
	} else if( ret == false ) {
		*count += sizeof( _local_system_freq_offset );
	} else {
		*count = sizeof( _local_system_freq_offset )-*count;
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

void *IEEE1588Port::watchNetLink(void)
{
	// Should never return
	net_iface->watchNetLink(this);

	return NULL;
}

Timestamp IEEE1588Clock::getSystemTime(void)
{
	return (Timestamp(0, 0, 0));
}

void timerq_handler(void *arg)
{

	event_descriptor_t *event_descriptor = (event_descriptor_t *) arg;
	event_descriptor->port->processEvent(event_descriptor->event);
}

void IEEE1588Clock::addEventTimer
( IEEE1588Port * target, Event e, unsigned long long time_ns )
{
	event_descriptor_t *event_descriptor = new event_descriptor_t();
	event_descriptor->event = e;
	event_descriptor->port = target;
	timerq->addEvent
		((unsigned)(time_ns / 1000), (int)e, timerq_handler, event_descriptor,
		 true, NULL);
}

void IEEE1588Clock::addEventTimerLocked
( IEEE1588Port * target, Event e, unsigned long long time_ns )
{
    if( getTimerQLock() == oslock_fail ) return;
	addEventTimer( target, e, time_ns );
    if( putTimerQLock() == oslock_fail ) return;
}



void IEEE1588Clock::deleteEventTimer(IEEE1588Port * target, Event event)
{
	timerq->cancelEvent((int)event, NULL);
}

void IEEE1588Clock::deleteEventTimerLocked(IEEE1588Port * target, Event event)
{
    if( getTimerQLock() == oslock_fail ) return;

	timerq->cancelEvent((int)event, NULL);

    if( putTimerQLock() == oslock_fail ) return;
}

FrequencyRatio IEEE1588Clock::calcLocalSystemClockRateDifference( Timestamp local_time, Timestamp system_time ) {
	unsigned long long inter_system_time;
	unsigned long long inter_local_time;
	FrequencyRatio ppt_offset;

	XPTPD_INFO( "Calculated local to system clock rate difference" );

	if( !_local_system_freq_offset_init ) {
		_prev_system_time = system_time;
		_prev_local_time = local_time;

		_local_system_freq_offset_init = true;

		return 1.0;
	}

	inter_system_time =
		TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(_prev_system_time);
	inter_local_time  =
		TIMESTAMP_TO_NS(local_time) -  TIMESTAMP_TO_NS(_prev_local_time);

	if( inter_system_time != 0 ) {
		ppt_offset = ((FrequencyRatio)inter_local_time)/inter_system_time;
	} else {
		ppt_offset = 1.0;
	}

	_prev_system_time = system_time;
	_prev_local_time = local_time;

  return ppt_offset;
}



FrequencyRatio IEEE1588Clock::calcMasterLocalClockRateDifference( Timestamp master_time, Timestamp sync_time ) {
	unsigned long long inter_sync_time;
	unsigned long long inter_master_time;
	FrequencyRatio ppt_offset;

	XPTPD_INFO( "Calculated master to local clock rate difference" );

	if( !_master_local_freq_offset_init ) {
		_prev_sync_time = sync_time;
		_prev_master_time = master_time;

		_master_local_freq_offset_init = true;

		return 1.0;
	}

	inter_sync_time =
		TIMESTAMP_TO_NS(sync_time) - TIMESTAMP_TO_NS(_prev_sync_time);
	inter_master_time =
		TIMESTAMP_TO_NS(master_time) -  TIMESTAMP_TO_NS(_prev_master_time);

	if( inter_sync_time != 0 ) {
		ppt_offset = ((FrequencyRatio)inter_master_time)/inter_sync_time;
	} else {
		ppt_offset = 1.0;
	}

	_prev_sync_time = sync_time;
	_prev_master_time = master_time;

	return ppt_offset;
}

void IEEE1588Clock::setMasterOffset
( int64_t master_local_offset, Timestamp local_time,
  FrequencyRatio master_local_freq_offset, int64_t local_system_offset,
  Timestamp system_time, FrequencyRatio local_system_freq_offset,
  unsigned sync_count, unsigned pdelay_count, PortState port_state, bool asCapable )
{
	_master_local_freq_offset = master_local_freq_offset;
	_local_system_freq_offset = local_system_freq_offset;

	if( ipc != NULL ) ipc->update
		( master_local_offset, local_system_offset, master_local_freq_offset,
		  local_system_freq_offset, TIMESTAMP_TO_NS(local_time), sync_count,
		  pdelay_count, port_state, asCapable );

	if( master_local_offset == 0 && master_local_freq_offset == 1.0 ) {
		return;
	}

	if( _syntonize ) {
		if( _new_syntonization_set_point ) {
			_new_syntonization_set_point = false;
			if( _timestamper ) {
				/* Make sure that there are no transmit operations
				   in progress */
				getTxLockAll();
				_timestamper->HWTimestamper_adjclockphase
					( -master_local_offset );
				_master_local_freq_offset_init = false;
				putTxLockAll();
				master_local_offset = 0;
			}
		}
		// Adjust for frequency offset
		long double phase_error = (long double) -master_local_offset;
		_ppm += (float) (INTEGRAL*phase_error +
			 PROPORTIONAL*((master_local_freq_offset-1.0)*1000000));
		if( _ppm < LOWER_FREQ_LIMIT ) _ppm = LOWER_FREQ_LIMIT;
		if( _ppm > UPPER_FREQ_LIMIT ) _ppm = UPPER_FREQ_LIMIT;
		if( _timestamper ) {
			if( !_timestamper->HWTimestamper_adjclockrate( _ppm )) {
				XPTPD_ERROR( "Failed to adjust clock rate" );
			}
		}
	}

	return;
}

/* Get current time from system clock */
Timestamp IEEE1588Clock::getTime(void)
{
	return getSystemTime();
}

/* Get timestamp from hardware */
Timestamp IEEE1588Clock::getPreciseTime(void)
{
	return getSystemTime();
}

bool IEEE1588Clock::isBetterThan(PTPMessageAnnounce * msg)
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

#if 0
	XPTPD_PRINTF("(Clk)Us: ");
	for (int i = 0; i < 14; ++i)
		XPTPD_PRINTF("%hhx ", this1[i]);
	XPTPD_PRINTF("\n");
	XPTPD_PRINTF("(Clk)Them: ");
	for (int i = 0; i < 14; ++i)
		XPTPD_PRINTF("%hhx ", that1[i]);
	XPTPD_PRINTF("\n");
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}

IEEE1588Clock::~IEEE1588Clock(void)
{
	// Do nothing
}
