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

/**@file*/

/**
 * @brief Lock type enumeration. The possible values are:
 * 	- oslock_recursive;
 * 	- oslock_nonrecursive;
 */
typedef enum { oslock_recursive, oslock_nonrecursive } OSLockType;
/**
 * @brief Lock result enumeration. The possible values are:
 * 	- oslock_ok;
 * 	- oslock_self;
 * 	- oslock_held;
 * 	- oslock_fail;
 */
typedef enum { oslock_ok, oslock_self, oslock_held, oslock_fail } OSLockResult;

/**
 * @brief Provides a generic mechanism for locking critical sections.
 */
class OSLock {
	public:
		/**
		 * @brief  Locks a critical section
		 * @return OSLockResult enumeration
		 */
		virtual OSLockResult lock() = 0;

		/**
		 * @brief  Unlocks a critical section
		 * @return OSLockResult enumeration
		 */
		virtual OSLockResult unlock() = 0;

		/**
		 * @brief  Tries locking a critical section
		 * @return OSLockResult enumeration
		 */
		virtual OSLockResult trylock() = 0;
	protected:
		/**
		 * @brief Default constructor
		 */
		OSLock() { }

		/**
		 * @brief  Initializes locking mechanism
		 * @param  type Enumeration OSLockType
		 * @return FALSE
		 */
		bool initialize(OSLockType type) {
			return false;
		}
		virtual ~OSLock() = 0;
};

inline OSLock::~OSLock() {}

/**
 * @brief Provides a factory pattern for OSLock
 */
class OSLockFactory {
	public:

		/**
		 * @brief  Creates locking mechanism
		 * @param  type Enumeration OSLockType
		 * @return Pointer to an enumeration of type OSLock
		 */
		virtual OSLock * createLock(OSLockType type) = 0;
		virtual ~OSLockFactory() = 0;
};

inline OSLockFactory::~OSLockFactory () {}

#endif
