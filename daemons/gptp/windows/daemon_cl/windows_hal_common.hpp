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

#ifndef WINDOWS_HAL_COMMON_HPP
#define WINDOWS_HAL_COMMON_HPP

#include <avbts_osnet.hpp>
#include <avbts_oslock.hpp>
#include <avbts_oscondition.hpp>
#include <avbts_ostimerq.hpp>
#include <avbts_ostimer.hpp>
#include <avbts_osthread.hpp>
#include <avbts_osipc.hpp>

#include <packet.hpp>
#include <IPCListener.hpp>

#include <list>
#include <map>

class WindowsTimestamperBase {
protected:
	LARGE_INTEGER tsc_hz;
public:
	LARGE_INTEGER getSystemFrequency() { return tsc_hz; }
	uint64_t scaleTSCClockToNanoseconds(uint64_t time);
};

class WindowsPCAPNetworkInterface : public OSNetworkInterface {
	friend class WindowsPCAPNetworkInterfaceFactory;
private:
	pfhandle_t handle;
	LinkLayerAddress local_addr;
public:
	virtual net_result send(LinkLayerAddress *addr, uint8_t *payload, size_t length, bool timestamp) {
		packet_addr_t dest;
		addr->toOctetArray(dest.addr);
		if (sendFrame(handle, &dest, PTP_ETHERTYPE, payload, length) != PACKET_NO_ERROR) return net_fatal;
		return net_succeed;
	}
	virtual net_result nrecv(LinkLayerAddress *addr, uint8_t *payload, size_t &length) {
		packet_addr_t dest;
		packet_error_t pferror = recvFrame(handle, &dest, payload, length);
		if (pferror != PACKET_NO_ERROR && pferror != PACKET_RECVTIMEOUT_ERROR) return net_fatal;
		if (pferror == PACKET_RECVTIMEOUT_ERROR) return net_trfail;
		*addr = LinkLayerAddress(dest.addr);
		return net_succeed;
	}
	virtual void getLinkLayerAddress(LinkLayerAddress *addr) {
		*addr = local_addr;
	}
	virtual unsigned getPayloadOffset() {
		return PACKET_HDR_LENGTH;
	}
	virtual ~WindowsPCAPNetworkInterface() {
		closeInterface(handle);
		if (handle != NULL) freePacketHandle(handle);
	}
protected:
	WindowsPCAPNetworkInterface() { handle = NULL; };
};

class WindowsPCAPNetworkInterfaceFactory : public OSNetworkInterfaceFactory {
public:
	virtual bool createInterface(OSNetworkInterface **net_iface, InterfaceLabel *label,
		Timestamper *timestamper, LinkLayerAddress *remote) {
		WindowsPCAPNetworkInterface *net_iface_l = new WindowsPCAPNetworkInterface();
		LinkLayerAddress *addr = dynamic_cast<LinkLayerAddress *>(label);
		if (addr == NULL) goto error_nofree;
		net_iface_l->local_addr = *addr;
		packet_addr_t pfaddr;
		addr->toOctetArray(pfaddr.addr);
		packet_addr_t raddr;
		if (remote == NULL) { raddr = ETHER_ADDR_ANY; }
		else { remote->toOctetArray(raddr.addr); }
		if (mallocPacketHandle(&net_iface_l->handle) != PACKET_NO_ERROR) goto error_nofree;
		if (openInterfaceByAddr(net_iface_l->handle, &pfaddr, 1) != PACKET_NO_ERROR) goto error_free_handle;
		if (packetBind(net_iface_l->handle, PTP_ETHERTYPE, raddr) != PACKET_NO_ERROR) goto error_free_handle;
		*net_iface = net_iface_l;

		return true;

	error_free_handle:
	error_nofree:
		delete net_iface_l;

		return false;
	}
};

class WindowsLock : public OSLock {
	friend class WindowsLockFactory;
private:
	OSLockType type;
	DWORD thread_id;
	HANDLE lock_c;
	OSLockResult lock_l(DWORD timeout);
	OSLockResult nonreentrant_lock_l(DWORD timeout);
protected:
	WindowsLock(const char *lock_name, bool debug_lock) : OSLock(lock_name, debug_lock) {
		lock_c = NULL;
	}
	bool initialize(OSLockType type);
	OSLockResult lock(const char *caller = NULL);
	OSLockResult trylock(const char *caller = NULL);
	OSLockResult unlock(const char *caller = NULL);
};

