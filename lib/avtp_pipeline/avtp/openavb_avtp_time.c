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
* MODULE SUMMARY : Avtp Time implementation
*/

#include "openavb_platform.h"
#include <stdlib.h>

#include "openavb_types.h"
#include "openavb_trace.h"
#include "openavb_avtp_time_pub.h"

#define	AVB_LOG_COMPONENT	"AVTP"
#include "openavb_log.h"

#define OPENAVB_AVTP_TIME_MAX_TS_DIFF (0x7FFFFFFF)

#include "openavb_avtp_time_osal.c"

static U64 x_getNsTime(timespec_t *tmTime)
{
	return ((U64)tmTime->tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)tmTime->tv_nsec;
}

static U32 x_getTimestamp(U64 timeNsec)
{
	return (U32)(timeNsec & 0x00000000FFFFFFFFL);
}

avtp_time_t* openavbAvtpTimeCreate(U32 maxLatencyUsec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_TIME);

	avtp_time_t *pAvtpTime = calloc(1, sizeof(avtp_time_t));

	if (pAvtpTime) {
		pAvtpTime->maxLatencyNsec = maxLatencyUsec * NANOSECONDS_PER_USEC;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVTP_TIME);
	return pAvtpTime;
}

void openavbAvtpTimeDelete(avtp_time_t *pAvtpTime)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVTP_TIME);

	if (pAvtpTime) {
		free(pAvtpTime);
		pAvtpTime = NULL;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVTP_TIME);
}

