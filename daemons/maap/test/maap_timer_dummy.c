#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "maap_timer.h"

struct maap_timer {
  int timer_id;
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
  (void)timer; (void)t;
}

int64_t Time_remaining(Timer *timer)
{
  (void)timer;
  return 1;
}

void Time_add(Time *a, Time *b)
{
  a->tv_sec = a->tv_sec + b->tv_sec;
  a->tv_usec = a->tv_usec + b->tv_usec;
  if (a->tv_usec > 1000000) {
    a->tv_sec++;
    a->tv_usec = a->tv_usec - 1000000;
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
  if (a->tv_usec < b->tv_usec) {
    return -1;
  }
  if (a->tv_usec > b->tv_usec) {
    return 1;
  }
  return 0;
}

int  Time_passed(Time *current, Time *target)
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
  t->tv_sec = nsec / 1000000000LL;
  t->tv_usec = (nsec - t->tv_sec * 1000000000LL) / 1000;
}

void Time_setFromMonotonicTimer(Time *t)
{
  gettimeofday(t, NULL);
}

void Time_dump(Time *t)
{
  printf("tv_sec: %lu tv_usec: %lu", (unsigned long)t->tv_sec, (unsigned long)t->tv_usec);
}
