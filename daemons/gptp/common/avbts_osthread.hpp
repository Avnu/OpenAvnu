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

#ifndef AVBTS_OSTHREAD_HPP
#define AVBTS_OSTHREAD_HPP

/**@file*/

/**
 * thread exit codes. Possible values are:
 * 	- osthread_ok;
 * 	- osthread_error;
 */
typedef enum { osthread_ok, osthread_error } OSThreadExitCode;

/**
 * Provides the OSThreadExitCode callback format
 */
typedef OSThreadExitCode(*OSThreadFunction) (void *);

/**
 * Provides a generic interface for threads
 */
class OSThread {
public:
	/**
	 * @brief  Starts a new thread
	 * @param  function Callback to be started on the thread
	 * @param  arg Function arguments
	 * @return Implementation dependent
	 */
	virtual bool start(OSThreadFunction function, void *arg) = 0;

	/**
	 * @brief  Joins the thread
	 * @param  exit_code OSThreadExitCode enumeration
	 * @return Implementation specific
	 */
	virtual bool join(OSThreadExitCode & exit_code) = 0;
	virtual ~OSThread() = 0;
};

inline OSThread::~OSThread() {}

/**
 * Provides factory design patter for OSThread class
 */
class OSThreadFactory {
public:
	/**
	 * @brief Creates a new thread
	 * @return Pointer to OSThread object
	 */
	virtual OSThread * createThread() = 0;

	/**
	 * Destroys the new thread
	 */
	virtual ~OSThreadFactory() = 0;
};

inline OSThreadFactory::~OSThreadFactory() {}


#endif
