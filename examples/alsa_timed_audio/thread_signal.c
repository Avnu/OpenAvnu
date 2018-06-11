/******************************************************************************

  Copyright (c) 2018, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <thread_signal.h>
#include <sdk.h>

#include <pthread.h>
#include <stdlib.h>
#include <errno.h>

struct isaudk_signal
{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	bool signaled;
};

isaudk_signal_error_t
isaudk_create_signal( struct isaudk_signal **signal )
{
	int err;

	*signal = (struct isaudk_signal *)
		malloc((size_t) sizeof( **signal ));
	if( *signal == NULL )
		return ISAUDK_SIGNAL_SYSERROR;

	err = pthread_cond_init( &(*signal)->cond, NULL );
	if( err != 0 )
		return ISAUDK_SIGNAL_SYSERROR;

	err = pthread_mutex_init( &(*signal)->mutex, NULL );
	if( err != 0 )
		return ISAUDK_SIGNAL_SYSERROR;

	return ISAUDK_SIGNAL_OK;
}

isaudk_signal_error_t
isaudk_free_signal( isaudk_signal_t signal )
{
	int err;

	err = pthread_mutex_lock( &signal->mutex );
	if( err )
		return ISAUDK_SIGNAL_SYSERROR;

	err = pthread_cond_destroy( &signal->cond );
	if( err )
		return ISAUDK_SIGNAL_SYSERROR;

	err = pthread_mutex_destroy( &signal->mutex );
	if( err )
		return ISAUDK_SIGNAL_SYSERROR;

	free(( void * ) signal );

	return ISAUDK_SIGNAL_OK;
}

#define NS_PER_SEC (1000000000LL)
#define MS_PER_SEC (1000)
#define NS_PER_MS (NS_PER_SEC/MS_PER_SEC)

static inline void
timespec_add_ns( struct timespec *ts, unsigned ns )
{
	ts->tv_nsec += ns % NS_PER_SEC;
	ts->tv_sec += ns / NS_PER_SEC;
	if( ts->tv_nsec >= NS_PER_SEC ) {
		ts->tv_nsec -= NS_PER_SEC;
		ts->tv_sec += 1;
	}
}

#include <stdio.h>

// Wait for signal, *wait* == 0: forever, *wait* != 0: *wait* milliseconds
isaudk_signal_error_t
isaudk_signal_wait( isaudk_signal_t signal, unsigned wait )
{
	int err;
	isaudk_signal_error_t retval = ISAUDK_SIGNAL_OK;

	err = pthread_mutex_lock( &signal->mutex );
	if( err != 0 )
		return ISAUDK_SIGNAL_SYSERROR;

	while( !signal->signaled )
	{
		if( wait != 0 )
		{
			struct timespec timeout;

			err = clock_gettime( CLOCK_REALTIME, &timeout );
			if( err != 0 )
			{
				retval = ISAUDK_SIGNAL_SYSERROR;
				goto done;
			}
			timespec_add_ns( &timeout, wait*NS_PER_MS );

			err = pthread_cond_timedwait
				( &signal->cond, &signal->mutex, &timeout );
		}
		else
		{
			err = pthread_cond_wait
				( &signal->cond, &signal->mutex );
		}
		if( err == ETIMEDOUT )
			goto done;
	}

	signal->signaled = false;

done:
	err = pthread_mutex_unlock( &signal->mutex );
	if( err != 0 )
		return ISAUDK_SIGNAL_SYSERROR;

	return retval;
}

isaudk_signal_error_t
isaudk_signal_send( isaudk_signal_t signal )
{
	int err;
	isaudk_signal_error_t retval = ISAUDK_SIGNAL_OK;

	err = pthread_mutex_lock( &signal->mutex );
	if( err != 0 )
	{
		retval = ISAUDK_SIGNAL_SYSERROR;
		goto done;
	}

	signal->signaled = true;
	err = pthread_cond_broadcast( &signal->cond );
	if( err != 0 )
	{
		retval = ISAUDK_SIGNAL_SYSERROR;
		goto done;
	}

done:
	err = pthread_mutex_unlock( &signal->mutex );
	if( err != 0 )
		return ISAUDK_SIGNAL_SYSERROR;

	return ISAUDK_SIGNAL_OK;
}
