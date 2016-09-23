#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "maap_timer.h"

struct maap_timer {
  timer_t timer_id;
};

#include <stdio.h>

Timer *Time_newTimer(void)
{
  return malloc(sizeof (Timer));
}

void Time_delTimer(Timer *timer)
{
  assert(timer);
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
