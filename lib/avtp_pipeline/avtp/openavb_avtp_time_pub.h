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
* HEADER SUMMARY : AVTP Time public interface
*/

#ifndef OPENAVB_AVTP_TIME_PUB_H
#define OPENAVB_AVTP_TIME_PUB_H 1

#include "openavb_platform_pub.h"
#include "openavb_types_pub.h"

/** \file
 * AVTP Time public interface.
 */

/// standard timespec type.
typedef struct timespec timespec_t;

/** AVTP time structure.
 */
typedef struct {
	/// Time in nanoseconds.
	U64 timeNsec;

	/// Maximum latency. Timestamps greater than now + max latency will be considered uncertain.
	U64 maxLatencyNsec;

	/// Timestamp valid.
	bool bTimestampValid;

	/// Timestamp uncertain.
	bool bTimestampUncertain;
} avtp_time_t;


/** Create a avtp_time_t structure.
 *
 * Allocate storage for a avtp_time_t structure. When a media queue items are
 * created an avtp_time_t structure is allocated for each item. Interface
 * modules do not need to be concerned with doing this.
 *
 * \param maxLatencyUsec Maximum Latency (in usec) for the avtp_time_t
 *        structure. Timestamps greater than now + maximum latency are
 *        considered uncertain.
 * \return A pointer to the avtp_time_t structure. Returns NULL if the memory
 *         could not be allocated.
 */
avtp_time_t * openavbAvtpTimeCreate(U32 maxLatencyUsec);

/** Delete the time struct.
 *
 * Delete the avtp_time_t structure and any additional allocations it owns.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 */
void openavbAvtpTimeDelete(avtp_time_t *pAvtpTime);

/** Set to wall time (gPTP time).
 *
 * Set the time in the avtp_time_t structure to that of the synchronized PTP
 * time. An interface module will normally use this function to set the time
 * that media data was placed into the media queue.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 */
void openavbAvtpTimeSetToWallTime(avtp_time_t *pAvtpTime);

/** Set to system time.
 *
 * Set the time in the avtp_time_t structure to that of the system time on the
 * device.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 */
void openavbAvtpTimeSetToSystemTime(avtp_time_t *pAvtpTime);

/** Set to timestamp.
 *
 * Set the time in the avtp_time_t structure to the value of the timestamp
 * parameter which is in the same format as an AVTP timestamp.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param timestamp A timestamp in the same format as the 1722 AVTP timestamp.
 */
void openavbAvtpTimeSetToTimestamp(avtp_time_t *pAvtpTime, U32 timestamp);

/** Set to timestamp.
 *
 * Set the time in the avtp_time_t structure to the value of timespec_t
 * *timestamp.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param timestamp A timestamp in the timespec_t format.
 */
void openavbAvtpTimeSetToTimespec(avtp_time_t *pAvtpTime, timespec_t* timestamp);

/** Push a timestamp, for use in Media Clock Recovery (MCR).
 * \note Not available in all platforms.
 *
 * Push a timestamp, for use in Media Clock Recover (MCR). *pAvtpTime is not
 * modified.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param timestamp A timestamp in the same format as the 1722 AVTP timestamp.
 */
void openavbAvtpTimePushMCR(avtp_time_t *pAvtpTime, U32 timestamp);

/** Set the AVTP timestamp valid indicator.
 *
 * Sets the indicator for AVTP timestamp is valid or not.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param validFlag Flag indicating is timestamp is valid.
 */
void openavbAvtpTimeSetTimestampValid(avtp_time_t *pAvtpTime, bool validFlag);

/** Set the AVTP timestamp uncertain indicator.
 *
 * Sets the indicator for AVTP timestamp is uncertain or not.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param uncertainFlag Flag indicating is timestamp is uncertain.
 */
void openavbAvtpTimeSetTimestampUncertain(avtp_time_t *pAvtpTime, bool uncertainFlag);

