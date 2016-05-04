/*************************************************************************************************************
Copyright (c) 2012-2016, Harman International Industries, Incorporated
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
*************************************************************************************************************/

#ifndef AVBAP_MESSAGE_HPP
#define AVBAP_MESSAGE_HPP

#include <stdint.h>
#include <avbts_osnet.hpp>
#include <ieee1588.hpp>

#include <list>
#include <algorithm>
#include <avbts_message.hpp>

/** @file **/

#define AP_TEST_STATUS_OFFSET 0									/*!< AP Test Status offset */
#define AP_TEST_STATUS_LENGTH 160								/*!< AP Test Status length in byte */
#define AP_TEST_STATUS_AVTP_SUBTYPE(x) x						/*!< AVTP Subtype */
#define AP_TEST_STATUS_AVTP_VERSION_CONTROL(x) x + 1			/*!< Version and control fields */
#define AP_TEST_STATUS_AVTP_STATUS_LENGTH(x) x + 2				/*!< Status and content length */
#define AP_TEST_STATUS_TARGET_ENTITY_ID(x) x + 4				/*!< Target entity ID */
#define AP_TEST_STATUS_CONTROLLER_ENTITY_ID(x) x + 12			/*!< Controller entity ID */
#define AP_TEST_STATUS_SEQUENCE_ID(x) x + 20					/*!< Sequence ID */
#define AP_TEST_STATUS_COMMAND_TYPE(x) x + 22					/*!< Command type */
#define AP_TEST_STATUS_DESCRIPTOR_TYPE(x) x + 24				/*!< Descriptor Type */
#define AP_TEST_STATUS_DESCRIPTOR_INDEX(x) x + 26				/*!< Descriptor Index */
#define AP_TEST_STATUS_COUNTERS_VALID(x) x + 28					/*!< Counters valid */
#define AP_TEST_STATUS_COUNTER_LINKUP(x) x + 32					/*!< Counter Link Up */
#define AP_TEST_STATUS_COUNTER_LINKDOWN(x) x + 36				/*!< Counter Link Down */
#define AP_TEST_STATUS_COUNTER_FRAMES_TX(x) x + 40				/*!< Counter Frames TX */
#define AP_TEST_STATUS_COUNTER_FRAMES_RX(x) x + 44				/*!< Counter Frames RX */
#define AP_TEST_STATUS_COUNTER_FRAMES_RX_CRC_ERROR(x) x + 48	/*!< Counter Frames RX CRC Error */
#define AP_TEST_STATUS_COUNTER_GM_CHANGED(x) x + 52				/*!< Counter GM Changed */
#define AP_TEST_STATUS_COUNTER_RESERVED(x) x + 56				/*!< Reserved counters */
#define AP_TEST_STATUS_MESSAGE_TIMESTAMP(x) x + 128				/*!< Message timestamp */
#define AP_TEST_STATUS_STATION_STATE(x) x + 136					/*!< Station state */
#define AP_TEST_STATUS_STATION_STATE_SPECIFIC_DATA(x) x + 137	/*!< Station state specific data */
#define AP_TEST_STATUS_COUNTER_27(x) x + 140					/*!< Counter 27 */
#define AP_TEST_STATUS_COUNTER_28(x) x + 144					/*!< Counter 28 */
#define AP_TEST_STATUS_COUNTER_29(x) x + 148					/*!< Counter 29 */
#define AP_TEST_STATUS_COUNTER_30(x) x + 152					/*!< Counter 30 */
#define AP_TEST_STATUS_COUNTER_31(x) x + 156					/*!< Counter 31 */


/**
 * Automotive Profile Test Status Station State
 */
typedef enum {
	STATION_STATE_RESERVED,
	STATION_STATE_ETHERNET_READY,
	STATION_STATE_AVB_SYNC,
	STATION_STATE_AVB_MEDIA_READY,
} StationState_t;


/**
 * Provides a class for building the ANvu Automotive
 * Profile Test Status message
 */
class APMessageTestStatus {
 private:
	uint16_t messageLength;			/*!< message length */

	APMessageTestStatus();
 public:
	/**
	 * @brief Default constructor. Creates APMessageTestStatus
	 * @param port IEEE1588Port
	 */
	APMessageTestStatus(IEEE1588Port * port);

	/**
	 * Destroys APMessageTestStatus interface
	 */
	~APMessageTestStatus();

	/**
	 * @brief  Assembles APMessageTestStatus message on the
	 *  	   IEEE1588Port payload
	 * @param  port IEEE1588Port where the message will be assembled
	 * @return void
	 */
	void sendPort(IEEE1588Port * port);
};


#endif
