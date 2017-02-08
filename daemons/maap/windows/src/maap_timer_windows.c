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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <time.h>

struct maap_timer {
	/* Time values in nanoseconds */
	uint64_t nStartTime;
	uint64_t nExpireTime;
};

#include "maap_timer.h"

#define MAAP_LOG_COMPONENT "Timer"
#include "maap_log.h"


/* Return the current time, in nanoseconds. */
static uint64_t getCurrentTime(void)
{
	/* Using GetTickCount64() is not ideal, as it only provides a 10-16 millisecond resolution.
	 * However, for our purposes, it is sufficient.
	 * If higher precision is needed, QueryPerformanceCounter() is one possibility. */
	uint64_t nTime = GetTickCount64();
	nTime *= 1000000; /* Convert from milliseconds to nanoseconds */
	return nTime;
}

/* Convert a Time structure to a nanoseconds value */
static uint64_t timeToNanoseconds(const Time *t)
{
	uint64_t nTime =
		((uint64_t)t->tv_sec * 1000000000ULL) + /* Convert seconds to nanoseconds */
		((uint64_t)t->tv_usec * 1000ULL); /* Convert microseconds to nanoseconds */
	return nTime;
}


Timer *Time_newTimer(void)
{
	Timer * newTimer = malloc(sizeof (Timer));
	if (newTimer)
	{
		newTimer->nStartTime = newTimer->nExpireTime = 0ULL;
	}
	return newTimer;
}

void Time_delTimer(Timer *timer)
{
	assert(timer);
	free(timer);
}

void Time_setTimer(Timer *timer, const Time *t)
{
	assert(timer);
	timer->nStartTime = getCurrentTime();
	timer->nExpireTime = timer->nStartTime + timeToNanoseconds(t);
}

int64_t Time_remaining(Timer *timer)
{
	uint64_t curr_value = getCurrentTime();

	assert(timer);
	if (curr_value >= timer->nExpireTime)
	{
		/* Timer has exired. */
		return 0;
	}
	return (int64_t)(timer->nExpireTime - curr_value);
}


void Time_add(Time *a, const Time *b)
{
	a->tv_sec = a->tv_sec + b->tv_sec;
	a->tv_usec = a->tv_usec + b->tv_usec;
	if (a->tv_usec > 1000000L) {
		a->tv_sec++;
		a->tv_usec = a->tv_usec - 1000000L;
	}
}

int64_t Time_diff(const Time *a, const Time *b)
{
	int64_t a_ns = (int64_t)timeToNanoseconds(a);
	int64_t b_ns = (int64_t)timeToNanoseconds(b);
	return b_ns - a_ns;
}

int  Time_cmp(const Time *a, const Time *b)
{
	if (a->tv_sec < b->tv_sec) {
		return -1;
	}
	if (a->tv_sec > b->tv_sec) {
		return 1;
	}
	if (a->tv_usec < b->tv_usec) {
		return -1;
	}
	if (a->tv_usec > b->tv_usec) {
		return 1;
	}
	return 0;
}

int Time_passed(const Time *current, const Time *target)
{
	if (current->tv_sec < target->tv_sec) {
		return 0;
	}
	if (current->tv_sec == target->tv_sec && current->tv_usec < target->tv_usec) {
		return 0;
	}
	return 1;
}

void Time_setFromNanos(Time *t, uint64_t nsec)
{
	t->tv_sec = (long)(nsec / 1000000000LL);
	t->tv_usec = (long)((nsec - (t->tv_sec * 1000000000LL)) / 1000LL);
}

void Time_setFromMonotonicTimer(Time *t)
{
	Time_setFromNanos(t, getCurrentTime());
}

const char * Time_dump(const Time *t)
{
	static char buffer[40];
	sprintf(buffer, "%lu sec, %06lu usec", (unsigned long)t->tv_sec, (unsigned long)t->tv_usec);
	return buffer;
}
