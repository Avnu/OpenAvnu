/******************************************************************************

Copyright (c) 2009-2015, Intel Corporation
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

#ifndef WIRELESS_TSTAMPER_HPP
#define WIRELESS_TSTAMPER_HPP

#include <common_tstamper.hpp>
#include <wireless_port.hpp>

#define MAX_DIALOG_TOKEN 255
#define OUI_8021AS_OCTETS { 0x00, 0x80, 0xC2 }
static const uint8_t OUI_8021AS[] = OUI_8021AS_OCTETS;
#define FWUP_TYPE 0
#define FWUP_VDEF_TAG 0xDD /* Vendor Defined Tag */

typedef enum _WIRELESS_EVENT_TYPE
{
	TIMINGMSMT_EVENT = 0,
	TIMINGMSMT_CONFIRM_EVENT,
	TIMINGMSMT_CORRELATEDTIME_EVENT,
} WIRELESS_EVENT_TYPE;

#pragma pack(push, 1)
typedef struct
{
	uint8_t oui[sizeof(OUI_8021AS)];
	uint8_t type;
} FwUpLabel_t;

typedef struct _PTP_SPEC
{
	FwUpLabel_t	FwUpLabel;
	uint8_t		fwup_data[1];
} PTP_SPEC;

static const FwUpLabel_t FwUpLabel = { OUI_8021AS_OCTETS, FWUP_TYPE };

typedef struct _WIFI_VENDOR_SPEC_HDR
{
	uint8_t		ElementId;
	uint8_t		Length;
} WIFI_VENDOR_SPEC_HDR;
#pragma pack(pop)

typedef struct _TIMINGMSMT_REQUEST
{
	uint8_t		PeerMACAddress[ETHER_ADDR_OCTETS];
	uint8_t		Category;
	uint8_t		Action;
	uint8_t		DialogToken;
	uint8_t		FollowUpDialogToken;
	uint32_t	T1;
	uint32_t	T4;
	uint8_t		MaxT1Error;
	uint8_t		MaxT4Error;

	WIFI_VENDOR_SPEC_HDR	WiFiVSpecHdr;
	PTP_SPEC				PtpSpec;
} TIMINGMSMT_REQUEST;

typedef struct _TIMINGMSMT_EVENT_DATA
{
	uint8_t		PeerMACAddress[ETHER_ADDR_OCTETS];
	uint32_t	DialogToken;
	uint32_t	FollowUpDialogToken;
	uint64_t	T1;
	uint32_t	MaxT1Error;
	uint64_t	T4;
	uint32_t	MaxT4Error;
	uint64_t	T2;
	uint32_t	MaxT2Error;
	uint64_t	T3;
	uint32_t	MaxT3Error;

	WIFI_VENDOR_SPEC_HDR	WiFiVSpecHdr;
	PTP_SPEC				PtpSpec;
} TIMINGMSMT_EVENT_DATA;

typedef struct _TIMINGMSMT_CONFIRM_EVENT_DATA
{
	uint8_t		PeerMACAddress[ETHER_ADDR_OCTETS];
	uint32_t	DialogToken;
	uint64_t	T1;
	uint32_t	MaxT1Error;
	uint64_t	T4;
	uint32_t	MaxT4Error;
} TIMINGMSMT_CONFIRM_EVENT_DATA;

typedef struct _WIRELESS_CORRELATEDTIME
{
	uint64_t	TSC;
	uint64_t	LocalClk;
} WIRELESS_CORRELATEDTIME;

struct S8021AS_Indication {
	TIMINGMSMT_EVENT_DATA indication;
	uint8_t followup[75]; // (34 header + 10 followup + 32 TLV) - 1 byte contained in indication struct = 75
};

union TimeSyncEventData
{
	TIMINGMSMT_CONFIRM_EVENT_DATA confirm;
	S8021AS_Indication indication;
	WIRELESS_CORRELATEDTIME ptm_wa;
};

class WirelessTimestamper : public CommonTimestamper
{
private:
	WirelessPort *port;
public:
	virtual ~WirelessTimestamper() {}

	/**
	 * @brief attach timestamper to port
	 * @param port port to attach
	 */
	void setPort( WirelessPort *port )
	{
		this->port = port;
	}

	/**
	 * @brief return reference to attached port
	 * @return reference to attached port
	 */
	WirelessPort *getPort(void) const
	{
		return port;
	}

	/**
	 * @brief Return buffer offset where followup message should be placed
	 * @return byte offset
	 */
	uint8_t getFwUpOffset(void)
	{
		// Subtract 1 to compensate for 'bogus' vendor specific buffer
		// length
		return (uint8_t)((size_t) &((TIMINGMSMT_REQUEST *) 0)->
				 PtpSpec.fwup_data );
	}

	/**
	 * @brief Request transmission of TM frame
	 * @param dest [in] MAC destination the frame should be sent to
	 * @param seq [in] 802.1AS sequence number
	 * @param prev_dialog [in] last dialog message
	 * @param follow_up [in] buffer containing followup message
	 * @param followup_length [in] fw-up message length in bytes
	 */
	virtual net_result requestTimingMeasurement
	( LinkLayerAddress *dest, uint16_t seq, WirelessDialog *prev_dialog,
	  uint8_t *follow_up, int followup_length );

	/**
	 * @brief abstract method for driver/os specific TM transmit code
	 * @param timingmsmt_req fully formed TM message
	 */
	virtual net_result _requestTimingMeasurement
	( TIMINGMSMT_REQUEST *timingmsmt_req ) = 0;

	/**
	 * @brief Asynchronous completion of TM transmit
	 * @param addr [in] MAC the message was transmitted to
	 * @param dialog [in] dialog filled with T1, T4 timestamps
	 */
	void timingMeasurementConfirmCB( LinkLayerAddress addr,
					 WirelessDialog *dialog );

	/**
	 * @brief Reception of TM frame
	 * @param addr [in] MAC the message was received from
	 * @param current [in] dialog filled with T2, T3 timestamps
	 * @param previous [in] dialog filled with T1, T4 timestamps
	 * @param buf [in] buffer containing followup message
	 * @param previous [in] length of followup message
	 */
	void timeMeasurementIndicationCB
	( LinkLayerAddress addr, WirelessDialog *current,
	  WirelessDialog *previous, uint8_t *buf, size_t buflen );
};

struct WirelessTimestamperCallbackArg
{
	WIRELESS_EVENT_TYPE iEvent_type;
	WirelessTimestamper *timestamper;
	TimeSyncEventData event_data;
};

#endif/*WIRELESS_TSTAMPER_HPP*/
