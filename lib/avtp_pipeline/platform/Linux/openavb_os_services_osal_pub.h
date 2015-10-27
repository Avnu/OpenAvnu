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

#ifndef _OPENAVB_OS_SERVICES_OSAL_PUB_H
#define _OPENAVB_OS_SERVICES_OSAL_PUB_H

#define LINUX 1	// !!! FIX ME !!! THIS IS A HACK TO SUPPORT ANOTHER HACK IN openavb_avtp_time.c. REMOVE THIS WHEN openavb_avtp_time.c GETS FIXED !!!

#include "openavb_time_osal_pub.h"

#define INLINE_VARIABLE_NUM_OF_ARGUMENTS inline // must be okay of gcc

#include <netinet/ether.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>


#define STD_LOG		stderr

#define NEWLINE		"\n"

#define SIN(rad) sin(rad)

#define THREAD_SELF()							   pthread_self()
#define GET_PID()								   getpid() 	


// Funky struct to hold a configurable ethernet address (MAC).
// The "mac" pointer is null if no config value was supplied,
// and points to the configured value if one was
typedef struct {
	struct ether_addr buffer;      
	struct ether_addr *mac;
} cfg_mac_t;

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
#define MUTEX_LOG_ERR(message)  			   	   if (mutex_err) AVB_LOG_ERROR(message);
#define MUTEX_IS_ERR  (mutex_err != 0)

// Alternate simplified mutex mapping
#define MUTEX_HANDLE_ALT(mutex_handle) pthread_mutex_t mutex_handle    
#define MUTEX_CREATE_ALT(mutex_handle) pthread_mutex_init(&mutex_handle, NULL)
#define MUTEX_LOCK_ALT(mutex_handle) pthread_mutex_lock(&mutex_handle)     
#define MUTEX_UNLOCK_ALT(mutex_handle) pthread_mutex_unlock(&mutex_handle)
#define MUTEX_DESTROY_ALT(mutex_handle) pthread_mutex_destroy(&mutex_handle)


//	pthread_mutexattr_t   mta;
//	pthread_mutexattr_init(&mta);
//	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE);
//	pthread_mutex_init(&maapMutex, &mta);

#if defined __BYTE_ORDER && defined __BIG_ENDIAN && defined __LITTLE_ENDIAN
#if __BYTE_ORDER == __BIG_ENDIAN
#  define ntohll(x)    (x)
#  define htonll(x)    (x)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#  define ntohll(x)    __bswap_64 (x)
#  define htonll(x)    __bswap_64 (x)
#else
#  error
#endif
#else
#  error
#endif

#endif // _OPENAVB_OS_SERVICES_OSAL_PUB_H

