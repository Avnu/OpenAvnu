/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

#ifndef AVBTS_PERSIST_HPP
#define AVBTS_PERSIST_HPP

#include <stdint.h>
#include <ptptypes.hpp>

/**@file*/


/**
 * @brief  Callback function to write persistent data.
 * @param bufPtr Buffer pointer where the restored persistent data shall be stored.
 * @param bufSize The size of the buffer.
 * @return True on success otherwise False
 */
typedef void (*gPTPPersistWriteCB_t)(char *bufPtr, uint32_t bufSize);



/**
 * @brief Generic interface for Simple Persistence for gPTP values
 */
class GPTPPersist {
public:
	/**
	 * @brief  Initializes the GPTP_PERSIST
	 * @param persistID string identifer of the persistence storage. For example a file name
	 * @return True on success otherwise False
	 */
	virtual bool initStorage(const char *persistID) = 0;

	/**
	 * @brief  Closes the GPTP_PERSIST instance
	 * @return True on success otherwise False
	 */
	virtual bool closeStorage(void) = 0;

	/**
	 * @brief  Read the persistent data
	 * @param bufPtr Buffer pointer where the restored persistent data is held.
	 * @param bufSize The size of the restored data.
	 * @return True on success otherwise False
	 */
	virtual bool readStorage(char **bufPtr, uint32_t *bufSize) = 0;

	/**
	 * @brief Register the write call back
	 * @param writeCB Write callback from
	 * @return none
	 */
	virtual void registerWriteCB(gPTPPersistWriteCB_t writeCB) = 0;

	/**
	 * @brief Set write data size
	 * @param dataSiuze The size of data that will be written
	 * @return none
	 */
	virtual void setWriteSize(uint32_t dataSize) = 0;

	/**
	 * @brief  Trigger the write callback.
	 * @return True on success otherwise False
	 */
	virtual bool triggerWriteStorage(void) = 0;

	/*
	 * Destroys the GPTP_PERSIST instance
	 */
	virtual ~GPTPPersist() = 0;
};

inline GPTPPersist::~GPTPPersist() {}

#endif  // AVBTS_PERSIST_HPP

