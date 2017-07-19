/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

#ifndef _OPENAVB_TIME_OSAL_PUB_H
#define _OPENAVB_TIME_OSAL_PUB_H

// timer Macros
#include <sys/timerfd.h>
#include <time.h>

#define AVB_TIME_INIT() osalAVBTimeInit()
#define AVB_TIME_CLOSE() osalAVBTimeClose()

#define TIMERFD_CREATE(arg1, arg2) timerfd_create(arg1, arg2)
#define TIMERFD_SETTIME(arg1, arg2, arg3, arg4) timerfd_settime(arg1, arg2, arg3, arg4)
#define TIMER_CLOSE(arg1) close(arg1)

// In this Linux port all clock IDs preceding OPENAVB_CLOCK_WALLTIME will be set to clock_gettime()
typedef enum {
	OPENAVB_CLOCK_REALTIME,
	OPENAVB_CLOCK_MONOTONIC,
	OPENAVB_TIMER_CLOCK,
	OPENAVB_CLOCK_WALLTIME
} openavb_clockId_t;

#define CLOCK_GETTIME(arg1, arg2) osalClockGettime(arg1, arg2)
#define CLOCK_GETTIME64(arg1, arg2) osalClockGettime64(arg1, arg2)

// Initialize the AVB Time system for client usage
bool osalAVBTimeInit(void);

// Close the AVB Time system for client usage
bool osalAVBTimeClose(void);

// Gets current time. Returns 0 on success otherwise -1
bool osalClockGettime(openavb_clockId_t openavbClockId, struct timespec *getTime);

// Gets current time as U64 nSec. Returns 0 on success otherwise -1
bool osalClockGettime64(openavb_clockId_t openavbClockId, U64 *timeNsec);


#endif // _OPENAVB_TIME_OSAL_PUB_H
