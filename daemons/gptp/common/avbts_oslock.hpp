/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
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

#ifndef AVBTS_OSLOCK_HPP
#define AVBTS_OSLOCK_HPP

#include <debugout.hpp>
#include <string.h>
#include <platform.hpp>

typedef enum { oslock_recursive, oslock_nonrecursive } OSLockType;
typedef enum { oslock_ok, oslock_self, oslock_held, oslock_fail } OSLockResult;

class OSLock {
 public:
	virtual OSLockResult lock(const char *caller = NULL) = 0;
	virtual OSLockResult unlock(const char *caller = NULL) = 0;
	virtual OSLockResult trylock(const char *caller = NULL) = 0;
	virtual ~OSLock() = 0;
 protected:
	OSLock() {
		this->debug_lock = false;
		this->lock_name = NULL;
	}
	OSLock(const char *lock_name, bool debug_lock) {
		this->debug_lock = debug_lock;
		if (lock_name != NULL) {
			this->lock_name = new char[strlen(lock_name) + 1];
			PLAT_strncpy(this->lock_name, lock_name, strlen(lock_name));
		}
		else {
			this->lock_name = NULL;
		}
	}
private:
	bool debug_lock;
	char *lock_name;
	void debugOutput(const char *verb, const char *caller) {
		const char *lock_name = this->lock_name != NULL ?
			this->lock_name : "Unnamed Lock";
		caller = caller != NULL ? caller : "Unknown Caller";
		if (debug_lock)
			XPTPD_WDEBUG
				("Lock Name: %s %s Lock (%s)", 
				 lock_name, verb, caller);
	}
};

inline OSLock::~OSLock() {
	delete lock_name;
}

// This code needs work
// For now, these base functions should be called by OS specific
// implementations
inline OSLockResult OSLock::lock(const char *caller) {
	debugOutput("Acquire", caller);
	return oslock_fail;
}
inline OSLockResult OSLock::unlock(const char *caller) {
	debugOutput("Release", caller);
	return oslock_fail;
}
inline OSLockResult OSLock::trylock(const char *caller) {
	debugOutput("Acquire (Try)", caller);
	return oslock_fail;
}

class OSLockFactory {
public:
	// Return value of NULL indicates error condition
	virtual OSLock* createLock
	(OSLockType type, const char *lock_name = NULL,
	 bool debug_lock = false ) = 0;
	virtual ~OSLockFactory() = 0;
};

inline OSLockFactory::~OSLockFactory() {}

#endif