class WindowsLockFactory : public OSLockFactory {
public:
	OSLock *createLock(OSLockType type, const char *lock_name = NULL, bool debug_lock = false) {
		WindowsLock *lock = new WindowsLock(lock_name, debug_lock);
		if (!lock->initialize(type)) {
			delete lock;
			lock = NULL;
		}
		return lock;
	}
};

class WindowsCondition : public OSCondition {
	friend class WindowsConditionFactory;
private:
	SRWLOCK lock;
	CONDITION_VARIABLE condition;
protected:
	bool initialize() {
		InitializeSRWLock(&lock);
		InitializeConditionVariable(&condition);
		return true;
	}
public:
	bool wait_prelock() {
		AcquireSRWLockExclusive(&lock);
		up();
		return true;
	}
	bool wait();
	bool signal() {
		AcquireSRWLockExclusive(&lock);
		setcsig();
		if (waiting()) WakeAllConditionVariable(&condition);
		ReleaseSRWLockExclusive(&lock);
		return true;
	}
};

class WindowsConditionFactory : public OSConditionFactory {
public:
	OSCondition *createCondition() {
		WindowsCondition *result = new WindowsCondition();
		return result->initialize() ? result : NULL;
	}
};

class WindowsTimerQueue;

struct TimerQueue_t;

struct WindowsTimerQueueHandlerArg {
	HANDLE timer_handle;
	HANDLE queue_handle;
	event_descriptor_t *inner_arg;
	ostimerq_handler func;
	int type;
	bool rm;
	WindowsTimerQueue *queue;
	TimerQueue_t *timer_queue;
};

typedef std::list<WindowsTimerQueueHandlerArg *> TimerArgList_t;

struct TimerQueue_t {
	TimerArgList_t arg_list;
	HANDLE queue_handle;
};


VOID CALLBACK WindowsTimerQueueHandler(PVOID arg_in, BOOLEAN ignore);

typedef std::map<int, TimerQueue_t> TimerQueueMap_t;

class WindowsTimerQueue : public OSTimerQueue {
	friend class WindowsTimerQueueFactory;
	friend VOID CALLBACK WindowsTimerQueueHandler(PVOID arg_in, BOOLEAN ignore);
private:
	TimerQueueMap_t timerQueueMap;
	TimerArgList_t retiredTimers;
	SRWLOCK retiredTimersLock;
	void cleanupRetiredTimers(bool final) {
		AcquireSRWLockExclusive(&retiredTimersLock);
		while (!retiredTimers.empty()) {
			WindowsTimerQueueHandlerArg *retired_arg = retiredTimers.front();
			retiredTimers.pop_front();
			ReleaseSRWLockExclusive(&retiredTimersLock);
			if (!final) DeleteTimerQueueTimer(retired_arg->queue_handle, retired_arg->timer_handle, INVALID_HANDLE_VALUE);
			if (retired_arg->rm) delete retired_arg->inner_arg;
			delete retired_arg;
			AcquireSRWLockExclusive(&retiredTimersLock);
		}
		ReleaseSRWLockExclusive(&retiredTimersLock);

	}
protected:
	WindowsTimerQueue() {
		InitializeSRWLock(&retiredTimersLock);
	};
public:
	bool addEvent(unsigned long micros, int type, ostimerq_handler func, event_descriptor_t *arg, bool rm, unsigned *event) {
		WindowsTimerQueueHandlerArg *outer_arg;
		outer_arg = new WindowsTimerQueueHandlerArg();
		cleanupRetiredTimers(stopped);

		if (!stopped) {
			if (timerQueueMap.find(type) == timerQueueMap.end()) {
				timerQueueMap[type].queue_handle = CreateTimerQueue();
			}
			outer_arg->queue_handle = timerQueueMap[type].queue_handle;
			outer_arg->inner_arg = arg;
			outer_arg->func = func;
			outer_arg->queue = this;
			outer_arg->type = type;
			outer_arg->rm = rm;
			outer_arg->timer_queue = &timerQueueMap[type];
			CreateTimerQueueTimer(&outer_arg->timer_handle, timerQueueMap[type].queue_handle, WindowsTimerQueueHandler, (void *)outer_arg, micros / 1000, 0, 0);
			timerQueueMap[type].arg_list.push_front(outer_arg);
		}

		return true;
	}
	bool cancelEvent(int type, MediaIndependentPort *port) {
		TimerQueueMap_t::iterator iter = timerQueueMap.find(type);
		if (iter == timerQueueMap.end()) return false;
		while (!timerQueueMap[type].arg_list.empty()) {
			WindowsTimerQueueHandlerArg *del_arg = timerQueueMap[type].arg_list.front();
			timerQueueMap[type].arg_list.pop_front();
			DeleteTimerQueueTimer(del_arg->queue_handle, del_arg->timer_handle, INVALID_HANDLE_VALUE);
			if (del_arg->rm) delete del_arg->inner_arg;
			delete del_arg;
		}

		return true;
	}
	bool stop() {
		// Stop all of the timers
		TimerQueueMap_t::iterator iter;

		lock();
		stopped = true;
		for (iter = timerQueueMap.begin(); iter != timerQueueMap.end(); ++iter) {
			DeleteTimerQueueEx(iter->second.queue_handle, INVALID_HANDLE_VALUE);
		}
		unlock();

		// Cleanup retired timers
		cleanupRetiredTimers(true);
		return true;
	}
	virtual bool stop_port(MediaIndependentPort *port) {
		// Stop all of the timers associated with this port
		TimerQueueMap_t::iterator iter;

		lock();
		for (iter = timerQueueMap.begin(); iter != timerQueueMap.end(); ++iter) {
			TimerArgList_t::iterator iter_in;
			for (iter_in = iter->second.arg_list.begin(); iter_in != iter->second.arg_list.end(); ++iter_in) {
				if ((*iter_in)->inner_arg->port == port) {
					WindowsTimerQueueHandlerArg *tmp = *iter_in;
					iter->second.arg_list.erase(iter_in++);
					DeleteTimerQueueTimer(tmp->queue_handle, tmp->timer_handle, INVALID_HANDLE_VALUE);
					if (tmp->rm) delete tmp->inner_arg;
					delete tmp;
				}
			}
		}
		unlock();

		return true;
	}
};

