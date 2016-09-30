#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

struct maap_timer {
  timer_t timer_id;
};

#include "maap_timer.h"


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

void Time_setTimer(Timer *timer, Time *t)
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
		printf("Error %d getting the timer time remaining\n", errno);
		return -1;
	}

	return (curr_value.it_value.tv_sec * 1000000000L + curr_value.it_value.tv_nsec);
}


void Time_add(Time *a, Time *b)
{
  a->tv_sec = a->tv_sec + b->tv_sec;
  a->tv_nsec = a->tv_nsec + b->tv_nsec;
  if (a->tv_nsec > 1000000000L) {
    a->tv_sec++;
    a->tv_nsec = a->tv_nsec - 1000000000L;
  }
}

int  Time_cmp(Time *a, Time *b)
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

int  Time_passed(Time *current, Time *target)
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

void Time_dump(Time *t)
{
  printf("tv_sec: %lu tv_nsec: %lu", (unsigned long)t->tv_sec, (unsigned long)t->tv_nsec);
}
