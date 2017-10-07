/*************************************************************************************************************
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
*************************************************************************************************************/

#ifndef SHAPER_HELPER_LINUX_H
#define SHAPER_HELPER_LINUX_H

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

// Uncomment to use manual data alignment adjustments. Not needed for Linux
//#define DATA_ALIGNMENT_ADJUSTMENT	1

/// Number of nanoseconds in second
#define NANOSECONDS_PER_SECOND		(1000000000ULL)
/// Number of nanoseconds in millisecond
#define NANOSECONDS_PER_MSEC		(1000000L)
/// Number of nanoseconds in microsecond
#define NANOSECONDS_PER_USEC		(1000L)
/// Number of microseconds in second
#define MICROSECONDS_PER_SECOND		(1000000L)
/// Number of microseconds in millisecond
#define MICROSECONDS_PER_MSEC		(1000L)

#define SLEEP(sec)  							   sleep(sec)
#define SLEEP_MSEC(mSec)						   usleep(mSec * 1000)
#define SLEEP_NSEC(nSec)						   usleep(nSec / 1000)
#define SLEEP_UNTIL_NSEC(nSec)  				   xSleepUntilNSec(nSec)
inline static void xSleepUntilNSec(uint64_t nSec)
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

#define THREAD_SET_RT_PRIORITY(threadhandle, priority) 													\
	{																									\
		struct sched_param param;																		\
		param.__sched_priority = priority;																\
		pthread_setschedparam(threadhandle##_ThreadData.pthread, SCHED_RR, &param);						\
	}

#define THREAD_PIN(threadhandle, affinity) 																		\
	{																									\
		cpu_set_t cpuset;																				\
		int i1;																							\
		CPU_ZERO(&cpuset);																				\
		for (i1 = 0; i1 < 32; i1++) {																	\
			if (affinity & (1 << i1)) CPU_SET(i1, &cpuset);																		\
		}																								\
		pthread_setaffinity_np(threadhandle##_ThreadData.pthread, sizeof(cpu_set_t), &cpuset);			\
	}

#define THREAD_CHECK_ERROR(threadhandle, message, error)										\
	do {																						\
		error=FALSE;																			\
		if (threadhandle##_ThreadData.err != 0) 												\
		{   																					\
			SHAPER_LOGF_ERROR("Thread error: %s code: %d", message, threadhandle##_ThreadData.err);   \
			error=TRUE; 																		\
			break;  																			\
		}   																					\
	} while (0)

#define THREAD_STARTTHREAD(err)
#define THREAD_KILL(threadhandle, signal)   	   pthread_kill(threadhandle##_ThreadData.pthread, signal)
#define THREAD_JOINABLE(threadhandle)
#define THREAD_JOIN(threadhandle, signal)   	   pthread_join(threadhandle##_ThreadData.pthread, (void**)signal)
#define THREAD_SLEEP(threadhandle, secs)		   sleep(secs)

#define THREAD_SELF()							   pthread_self()
#define GET_PID()								   getpid()

#define SEM_T(sem) sem_t sem;
#define SEM_ERR_T(err) int err;
#define SEM_INIT(sem, init, err) err = sem_init(&sem, 0, init);
#define SEM_WAIT(sem, err) err = sem_wait(&sem);
#define SEM_POST(sem, err) err = sem_post(&sem);
#define SEM_DESTROY(sem, err) err = sem_destroy(&sem);
#define SEM_IS_ERR_NONE(err) (0 == err)
#define SEM_IS_ERR_TIMEOUT(err) (ETIMEDOUT == err || -1 == err)
#define SEM_LOG_ERR(err) if (0 != err) SHAPER_LOGF_ERROR("Semaphore error code: %d", err);

#define MUTEX_ATTR_TYPE_DEFAULT				  	   PTHREAD_MUTEX_DEFAULT
#define MUTEX_ATTR_TYPE_RECURSIVE				   PTHREAD_MUTEX_RECURSIVE
#define MUTEX_HANDLE(mutex_handle)                 pthread_mutex_t mutex_handle
#define MUTEX_ATTR_HANDLE(mutex_attr_name)         pthread_mutexattr_t mutex_attr_name
#define MUTEX_ATTR_CREATE_ERR()                    static mutex_err
#define MUTEX_ATTR_INIT(mutex_attr_name)           pthread_mutexattr_init(&mutex_attr_name)
#define MUTEX_ATTR_SET_TYPE(mutex_attr_name,type)   pthread_mutexattr_settype(&mutex_attr_name, type)
#define MUTEX_ATTR_SET_NAME(mutex_attr_name, name)
#define MUTEX_CREATE_ERR()                         int mutex_err
#define MUTEX_CREATE(mutex_handle,mutex_attr_name) mutex_err=pthread_mutex_init(&mutex_handle, &mutex_attr_name)
#define MUTEX_LOCK(mutex_handle)                   mutex_err=pthread_mutex_lock(&mutex_handle)
#define MUTEX_UNLOCK(mutex_handle)                 mutex_err=pthread_mutex_unlock(&mutex_handle)
#define MUTEX_DESTROY(mutex_handle)                mutex_err=pthread_mutex_destroy(&mutex_handle)
#define MUTEX_LOG_ERR(message)  			   	   if (mutex_err) SHAPER_LOG_ERROR(message);
#define MUTEX_IS_ERR  (mutex_err != 0)

// Alternate simplified mutex mapping
#define MUTEX_HANDLE_ALT(mutex_handle) pthread_mutex_t mutex_handle
#define MUTEX_CREATE_ALT(mutex_handle) pthread_mutex_init(&mutex_handle, NULL)
#define MUTEX_LOCK_ALT(mutex_handle) pthread_mutex_lock(&mutex_handle)
#define MUTEX_UNLOCK_ALT(mutex_handle) pthread_mutex_unlock(&mutex_handle)
#define MUTEX_DESTROY_ALT(mutex_handle) pthread_mutex_destroy(&mutex_handle)

#endif // SHAPER_HELPER_LINUX_H
