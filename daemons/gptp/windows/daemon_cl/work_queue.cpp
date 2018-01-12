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

#include <work_queue.hpp>
#include <stdio.h>


struct WWQueueThreadState {
	bool running;
	bool stop;
	WWQueueCallback task;
	LPVOID arg;
};

DWORD WINAPI WindowsWorkQueueLoop(LPVOID arg) {
	WWQueueThreadState *state = (WWQueueThreadState *)arg;
	state->running = true;
	while (!state->stop) {
		if (state->task != NULL) {
			state->task(state->arg);
			delete state->arg;
			state->task = NULL;
		}
		Sleep(1);
	}
	state->running = false;

	return 0;
}

bool WindowsWorkQueue::init(int number_threads)
{
	if (number_threads == 0) number_threads = DEFAULT_THREAD_COUNT;
	state = new WWQueueThreadState[number_threads];
	for (int i = 0; i < number_threads; ++i) {
		state[i].running = false;
		state[i].stop = false;
		state[i].task = NULL;
	}
	workers = new HANDLE[number_threads];
	for (int i = 0; i < number_threads; ++i) {
		workers[i] = CreateThread(NULL, 0, WindowsWorkQueueLoop, state + i, 0, NULL);
		if (workers[i] == INVALID_HANDLE_VALUE)
			return false;
		while (!state[i].running)
			Sleep(1);
	}
	this->number_threads = number_threads;
	return true;
}

bool WindowsWorkQueue::submit(WWQueueCallback cb, LPVOID arg)
{
	int i;

	for (i = 0; i < number_threads; ++i) {
		if (state[i].task == NULL) {
			state[i].arg = arg;
			state[i].task = cb;
			break;
		}
	}
	if (i == number_threads)
		return false;

	return true;
}

void WindowsWorkQueue::stop()
{
	for (int i = 0; i < number_threads; ++i) {
		state[i].stop = true;
		while (state[i].running)
			Sleep(1);
	}
}
