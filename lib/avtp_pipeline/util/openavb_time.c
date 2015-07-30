/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : Implementation of time support functions.
*/

#include "openavb_time.h"

OPENAVB_CODE_MODULE_PRI

U64 openavbTimeTimespecToNSec(struct timespec *pTime)
{
	if (!pTime) {
		return 0;			// Error case
	}

	return ((U64)pTime->tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)pTime->tv_nsec;
}

void openavbTimeTimespecFromNSec(struct timespec *pTime, U64 nSec)
{
	if (!pTime) {
		return;		// Error case
	}

	pTime->tv_sec = nSec / NANOSECONDS_PER_SECOND;
	pTime->tv_nsec = nSec % NANOSECONDS_PER_SECOND;
}

void openavbTimeTimespecAddUsec(struct timespec *pTime, U32 us)
{
	if (!pTime) {
		return;		// Error case - undefined behavior
	}

	pTime->tv_nsec += us * NANOSECONDS_PER_USEC;
    pTime->tv_sec += pTime->tv_nsec / NANOSECONDS_PER_SECOND;
    pTime->tv_nsec = pTime->tv_nsec % NANOSECONDS_PER_SECOND;
}

void openavbTimeTimespecSubUsec(struct timespec *pTime, U32 us)
{
	if (!pTime) {
		return;		// Error case - undefined behavior
	}

	U64 nSec = ((U64)pTime->tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)pTime->tv_nsec;
	nSec -= us * NANOSECONDS_PER_USEC;
	pTime->tv_sec = nSec / NANOSECONDS_PER_SECOND;
	pTime->tv_nsec = nSec % NANOSECONDS_PER_SECOND;
}

S64 openavbTimeTimespecUsecDiff(struct timespec *pTime1, struct timespec *pTime2)
{
	if (!pTime1 || !pTime2) {
		return 0;	// Error case - undefined behavior
	}

	U64 timeUSec1 = (pTime1->tv_sec * MICROSECONDS_PER_SECOND) + (pTime1->tv_nsec / NANOSECONDS_PER_USEC);
	U64 timeUSec2 = (pTime2->tv_sec * MICROSECONDS_PER_SECOND) + (pTime2->tv_nsec / NANOSECONDS_PER_USEC);
	return timeUSec2 - timeUSec1;
}

S32 openavbTimeTimespecCmp(struct timespec *pTime1, struct timespec *pTime2)
{
	if (!pTime1 || !pTime2) {
		return -1;	// Error case - undefined behavior
	}

	if (pTime1->tv_sec < pTime2->tv_sec) {
		return -1;
	}
	if (pTime1->tv_sec > pTime2->tv_sec) {
		return 1;
	}
	if (pTime1->tv_sec == pTime2->tv_sec) {
		if (pTime1->tv_nsec < pTime2->tv_nsec) {
			return -1;
		}
		if (pTime1->tv_nsec > pTime2->tv_nsec) {
			return 1;
		}
	}
	return 0;	// Equal
}

U64 openavbTimeUntilUSec(struct timespec *pTime1, struct timespec *pTime2)
{
	if (!pTime1 || !pTime2) {
		return 0;	// Error case - undefined behavior
	}

	U64 timeUSec1 = (pTime1->tv_sec * MICROSECONDS_PER_SECOND) + (pTime1->tv_nsec / NANOSECONDS_PER_USEC);
	U64 timeUSec2 = (pTime2->tv_sec * MICROSECONDS_PER_SECOND) + (pTime2->tv_nsec / NANOSECONDS_PER_USEC);

	if (timeUSec2 > timeUSec1) {
		return timeUSec2 - timeUSec1;
	}
	return 0;
}

U32 openavbTimeUntilMSec(struct timespec *pTime1, struct timespec *pTime2)
{
	return openavbTimeUntilUSec(pTime1, pTime2) / 1000;
}

