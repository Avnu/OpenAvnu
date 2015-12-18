/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#ifndef _OPENAVB_OS_SERVICES_OSAL_H
#define _OPENAVB_OS_SERVICES_OSAL_H

#include "openavb_hal.h"

#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/mman.h>
#include <poll.h>
#include <fcntl.h>
#include <net/if.h>
#include <dlfcn.h>

#include "openavb_tasks.h"

#define EXTERN_DLL_EXPORT  extern DLL_EXPORT

#ifndef PTHREAD_MUTEX_RECURSIVE
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

#include "openavb_osal_pub.h"

// Uncomment to use manual data alignment adjustments. Not needed for Linux
//#define DATA_ALIGNMENT_ADJUSTMENT	1

// Many socket implementations support a minimum timeout of 1ms (value 1000 here).
#define RAWSOCK_MIN_TIMEOUT_USEC	1

#define SLEEP(sec)  							   sleep(sec)
#define SLEEP_MSEC(mSec)						   usleep(mSec * 1000)
#define SLEEP_NSEC(nSec)						   usleep(nSec)
#define SLEEP_UNTIL_NSEC(nSec)  				   xSleepUntilNSec(nSec)
inline static void xSleepUntilNSec(U64 nSec)
{
	struct timespec tmpTime;
	tmpTime.tv_sec = nSec / NANOSECONDS_PER_SECOND;
	tmpTime.tv_nsec = nSec % NANOSECONDS_PER_SECOND;
	clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tmpTime, NULL);
}



#define RAND()  								   random()
#define SRAND(seed) 							   srandom(seed)

#define PRAGMA_ALIGN_8

#define SIGNAL_CALLBACK_SETUP(__NAM, __CB)  					\
	struct sigaction __NAM; 									\
	__NAM.sa_handler = __CB

#define SIGNAL_SIGNAL_SETUP(__SIG, __NAM, __CB, __ERR)  		\
	sigemptyset(&__NAM.sa_mask);								\
	__NAM.sa_flags = 0; 										\
	__ERR = sigaction(__SIG, &__NAM, NULL)


// the following macros define thread related items
#define THREAD_TYPE(thread)							\
typedef struct										\
{   												\
	pthread_t pthread;								\
	int err;										\
} thread##_type;

#define THREAD_DEFINITON(thread)					\
thread##_type	thread##_ThreadData

#define THREAD_CREATE(threadName, threadhandle, thread_attr_name, thread_function, thread_function_arg) 			\
	{																												\
		pthread_attr_t thread_attr; 																				\
		do {																										\
			threadhandle##_ThreadData.err = pthread_attr_init(&thread_attr);										\
			if (threadhandle##_ThreadData.err) break;																\
			threadhandle##_ThreadData.err = pthread_attr_setstacksize(&thread_attr, threadName##_THREAD_STK_SIZE);	\
			if (threadhandle##_ThreadData.err) break;																\
			threadhandle##_ThreadData.err = pthread_create(															\
				(pthread_t*)&threadhandle##_ThreadData.pthread, 													\
				&thread_attr,   																					\
				thread_function,																					\
				(void*)thread_function_arg);																		\
		} while (0);																								\
		pthread_attr_destroy(&thread_attr);																			\
	}

#define THREAD_CHECK_ERROR(threadhandle, message, error)										\
	do {																						\
		error=FALSE;																			\
		if (threadhandle##_ThreadData.err != 0) 												\
		{   																					\
			AVB_LOGF_ERROR("Thread error: %s code: %d", message, threadhandle##_ThreadData.err);   \
			error=TRUE; 																		\
			break;  																			\
		}   																					\
	} while (0)

#define THREAD_STARTTHREAD(err)
#define THREAD_KILL(threadhandle, signal)   	   pthread_kill(threadhandle##_ThreadData.pthread, signal)
#define THREAD_JOINABLE(threadhandle)
#define THREAD_JOIN(threadhandle, signal)   	   pthread_join(threadhandle##_ThreadData.pthread, (void**)signal)
#define THREAD_SLEEP(threadhandle, secs)		   sleep(secs)


#define SEM_T(sem) sem_t sem;
#define SEM_ERR_T(err) int err;
#define SEM_INIT(sem, init, err) err = sem_init(&sem, 0, init);
#define SEM_WAIT(sem, err) err = sem_wait(&sem);
#define SEM_TIMEDWAIT(sem, timeoutMSec, err)								\
{																			\
	struct timespec timeout;												\
	CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &timeout);								\
	openavbTimeTimespecAddUsec(&timeout, timeoutMSec * MICROSECONDS_PER_MSEC);	\
	err = sem_timedwait(&sem, &timeout);									\
}
#define SEM_POST(sem, err) err = sem_post(&sem);
#define SEM_DESTROY(sem, err) err = sem_destroy(&sem);
#define SEM_IS_ERR_NONE(err) (0 == err)
#define SEM_IS_ERR_TIMEOUT(err) (ETIMEDOUT == err)
#define SEM_LOG_ERR(err) if (0 != err) AVB_LOGF_ERROR("Semaphore error code: %d", err);


typedef struct
{
	char *libName;
	char *funcName;
	void *libHandle;
} link_lib_t;

#define LINK_LIB(library)					\
link_lib_t library

#endif // _OPENAVB_OS_SERVICES_OSAL_H

