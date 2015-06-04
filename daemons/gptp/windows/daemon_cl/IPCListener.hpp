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

#ifndef IPCLISTENER_HPP
#define IPCLISTENER_HPP

#include <stdlib.h>
#include <ipcdef.hpp>
#include <PeerList.hpp>
#include <Stoppable.hpp>

/**@file */

/**
 * Provides an interface for offset with lock
 */
class LockableOffset : public Lockable, public Offset {
private:
	bool ready;
public:
	/**
	 * Default constructor. Initializes internal variables
	 */
	LockableOffset() {
		ml_phoffset = 0;
		ml_freqoffset = 0.0;
		ls_phoffset = 0;
		ls_freqoffset = 0.0;
		local_time = 0;
	}
	/**
	 * @brief  Get Internal ready flag
	 * @return TRUE if is ready. FALSE otherwise.
	 */
	bool isReady() { return ready; }
	/**
	 * @brief  Sets ready flag
	 * @param  ready Value to be set.
	 */
	void setReady( bool ready ) { this->ready = ready; }
};

/**
 * Provides an interface for the IPC shared data
 */
class IPCSharedData {
public:
	PeerList *list;		/*!< List of peers */
	LockableOffset *offset;	/*!< Ponter to LockableOffset class */
};

/**
 * Provides an interface for the IPC Listener
 */
class IPCListener : public Stoppable {
private:
	static DWORD WINAPI IPCListenerLoopWrap( LPVOID arg ) {
		DWORD ret;
		LPVOID *argl = (LPVOID *) arg;
		IPCListener *this0 = (IPCListener *) argl[0];
		ret = this0->IPCListenerLoop((IPCSharedData *) argl[1] );
		delete argl[1];
		delete [] argl;
		return ret;
	}
	DWORD WINAPI IPCListenerLoop( IPCSharedData *arg );
public:
	/**
	 * @brief  Starts the listener loop in a new thread
	 * @return TRUE in case of success, FALSE in case of error
	 */
	bool start( IPCSharedData data ) {
		LPVOID *arg = new LPVOID[2];
		if( thread != NULL ) {
			return false;
		}
		arg[1] = (void *) malloc((size_t) sizeof(IPCSharedData));
		arg[0] = this;
		*((IPCSharedData *) arg[1]) = data;
		thread = CreateThread( NULL, 0, &IPCListenerLoopWrap, arg, 0, NULL );
		if( thread != NULL ) return true;
		else return false;
	}
	/**
	 * Destroys the IPC listener interface
	 */
	~IPCListener() {}
};



#endif/*IPCListener_hpp*/
