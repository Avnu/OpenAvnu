/******************************************************************************

Copyright (c) 2009-2012, Intel Corporation
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

#include <windows_hal_common.hpp>

#include <tsc.hpp>

#include <Windows.h>

uint64_t WindowsTimestamperBase::scaleTSCClockToNanoseconds(uint64_t time) {
	long double scaled_output = ((long double)tsc_hz.QuadPart) / 1000000000;
	scaled_output = ((long double)time) / scaled_output;
	return (uint64_t)scaled_output;
}

DWORD WINAPI OSThreadCallback(LPVOID input) {
	OSThreadArg *arg = (OSThreadArg*)input;
	arg->ret = arg->func(arg->arg);
	return 0;
}

VOID CALLBACK WindowsTimerQueueHandler(PVOID arg_in, BOOLEAN ignore) {
	WindowsTimerQueueHandlerArg *arg = (WindowsTimerQueueHandlerArg *)arg_in;
	size_t diff = 0;

	// Remove myself from unexpired timer queue
	arg->queue->lock();
	if (!arg->queue->stopped) {
		diff = arg->timer_queue->arg_list.size();
		if (diff != 0) {
			arg->timer_queue->arg_list.remove(arg);
			diff -= arg->timer_queue->arg_list.size();
		}


		// If diff == 0 the event was already cancelled
		if (diff > 0) arg->func(arg->inner_arg);
	}
	arg->queue->unlock();

	// Add myself to the expired timer queue
	AcquireSRWLockExclusive(&arg->queue->retiredTimersLock);
	arg->queue->retiredTimers.push_front(arg);
	ReleaseSRWLockExclusive(&arg->queue->retiredTimersLock);
}

OSLockResult WindowsLock::lock_l(DWORD timeout) {
	DWORD wait_result = WaitForSingleObject(lock_c, timeout);
	if (wait_result == WAIT_TIMEOUT) return oslock_held;
	else if (wait_result == WAIT_OBJECT_0) return oslock_ok;
	else return oslock_fail;

}

OSLockResult WindowsLock::nonreentrant_lock_l(DWORD timeout) {
	OSLockResult result;
	DWORD wait_result;
	wait_result = WaitForSingleObject(lock_c, timeout);
	if (wait_result == WAIT_OBJECT_0) {
		if (thread_id == GetCurrentThreadId()) {
			result = oslock_self;
			ReleaseMutex(lock_c);
		}
		else {
			result = oslock_ok;
			thread_id = GetCurrentThreadId();
		}
	}
	else if (wait_result == WAIT_TIMEOUT) result = oslock_held;
	else result = oslock_fail;

	return result;
}

bool WindowsLock::initialize(OSLockType type) {
	lock_c = CreateMutex(NULL, false, NULL);
	if (lock_c == NULL) return false;
	this->type = type;
	return true;
}

OSLockResult WindowsLock::lock(const char *caller) {
	if (type == oslock_recursive) {
		return lock_l(INFINITE);
	}
	return nonreentrant_lock_l(INFINITE);
}

OSLockResult WindowsLock::trylock(const char *caller) {
	if (type == oslock_recursive) {
		return lock_l(0);
	}
	return nonreentrant_lock_l(0);
}

OSLockResult WindowsLock::unlock(const char *caller) {
	ReleaseMutex(lock_c);
	return oslock_ok;
}

bool WindowsCondition::wait() {
	bool ret = true;
	while (!getcsig()) { // We're signaled
		if (SleepConditionVariableSRW(&condition, &lock, INFINITE, 0) != TRUE) {
			ret = false;
			break;
		}
	}
	down();
	ReleaseSRWLockExclusive(&lock);
	return ret;
}

bool WindowsNamedPipeIPC::init(OS_IPC_ARG *arg) {
	IPCListener *ipclistener;
	IPCSharedData ipcdata = { &peerlist, &loffset };

	ipclistener = new IPCListener();
	// Start IPC listen thread
	if (!ipclistener->start(ipcdata)) {
		XPTPD_ERROR("Starting IPC listener thread failed");
	}
	else {
		XPTPD_INFO("Starting IPC listener thread succeeded");
	}

	return true;
}

bool WindowsNamedPipeIPC::setSystemClockFrequency(LARGE_INTEGER frequency) {
	loffset.get();
	loffset.tick_frequency = frequency.QuadPart;
	loffset.put();

	return true;
}

bool WindowsNamedPipeIPC::update(clock_offset_t *update) {


	loffset.get();
	loffset.local_time = update->master_time + update->ml_phoffset;
	loffset.ml_freqoffset = update->ml_freqoffset;
	loffset.ml_phoffset = update->ml_phoffset;
	loffset.ls_freqoffset = update->ls_freqoffset;
	loffset.ls_phoffset = update->ls_phoffset;

	if (!loffset.isReady()) loffset.setReady(true);
	loffset.put();
	return true;
}
