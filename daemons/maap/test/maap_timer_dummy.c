#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>

#include "platform.h"

struct testtime {
  unsigned long sec;
  unsigned long nsec;
};

/* Use our local time structure rather than the OS-specific one. */
#undef OS_TIME_TYPE
#define OS_TIME_TYPE struct testtime

#include "maap_timer.h"


struct maap_timer {
  int timer_id;
  struct testtime expires;
};


Timer *Time_newTimer(void)
{
  static int s_new_timer_id = 0;

  Timer *newTimer = malloc(sizeof (Timer));
  if (newTimer) {
    newTimer->timer_id = ++s_new_timer_id;
    newTimer->expires.sec = newTimer->expires.nsec = 0;
  }
  return newTimer;
}

void Time_delTimer(Timer *timer)
{
  assert(timer);
  assert(timer->timer_id);
  free(timer);
}

void Time_setTimer(Timer *timer, const Time *t)
{
  assert(timer);
  assert(timer->timer_id);
  assert(t);
  timer->expires.sec = t->sec;
  timer->expires.nsec = t->nsec;
}

int64_t Time_remaining(Timer *timer)
{
  Time timeCurrent;
  int64_t timeRemaining;

  assert(timer);
  assert(timer->timer_id);
  assert(timer->expires.sec || timer->expires.nsec);
  Time_setFromMonotonicTimer(&timeCurrent);
  timeRemaining = ((long) timer->expires.sec - (long) timeCurrent.sec) * 1000000000LL + ((long) timer->expires.nsec - (long) timeCurrent.nsec);
  return (timeRemaining > 0LL ? timeRemaining : 0LL);
}

void Time_add(Time *a, const Time *b)
{
  a->sec = a->sec + b->sec;
  a->nsec = a->nsec + b->nsec;
  if (a->nsec > 1000000000) {
    a->sec++;
    a->nsec = a->nsec - 1000000000;
  }
}

int  Time_cmp(const Time *a, const Time *b)
{
  if (a->sec < b->sec) {
    return -1;
  }
  if (a->sec > b->sec) {
    return 1;
  }
  if (a->nsec < b->nsec) {
    return -1;
  }
  if (a->nsec > b->nsec) {
    return 1;
  }
  return 0;
}

int  Time_passed(const Time *current, const Time *target)
{
  if (current->sec < target->sec) {
    return 0;
  }
  if (current->sec == target->sec && current->nsec < target->nsec) {
    return 0;
  }
  return 1;
}

void Time_setFromNanos(Time *t, uint64_t nsec)
{
  t->sec = (unsigned long) (nsec / 1000000000LL);
  t->nsec = (unsigned long) (nsec % 1000000000LL);
}

void Time_setFromMonotonicTimer(Time *t)
{
  /* Use a hard-wired value. */
  static unsigned long long s_basetime = 0;
  s_basetime += 100000000LL; /* The next time will be 0.1 seconds greater than the last time used. */
  t->sec = (unsigned long) (s_basetime / 1000000000LL);
  t->nsec = (unsigned long) (s_basetime % 1000000000LL);
}

void Time_dump(const Time *t)
{
  printf("sec: %lu nsec: %lu", t->sec, t->nsec);
}
