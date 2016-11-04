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

static unsigned long long s_basetime = 1000000000LL;


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
  timeRemaining = ((int64_t) timer->expires.sec - (int64_t) timeCurrent.sec) * 1000000000LL + ((int64_t) timer->expires.nsec - (int64_t) timeCurrent.nsec);
  return (timeRemaining > 0LL ? timeRemaining : 0LL);
}

void Time_add(Time *a, const Time *b)
{
  a->sec = a->sec + b->sec;
  a->nsec = a->nsec + b->nsec;
  if (a->nsec > 1000000000L) {
    a->sec++;
    a->nsec = a->nsec - 1000000000L;
  }
}

int64_t Time_diff(const Time *a, const Time *b)
{
  int64_t a_ns = (int64_t) a->sec * 1000000000LL + (int64_t) a->nsec;
  int64_t b_ns = (int64_t) b->sec * 1000000000LL + (int64_t) b->nsec;
  return b_ns - a_ns;
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
  t->sec = (unsigned long) (s_basetime / 1000000000LL);
  t->nsec = (unsigned long) (s_basetime % 1000000000LL);
}

void Time_dump(const Time *t)
{
  printf("sec: %lu nsec: %09lu", t->sec, t->nsec);
}


/* Special function used for testing only. */
void Time_increaseNanos(uint64_t nsec)
{
  assert(nsec < 60LL * 1000000000LL);
  s_basetime += nsec;
}