void openavbAvtpTimeAddUSec(avtp_time_t *pAvtpTime, long uSec)
{
	if (pAvtpTime) {
		pAvtpTime->timeNsec += uSec * NANOSECONDS_PER_USEC;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeAddNSec(avtp_time_t *pAvtpTime, long nSec)
{
	if (pAvtpTime) {
		pAvtpTime->timeNsec += nSec;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeSubUSec(avtp_time_t *pAvtpTime, long uSec)
{
	if (pAvtpTime) {
		pAvtpTime->timeNsec -= uSec * NANOSECONDS_PER_USEC;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeSubNSec(avtp_time_t *pAvtpTime, long nSec)
{
	if (pAvtpTime) {
		pAvtpTime->timeNsec -= nSec;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}


void openavbAvtpTimeSetToWallTime(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		timespec_t tmNow;
		if (CLOCK_GETTIME(OPENAVB_CLOCK_WALLTIME, &tmNow)) {
			pAvtpTime->timeNsec = x_getNsTime(&tmNow);
			pAvtpTime->bTimestampValid = TRUE;
			pAvtpTime->bTimestampUncertain = FALSE;
		}
		else {
			pAvtpTime->bTimestampValid = FALSE;
			pAvtpTime->bTimestampUncertain = FALSE;
		}
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeSetToSystemTime(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		timespec_t tmNow = {0, 0};
		CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &tmNow);
		pAvtpTime->timeNsec = x_getNsTime(&tmNow);
		pAvtpTime->bTimestampValid = tmNow.tv_sec || tmNow.tv_nsec;
		pAvtpTime->bTimestampUncertain = FALSE;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

// This function will take a AVTP timestamp and set the local AvtpTime values for it.
// Commonly this is used in the RX mapping callbacks, to take the AVTP timestamp received
// on the listener and place it into the AvtpTime so that the media queue and determine
// the correct time to release the media queue item for presentation.
void openavbAvtpTimeSetToTimestamp(avtp_time_t *pAvtpTime, U32 timestamp)
{
	if (pAvtpTime) {
		timespec_t tmNow;
		if (CLOCK_GETTIME(OPENAVB_CLOCK_WALLTIME, &tmNow)) {
			U64 nsNow = x_getNsTime(&tmNow);
			U32 tsNow = x_getTimestamp(nsNow);

			U32 delta;
			if (tsNow < timestamp) {
				delta = timestamp - tsNow;
			}  
			else if (tsNow > timestamp) {
				delta = timestamp + (0x100000000ULL - tsNow);
			}
			else {
				delta = 0;
			}

			if (delta < OPENAVB_AVTP_TIME_MAX_TS_DIFF) {
			  	// Normal case, timestamp is upcoming
				pAvtpTime->timeNsec = nsNow + delta;
			}
			else {
			  	// Timestamp is past
				pAvtpTime->timeNsec = nsNow - (0x100000000ULL - delta);
			}

			pAvtpTime->bTimestampValid = TRUE;
			pAvtpTime->bTimestampUncertain = FALSE;
		}
		else {
			pAvtpTime->bTimestampValid = FALSE;
			pAvtpTime->bTimestampUncertain = TRUE;
		}
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeSetToTimestampNS(avtp_time_t *pAvtpTime, U64 timeNS)
{
	if (pAvtpTime) {
		pAvtpTime->timeNsec = timeNS;
		pAvtpTime->bTimestampValid = TRUE;
		pAvtpTime->bTimestampUncertain = FALSE;
	}  
}

void openavbAvtpTimeSetToTimespec(avtp_time_t *pAvtpTime, timespec_t* timestamp)
{
	if (pAvtpTime)
	{
		if ((timestamp->tv_sec == 0) && (timestamp->tv_nsec == 0))
		{
			pAvtpTime->bTimestampValid = FALSE;
			pAvtpTime->bTimestampUncertain = TRUE;
		}
		else
		{
			U64 nsec = x_getNsTime(timestamp);
			pAvtpTime->timeNsec = nsec;
			pAvtpTime->bTimestampValid = TRUE;
			pAvtpTime->bTimestampUncertain = FALSE;
		}
	}
	else
	{
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimePushMCR(avtp_time_t *pAvtpTime, U32 timestamp)
{
	if (pAvtpTime)
	{
		if ( HAL_PUSH_MCR(&timestamp) == FALSE) {
			AVB_LOG_ERROR("Pushing MCR timestamp");
		}
	}
}


void openavbAvtpTimeSetTimestampValid(avtp_time_t *pAvtpTime, bool validFlag)
{
	if (pAvtpTime) {
		pAvtpTime->bTimestampValid = validFlag;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

void openavbAvtpTimeSetTimestampUncertain(avtp_time_t *pAvtpTime, bool uncertainFlag)
{
	if (pAvtpTime) {
		pAvtpTime->bTimestampUncertain = uncertainFlag;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
}

U32 openavbAvtpTimeGetAvtpTimestamp(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		return x_getTimestamp(pAvtpTime->timeNsec);
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return 0;
	}
}

U64 openavbAvtpTimeGetAvtpTimeNS(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		return pAvtpTime->timeNsec;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return 0;
	}
}

bool openavbAvtpTimeTimestampIsValid(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		return pAvtpTime->bTimestampValid;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return FALSE;
	}
}

bool openavbAvtpTimeTimestampIsUncertain(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		return pAvtpTime->bTimestampUncertain;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return TRUE;
	}
}

// Check if the AvtpTime is past now. If something goes wrong return true to allow 
//  data to continue to flow. 
bool openavbAvtpTimeIsPast(avtp_time_t *pAvtpTime)
{
	if (pAvtpTime) {
		timespec_t tmNow;

		if (!pAvtpTime->bTimestampValid || pAvtpTime->bTimestampUncertain) {
			return TRUE;    // If timestamp can't be trusted assume time is past.
		}

		if (CLOCK_GETTIME(OPENAVB_CLOCK_WALLTIME, &tmNow)) {
			U64 nsNow = x_getNsTime(&tmNow);

			if (nsNow >= pAvtpTime->timeNsec) {
				return TRUE;    // Normal timestamp time reached.
			}

			if (nsNow + pAvtpTime->maxLatencyNsec < pAvtpTime->timeNsec) {
				IF_LOG_INTERVAL(100) {
					AVB_LOGF_INFO("Timestamp out of range: Now:%" PRIu64 " TSTime%" PRIu64 " MaxLatency:%" PRIu64 "ns Delta:%" PRIu64 "ns", nsNow, pAvtpTime->timeNsec, pAvtpTime->maxLatencyNsec, pAvtpTime->timeNsec - nsNow);
				}
				return TRUE;
			}

			return FALSE;
		}
		else {
			return TRUE;    // If timestamp can't be retrieved assume time is past to keep data flowing. 
		}
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return TRUE;
	}
}

bool openavbAvtpTimeIsPastTime(avtp_time_t *pAvtpTime, U64 nSecTime)
{
	if (pAvtpTime) {

		if (!pAvtpTime->bTimestampValid || pAvtpTime->bTimestampUncertain) {
			return TRUE;    // If timestamp can't be trusted assume time is past.
		}

		if (nSecTime >= pAvtpTime->timeNsec) {
			return TRUE;    // Normal timestamp time reached.
		}

		if (nSecTime + pAvtpTime->maxLatencyNsec < pAvtpTime->timeNsec) {
			IF_LOG_INTERVAL(100) {
				AVB_LOGF_INFO("Timestamp out of range: Now:%" PRIu64 "TSTime%" PRIu64 " MaxLatency:%" PRIu64 " ns Delta:%" PRIu64 "ns", nSecTime, pAvtpTime->timeNsec, pAvtpTime->maxLatencyNsec, pAvtpTime->timeNsec - nSecTime);
			}
			return TRUE;
		}

		return FALSE;
}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return TRUE;
	}
}



bool openavbAvtpTimeUsecTill(avtp_time_t *pAvtpTime, U32 *pUsecTill)
{
	if (pAvtpTime) {
		if (pAvtpTime->bTimestampValid && !pAvtpTime->bTimestampUncertain) {

			timespec_t tmNow;

			if (CLOCK_GETTIME(OPENAVB_CLOCK_WALLTIME, &tmNow)) {
				U64 nsNow = x_getNsTime(&tmNow);

				if (pAvtpTime->timeNsec >= nsNow) {
					U32 usecTill = (pAvtpTime->timeNsec - nsNow) / NANOSECONDS_PER_USEC;

					if (usecTill <= MICROSECONDS_PER_SECOND * 5) {
						*pUsecTill = usecTill;
						return TRUE;
					}
					else {
						return FALSE;
					}
				}
				else {
					*pUsecTill = 0;
					return TRUE;
				}
			}
		}

		// When the timestamp is invalid or uncertain assume timestamp is now.
		*pUsecTill = 0;
		return TRUE;
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
		return FALSE;
	}
}

S32 openavbAvtpTimeUsecDelta(avtp_time_t *pAvtpTime)
{
	S32 delta = 0;
	if (pAvtpTime) {
		if (pAvtpTime->bTimestampValid && !pAvtpTime->bTimestampUncertain) {
			timespec_t tmNow;

			if (CLOCK_GETTIME(OPENAVB_CLOCK_WALLTIME, &tmNow)) {
				U64 nsNow = x_getNsTime(&tmNow);

				delta = (S64)(pAvtpTime->timeNsec - nsNow) / NANOSECONDS_PER_USEC;
			}
		}
	}
	else {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVTP_TIME_FAILURE | OPENAVBAVTPTIME_RC_INVALID_PTP_TIME));
	}
	return delta;
}


