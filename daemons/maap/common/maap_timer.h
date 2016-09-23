#ifndef MAAP_TIMER_H
#define MAAP_TIMER_H

#include <stdint.h>

#include "platform.h"

typedef struct maap_timer Timer;
typedef OS_TIME_TYPE Time;

Timer *Time_newTimer();

void Time_delTimer(Timer *timer);

void Time_setTimer(Timer *timer, Time *t);


void Time_add(Time *a, Time *b);

int  Time_cmp(Time *a, Time *b);

int  Time_passed(Time *current, Time *target);

void Time_setFromNanos(Time *t, uint64_t nsec);

void Time_setFromMonotonicTimer(Time *t);

void Time_dump(Time *t);

#endif
