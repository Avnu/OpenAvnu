/*************************************************************************************
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
*************************************************************************************/

/**
 * @file
 *
 * @brief Support for platform-specific timer operations
 *
 * These functions support the platform-specific timers allowing for measuring time elapsed and remaining.
 */

#ifndef MAAP_TIMER_H
#define MAAP_TIMER_H

#include <stdint.h>

#include "platform.h"

/** Structure to hold the timer information.  @b struct @b maap_timer will be defined in the platform-specific implementation file. */
typedef struct maap_timer Timer;

/** Time type to use.  @p OS_TIME_TYPE is defined in platform.h */
typedef OS_TIME_TYPE Time;

/**
 * Create a new #Timer pointer to use when measuring time elapsed and remaining.
 *
 * @return A #Timer pointer, or NULL if an error occurred.
 */
Timer *Time_newTimer();

/**
 * Delete a #Timer pointer when done with the timer support.
 *
 * @param timer The pointer to free
 */
void Time_delTimer(Timer *timer);


/**
 * Set the timer to expire at the specified time.
 *
 * @param timer The timer to set
 * @param t The system time when the timer should expire
 */
void Time_setTimer(Timer *timer, const Time *t);

/**
 * Returns the time remaining on the timer.
 *
 * @param timer The timer to test
 *
 * @return The number of nanoseconds remaining on the timer, 0 if the timer has expired, or -1 if an error occurred.
 */
int64_t Time_remaining(Timer *timer);


/**
 * Adds the second time to the first time.
 *
 * @param a Time that the second time is added to.  This value is modified.
 * @param b Time that is added to the first time.  This value is not modified.
 */
void Time_add(Time *a, const Time *b);

/**
 * Returns the difference between the two times in nanoseconds.
 *
 * @param a Time "a" to compare.
 * @param b Time "b" to compare.
 *
 * @return The "a" time in nanoseconds subtracted from the "b" time in nanoseconds.
 * If "a" is greater than "b", the returned value will be negative.
 */
int64_t Time_diff(const Time *a, const Time *b);

/**
 * Compare two different time values.
 *
 * @param a First time to compare
 * @param b Second time to compare
 *
 * @return -1 if the first time is less than the second time, 1 if the first time is greater than the second time, or 0 if they are equal.
 */
int  Time_cmp(const Time *a, const Time *b);

/**
 * Determine if the target time has arrived.
 *
 * @param current Current time
 * @param target Target time
 *
 * @return 1 if the current time is at least as great as the target time, 0 otherwise.
 */
int  Time_passed(const Time *current, const Time *target);


/**
 * Set the Time structure to the number of nanoseconds value.
 *
 * @param t Pointer to the Time structure to set
 * @param nsec Number of nanoseconds to convert into a time
 */
void Time_setFromNanos(Time *t, uint64_t nsec);

/**
 * Set the current time
 *
 * @param t Pointer to the Time structure to fill with the current time
 */
void Time_setFromMonotonicTimer(Time *t);

/**
 * Returns the string equivalent of the supplied time for debugging purposes.
 * The string will remain valid until the next call to Time_dump().
 * @note This function is not multithread safe.
 *
 * @param t Pointer to the time to convert.
 *
 * @return A string representing the supplied timer value.
 */
const char * Time_dump(const Time *t);

#endif
