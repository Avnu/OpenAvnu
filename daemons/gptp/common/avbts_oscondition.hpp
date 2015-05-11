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

#ifndef AVBTS_OSSIGNAL_HPP
#define AVBTS_OSSIGNAL_HPP

/**
 * Provides a generic interface for OS's locking condition
 */
class OSCondition {
private:
	int wait_count;
public:
	/**
	 * @brief Waits until a condition is met
	 * @return TRUE after waiting
	 */
	virtual bool wait() = 0;

	/**
	 * @brief Waits for lock
	 * @return TRUE after waiting
	 */
	virtual bool wait_prelock() = 0;

	/**
	 * @brief Sends a signal to unblock other threads
	 * @return TRUE
	 */
	virtual bool signal() = 0;

	/**
	 * Deletes previously declared flags
	 */
	virtual ~OSCondition() = 0;
protected:
	/**
	 * Default constructor. Initializes internal variables
	 */
	OSCondition() {
		wait_count = 0;
	};

	/**
	 * @brief  Counts up waiting condition
	 * @return void
	 */
	void up() {
		++wait_count;
	}

	/**
	 * @brief  Conds down waiting condition
	 * @return void
	 */
	void down() {
		--wait_count;
	}

	/**
	 * @brief  Checks if OS is waiting
	 * @return TRUE if up counter is greater than zero. FALSE otherwise.
	 */
	bool waiting() {
		return wait_count > 0;
	}
};

inline OSCondition::~OSCondition() { }

/*
 * Provides factory design patter for OS Condition class
 */
class OSConditionFactory {
public:
	/**
	 * @brief  Creates OSCondition class
	 * @return Pointer to OSCondition object
	 */
	virtual OSCondition * createCondition() = 0;

	/*
	 * Destroys OSCondition objects
	 */
	virtual ~OSConditionFactory() = 0;
};

inline OSConditionFactory::~OSConditionFactory() {}


#endif
