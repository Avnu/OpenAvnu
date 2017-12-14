/******************************************************************************

Copyright (c) 2009-2015, Intel Corporation
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

#ifndef WORK_QUEUE_HPP
#define WORK_QUEUE_HPP

#include <WTypesbase.h>

#define DEFAULT_THREAD_COUNT (5)

struct WWQueueThreadState;

typedef void(*WWQueueCallback)(LPVOID arg);

class WindowsWorkQueue
{
private:
	HANDLE *workers;
	WWQueueThreadState *state;
	int number_threads;

public:
	/**
	 * @brief initialize work queue
	 * @param number_threads [in] number of threads (0 = default)
	 * @return true on success
	 */
	bool init( int number_threads );

	/**
	 * @brief submit job to work queue
	 * @param cb [in] function to call
	 * @param arg [in] parameter provided to callback
	 * @return true on success
	 */
	bool submit( WWQueueCallback cb, LPVOID arg );

	/**
	 * @brief stop work queue
	 */
	void stop();
};

#endif/*WORK_QUEUE_HPP*/