class WindowsTimerQueueFactory : public OSTimerQueueFactory {
public:
	virtual OSTimerQueue *createOSTimerQueue(OSLockFactory *lock_factory) {
		WindowsTimerQueue *timerq = new WindowsTimerQueue();
		if (!OSTimerQueueFactory::createOSTimerQueue(lock_factory, timerq)) {
			return NULL;
		}

		return timerq;
	};
};

class WindowsTimer : public OSTimer {
	friend class WindowsTimerFactory;
public:
	virtual unsigned long sleep(unsigned long micros) {
		Sleep(micros / 1000);
		return micros;
	}
protected:
	WindowsTimer() {};
};

class WindowsTimerFactory : public OSTimerFactory {
public:
	virtual OSTimer *createTimer() {
		return new WindowsTimer();
	}
};

struct OSThreadArg {
	OSThreadFunction func;
	void *arg;
	OSThreadExitCode ret;
};

DWORD WINAPI OSThreadCallback(LPVOID input);

class WindowsThread : public OSThread {
	friend class WindowsThreadFactory;
private:
	HANDLE thread_id;
	OSThreadArg *arg_inner;
public:
	virtual bool start(OSThreadFunction function, void *arg) {
		arg_inner = new OSThreadArg();
		arg_inner->func = function;
		arg_inner->arg = arg;
		thread_id = CreateThread(NULL, 0, OSThreadCallback, arg_inner, 0, NULL);
		if (thread_id == NULL) return false;
		else return true;
	}
	virtual bool join(OSThreadExitCode &exit_code) {
		if (WaitForSingleObject(thread_id, INFINITE) != WAIT_OBJECT_0) return false;
		exit_code = arg_inner->ret;
		delete arg_inner;
		return true;
	}
protected:
	WindowsThread() {};
};

class WindowsThreadFactory : public OSThreadFactory {
public:
	OSThread *createThread() {
		return new WindowsThread();
	}
};

class WindowsIPC {
	virtual bool setSystemClockFrequency(LARGE_INTEGER frequency) { return false; }
};

class WindowsNamedPipeIPC : public OS_IPC, public WindowsIPC {
private:
	HANDLE pipe;
	LockableOffset loffset;
	PeerList peerlist;
public:
	WindowsNamedPipeIPC() { };
	~WindowsNamedPipeIPC() {
		CloseHandle(pipe);
	}
	virtual bool init(OS_IPC_ARG *arg = NULL);
	virtual bool update(clock_offset_t *update);
	virtual bool setSystemClockFrequency(LARGE_INTEGER frequency);
};

#endif/*WINDOWS_HAL_COMMON_HPP*/
