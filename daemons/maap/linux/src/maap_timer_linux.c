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
#include <signal.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

struct maap_timer {
	timer_t timer_id;
};

#include "maap_timer.h"

#define MAAP_LOG_COMPONENT "Timer"
#include "maap_log.h"


Timer *Time_newTimer(void)
{
	struct sigevent sev;
	Timer * newTimer = malloc(sizeof (Timer));
	if (newTimer)
	{
		sev.sigev_notify = SIGEV_NONE;
		sev.sigev_signo = 0;
		sev.sigev_value.sival_ptr = NULL;
		if (timer_create(CLOCK_MONOTONIC, &sev, &(newTimer->timer_id)) < 0)
		{
			newTimer->timer_id = (timer_t)(-1);
		}
	}
	return newTimer;
}

void Time_delTimer(Timer *timer)
{
	assert(timer);
	if (timer && timer->timer_id != (timer_t)(-1))
	{
		timer_delete(timer->timer_id);
	}
	free(timer);
}

void Time_setTimer(Timer *timer, const Time *t)
{
	struct itimerspec tspec;
	tspec.it_value.tv_sec = t->tv_sec;
	tspec.it_value.tv_nsec = t->tv_nsec;
	tspec.it_interval.tv_sec = 0;
	tspec.it_interval.tv_nsec = 0;
	timer_settime(timer->timer_id, TIMER_ABSTIME, &tspec, NULL);
}

int64_t Time_remaining(Timer *timer)
{
	struct itimerspec curr_value;

	assert(timer);
	if (timer_gettime(timer->timer_id, &curr_value) < 0)
	{
		MAAP_LOGF_ERROR("Error %d getting the timer time remaining (%s)", errno, strerror(errno));
		return -1;
	}

	return ((int64_t) curr_value.it_value.tv_sec * 1000000000LL + (int64_t) curr_value.it_value.tv_nsec);
}


void Time_add(Time *a, const Time *b)
{
	a->tv_sec = a->tv_sec + b->tv_sec;
	a->tv_nsec = a->tv_nsec + b->tv_nsec;
	if (a->tv_nsec > 1000000000L) {
		a->tv_sec++;
		a->tv_nsec = a->tv_nsec - 1000000000L;
	}
}

int64_t Time_diff(const Time *a, const Time *b)
{
	int64_t a_ns = (int64_t) a->tv_sec * 1000000000LL + (int64_t) a->tv_nsec;
	int64_t b_ns = (int64_t) b->tv_sec * 1000000000LL + (int64_t) b->tv_nsec;
	return b_ns - a_ns;
}

int Time_cmp(const Time *a, const Time *b)
{
	if (a->tv_sec < b->tv_sec) {
		return -1;
	}
	if (a->tv_sec > b->tv_sec) {
		return 1;
	}
	if (a->tv_nsec < b->tv_nsec) {
		return -1;
	}
	if (a->tv_nsec > b->tv_nsec) {
		return 1;
	}
	return 0;
}

int Time_passed(const Time *current, const Time *target)
{
	if (current->tv_sec < target->tv_sec) {
		return 0;
	}
	if (current->tv_sec == target->tv_sec && current->tv_nsec < target->tv_nsec) {
		return 0;
	}
	return 1;
}

void Time_setFromNanos(Time *t, uint64_t nsec)
{
	t->tv_sec = nsec / 1000000000LL;
	t->tv_nsec = nsec - t->tv_sec * 1000000000LL;
}

void Time_setFromMonotonicTimer(Time *t)
{
	clock_gettime(CLOCK_MONOTONIC, t);
}

const char * Time_dump(const Time *t)
{
	static char buffer[40];
	sprintf(buffer, "%lu sec, %09lu nsec", (unsigned long)t->tv_sec, (unsigned long)t->tv_nsec);
	return buffer;
}
