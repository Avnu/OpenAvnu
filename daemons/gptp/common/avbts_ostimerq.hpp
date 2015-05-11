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

#ifndef AVBTS_OSTIMERQ_HPP
#define AVBTS_OSTIMERQ_HPP

typedef void (*ostimerq_handler) (void *);

class IEEE1588Clock;

/*
 * OSTimerQueue generic interface
 */
class OSTimerQueue {
protected:
	/**
	 * @brief Initializes timer queue
	 * @return TRUE
	 */
	virtual bool init() { return true; }

	/*
	 * Default constructor
	 */
	OSTimerQueue() {}
public:
	/**
	 * @brief Add an event to the timer queue
	 * @param micros Time in microsseconds
	 * @param type  Event type
	 * @param func Callback
	 * @param arg inner argument of type event_descriptor_t
	 * @param dynamic Flag to be used internally (for instance rm)
	 * @param event [inout] Pointer to the event
	 * @return TRUE success, FALSE fail
	 */
	virtual bool addEvent
	(unsigned long micros, int type, ostimerq_handler func,
	 event_descriptor_t * arg, bool dynamic, unsigned *event) = 0;

	/**
	 * @brief Removes an event from the timer queue
	 * @param type Event type
	 * @param event [inout] Pointer to the event
	 * @return TRUE success, FALSE fail
	 */
	virtual bool cancelEvent(int type, unsigned *event) = 0;
	virtual ~OSTimerQueue() = 0;
};

inline OSTimerQueue::~OSTimerQueue() {}

/**
 * Implements factory design patter for OSTimerQueue class
 */
class OSTimerQueueFactory {
public:
	/**
	 * @brief Creates the OSTimerQueue object
	 * @param clock [in] Pointer to the IEEE1555Clock object
	 * @return Pointer to OSTimerQueue 
	 */
	virtual OSTimerQueue *createOSTimerQueue( IEEE1588Clock *clock ) = 0;
	virtual ~OSTimerQueueFactory() = 0;
};

inline OSTimerQueueFactory::~OSTimerQueueFactory() {}

#endif
