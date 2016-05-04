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

#include <ieee1588.hpp>
#include <avbts_clock.hpp>
#include <avbap_message.hpp>
#include <avbts_port.hpp>
#include <avbts_ostimer.hpp>
#include <gptp_log.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>


#define ntohll(x)    __bswap_64 (x)
#define htonll(x)    __bswap_64 (x)


// Octet based data 2 buffer macros
#define OCT_D2BMEMCP(d, s) memcpy(d, s, sizeof(s)); d += sizeof(s);
#define OCT_D2BBUFCP(d, s, len) memcpy(d, s, len); d += len;
#define OCT_D2BHTONB(d, s) *(U8 *)(d) = s; d += sizeof(s);
#define OCT_D2BHTONS(d, s) *(U16 *)(d) = PLAT_htons(s); d += sizeof(s);
#define OCT_D2BHTONL(d, s) *(U32 *)(d) = PLAT_htonl(s); d += sizeof(s);

// Bit based data 2 buffer macros
#define BIT_D2BHTONB(d, s, shf) *(uint8_t *)(d) |= s << shf;
#define BIT_D2BHTONS(d, s, shf) *(uint16_t *)(d) |= PLAT_htons((uint16_t)(s << shf));
#define BIT_D2BHTONL(d, s, shf) *(uint32_t *)(d) |= PLAT_htonl((uint32_t)(s << shf));


APMessageTestStatus::APMessageTestStatus()
{
}

APMessageTestStatus::~APMessageTestStatus()
{
}

APMessageTestStatus::APMessageTestStatus(IEEE1588Port * port)
{
}

void APMessageTestStatus::sendPort(IEEE1588Port * port)
{
	static uint16_t sequenceId = 0;

	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	uint16_t tmp16;
	uint32_t tmp32;
	uint64_t tmp64;

	memset(buf_t, 0, 256);

	// Create packet in buf
	messageLength = AP_TEST_STATUS_LENGTH;

	BIT_D2BHTONB(buf_ptr + AP_TEST_STATUS_AVTP_SUBTYPE(AP_TEST_STATUS_OFFSET), 0xfb, 0);
	BIT_D2BHTONB(buf_ptr + AP_TEST_STATUS_AVTP_VERSION_CONTROL(AP_TEST_STATUS_OFFSET), 0x1, 0);
	BIT_D2BHTONS(buf_ptr + AP_TEST_STATUS_AVTP_STATUS_LENGTH(AP_TEST_STATUS_OFFSET), 148, 0);

	port->getLocalAddr()->toOctetArray(buf_ptr + AP_TEST_STATUS_TARGET_ENTITY_ID(AP_TEST_STATUS_OFFSET));

	tmp16 = PLAT_htons(sequenceId++);
	memcpy(buf_ptr + AP_TEST_STATUS_SEQUENCE_ID(AP_TEST_STATUS_OFFSET), &tmp16, sizeof(tmp16));

	BIT_D2BHTONS(buf_ptr + AP_TEST_STATUS_COMMAND_TYPE(AP_TEST_STATUS_OFFSET), 1, 15);
	BIT_D2BHTONS(buf_ptr + AP_TEST_STATUS_COMMAND_TYPE(AP_TEST_STATUS_OFFSET), 0x29, 0);

	tmp16 = PLAT_htons(0x09);
	memcpy(buf_ptr + AP_TEST_STATUS_DESCRIPTOR_TYPE(AP_TEST_STATUS_OFFSET), &tmp16, sizeof(tmp16));
	tmp16 = PLAT_htons(0x00);
	memcpy(buf_ptr + AP_TEST_STATUS_DESCRIPTOR_INDEX(AP_TEST_STATUS_OFFSET), &tmp16, sizeof(tmp16));

	tmp32 = PLAT_htonl(0x07000023);
	memcpy(buf_ptr + AP_TEST_STATUS_COUNTERS_VALID(AP_TEST_STATUS_OFFSET), &tmp32, sizeof(tmp32));

	tmp32 = PLAT_htonl(port->getLinkUpCount());
	memcpy(buf_ptr + AP_TEST_STATUS_COUNTER_LINKUP(AP_TEST_STATUS_OFFSET), &tmp32, sizeof(tmp32));

	tmp32 = PLAT_htonl(port->getLinkDownCount());
	memcpy(buf_ptr + AP_TEST_STATUS_COUNTER_LINKDOWN(AP_TEST_STATUS_OFFSET), &tmp32, sizeof(tmp32));

	Timestamp system_time;
	Timestamp device_time;
	uint32_t local_clock, nominal_clock_rate;
	port->getDeviceTime(system_time, device_time, local_clock, nominal_clock_rate);
	tmp64 = htonll(TIMESTAMP_TO_NS(system_time));
	memcpy(buf_ptr + AP_TEST_STATUS_MESSAGE_TIMESTAMP(AP_TEST_STATUS_OFFSET), &tmp64, sizeof(tmp64));

	BIT_D2BHTONB(buf_ptr + AP_TEST_STATUS_STATION_STATE(AP_TEST_STATUS_OFFSET), (uint8_t)port->getStationState(), 0);

	if (port->getStationState() == STATION_STATE_ETHERNET_READY) {
		GPTP_LOG_INFO("AVnu AP Status : STATION_STATE_ETHERNET_READY");
	}
	else if (port->getStationState() == STATION_STATE_AVB_SYNC) {
		GPTP_LOG_INFO("AVnu AP Status : STATION_STATE_AVB_SYNC");
	}
	else if (port->getStationState() == STATION_STATE_AVB_MEDIA_READY) {
		GPTP_LOG_INFO("AVnu AP Status : STATION_STATE_AVB_MEDIA_READY");
	}

	port->sendGeneralPort(AVTP_ETHERTYPE, buf_t, messageLength, MCAST_TEST_STATUS, NULL);

	return;
}

