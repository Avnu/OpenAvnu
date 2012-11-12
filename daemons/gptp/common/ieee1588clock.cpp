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

IEEE1588Clock::IEEE1588Clock(bool forceOrdinarySlave,
			     OSTimerQueueFactory * timerq_factory, OS_IPC * ipc)
{
	timerq = timerq_factory->createOSTimerQueue();

	priority1 = 248;
	priority2 = 248;

	master_local_offset_nrst125us_initialized = false;

	number_ports = 0;

	this->forceOrdinarySlave = forceOrdinarySlave;

	clock_quality.clockAccuracy = 0xfe;
	clock_quality.cq_class = 248;
	clock_quality.offsetScaledLogVariance = 16640;

	time_source = 160;

	domain_number = 0;

	this->ipc = ipc;

 	memset( &LastEBestIdentity, 0xFF, sizeof( LastEBestIdentity ));
	return;
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

void IEEE1588Clock::addEventTimer(IEEE1588Port * target, Event e,
				  unsigned long long time_ns)
{
	event_descriptor_t *event_descriptor = new event_descriptor_t();
	event_descriptor->event = e;
	event_descriptor->port = target;
	timerq->addEvent((unsigned)time_ns / 1000, (int)e, timerq_handler,
			 event_descriptor, true, NULL);
}

void IEEE1588Clock::deleteEventTimer(IEEE1588Port * target, Event event)
{
	timerq->cancelEvent((int)event, NULL);
}

// Sync clock to argument time
void IEEE1588Clock::setMasterOffset(int64_t master_local_offset,
				    Timestamp local_time,
				    int32_t master_local_freq_offset,
				    int64_t local_system_offset,
				    Timestamp system_time,
				    int32_t local_system_freq_offset,
				    uint32_t nominal_clock_rate,
				    uint32_t local_clock)
{

	if (ipc != NULL)
		ipc->update(master_local_offset, local_system_offset,
			    master_local_freq_offset, local_system_freq_offset,
			    TIMESTAMP_TO_NS(local_time));
	return;
}

void IEEE1588Clock::getGrandmasterIdentity(char *id)
{
	grandmaster_port_identity.getClockIdentityString(id);
}

// Get current time from system clock
Timestamp IEEE1588Clock::getTime(void)
{
	return getSystemTime();
}

// Get timestamp from hardware
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

	clock_identity.getIdentityString((char *)this1 + 6);
	//memcpy( this1+6, clock_identity, PTP_CLOCK_IDENTITY_LENGTH );
	msg->getGrandmasterIdentity((char *)that1 + 6);

#if 0
	fprintf(stderr, "(Clk)Us: ");
	for (int i = 0; i < 14; ++i)
		fprintf(stderr, "%hhx ", this1[i]);
	fprintf(stderr, "\n");
	fprintf(stderr, "(Clk)Them: ");
	for (int i = 0; i < 14; ++i)
		fprintf(stderr, "%hhx ", that1[i]);
	fprintf(stderr, "\n");
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}

IEEE1588Clock::~IEEE1588Clock(void)
{
	// Unmap shared memory
}