/** Add microseconds to the time.
 *
 * Add the number of microseconds passed in to the time stored in avtp_time_t.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param uSec Number of microseconds to add to the stored time.
 */
void openavbAvtpTimeAddUSec(avtp_time_t *pAvtpTime, long uSec);

/** Add nanoseconds to the time.
 *
 * Add the number of nanoseconds passed in to the time stored in avtp_time_t.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param nSec Number of nanoseconds to add to the stored time.
 */
void openavbAvtpTimeAddNSec(avtp_time_t *pAvtpTime, long nSec);

/** Subtract microseconds from the time
 *
 * Subtract the number of microseconds passed in from the time
 * stored in avtp_time_t. 
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param uSec Number of microseconds to subtract from the 
 *  	  stored time.
 */
void openavbAvtpTimeSubUSec(avtp_time_t *pAvtpTime, long uSec);

/** Subtract nanoseconds from the time
 *
 * Subtract the number of nanoseconds passed in from the time
 * stored in avtp_time_t. 
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param nSec Number of nanoseconds to subtract from the stored
 *  	  time.
 */
void openavbAvtpTimeSubNSec(avtp_time_t *pAvtpTime, long nSec);

/** Get AVTP timestamp
 *
 * Get the time stored in avtp_time_t and return it in an AVTP timestamp format
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Returns an integer (U32) in the same format as a 1722 AVTP Timestamp.
 */
U32 openavbAvtpTimeGetAvtpTimestamp(avtp_time_t *pAvtpTime);

/** Get AVTP timestamp in nanoseconds
 *
 * Get the time stored in avtp_time_t and return it as a full time value in nanoseconds
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Returns an integer (U64) of the full walltime in 
 *  	   nanoseconds.
 */
U64 openavbAvtpTimeGetAvtpTimeNS(avtp_time_t *pAvtpTime);

/** Get the AVTP timestamp valid indicator.
 *
 * Gets the indicator for AVTP timestamp is valid or not.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Returns TRUE if the AVTP timestamp valid indicator is set otherwise
 *         FALSE.
 */
bool openavbAvtpTimeTimestampIsValid(avtp_time_t *pAvtpTime);

/** Get the AVTP timestamp uncertain indicator.
 *
 * Gets the indicator for AVTP timestamp is uncertain or not.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Returns TRUE if the AVTP timestamp uncertain indicator is set
 *         otherwise FALSE.
 */
bool openavbAvtpTimeTimestampIsUncertain(avtp_time_t *pAvtpTime);

/** Check if time is in the past.
 *
 * Checks if the time stored in avtp_time_t is past the PTP wall time.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Returns TRUE if time is in the past otherwise FALSE.
 */
bool openavbAvtpTimeIsPast(avtp_time_t *pAvtpTime);

/** Check if time is in the past a specific time (PTP time)
 *
 * Checks if the time stored in avtp_time_t is past the time 
 * passed in. 
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure. 
 * \param nSecTime Time in nanoseconds to compare against.
 * \return Returns TRUE if time is in the past otherwise FALSE.
 */
bool openavbAvtpTimeIsPastTime(avtp_time_t *pAvtpTime, U64 nSecTime);

/** Determines microseconds until PTP time.
 *
 * Returns the number of microseconds until the time stored in avtp_time_t
 * reaches the PTP time.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \param pUsecTill An output parameter that is set with the number of
 *        microseconds until the time is reached.
 * \return Return FALSE if greater than 5 second otherwise TRUE.
 */
bool openavbAvtpTimeUsecTill(avtp_time_t *pAvtpTime, U32 *pUsecTill);

/** Returns delta from timestamp and now.
 *
 * Returns difference between timestamp and current time.
 *
 * \param pAvtpTime A pointer to the avtp_time_t structure.
 * \return Difference in microseconds between timestamp and now.
 */
S32 openavbAvtpTimeUsecDelta(avtp_time_t *pAvtpTime);

#endif  // OPENAVB_AVTP_TIME_PUB_H
