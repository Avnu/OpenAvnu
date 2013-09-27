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

#ifndef AVBTS_MESSAGE_HPP
#define AVBTS_MESSAGE_HPP

#include <stdint.h>
#include <avbts_osnet.hpp>
#include <ieee1588.hpp>

#define PTP_CODE_STRING_LENGTH 4
#define PTP_SUBDOMAIN_NAME_LENGTH 16
#define PTP_FLAGS_LENGTH 2

#define GPTP_VERSION 2
#define PTP_NETWORK_VERSION 1

#define PTP_ETHER 1
#define PTP_DEFAULT 255

#define PTP_COMMON_HDR_OFFSET 0
#define PTP_COMMON_HDR_LENGTH 34
#define PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(x) x
#define PTP_COMMON_HDR_PTP_VERSION(x) x+1
#define PTP_COMMON_HDR_MSG_LENGTH(x) x+2
#define PTP_COMMON_HDR_DOMAIN_NUMBER(x) x+4
#define PTP_COMMON_HDR_FLAGS(x) x+6
#define PTP_COMMON_HDR_CORRECTION(x) x+8
#define PTP_COMMON_HDR_SOURCE_CLOCK_ID(x) x+20
#define PTP_COMMON_HDR_SOURCE_PORT_ID(x) x+28
#define PTP_COMMON_HDR_SEQUENCE_ID(x) x+30
#define PTP_COMMON_HDR_CONTROL(x) x+32
#define PTP_COMMON_HDR_LOG_MSG_INTRVL(x) x+33

#define PTP_ANNOUNCE_OFFSET 34
#define PTP_ANNOUNCE_LENGTH 30
#define PTP_ANNOUNCE_CURRENT_UTC_OFFSET(x) x+10
#define PTP_ANNOUNCE_GRANDMASTER_PRIORITY1(x) x+13
#define PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY(x) x+14
#define PTP_ANNOUNCE_GRANDMASTER_PRIORITY2(x) x+18
#define PTP_ANNOUNCE_GRANDMASTER_IDENTITY(x) x+19
#define PTP_ANNOUNCE_STEPS_REMOVED(x) x+27
#define PTP_ANNOUNCE_TIME_SOURCE(x) x+29

#define PTP_SYNC_OFFSET 34
#define PTP_SYNC_LENGTH 10
#define PTP_SYNC_SEC_MS(x) x
#define PTP_SYNC_SEC_LS(x) x+2
#define PTP_SYNC_NSEC(x) x+6

#define PTP_FOLLOWUP_OFFSET 34
#define PTP_FOLLOWUP_LENGTH 10
#define PTP_FOLLOWUP_SEC_MS(x) x
#define PTP_FOLLOWUP_SEC_LS(x) x+2
#define PTP_FOLLOWUP_NSEC(x) x+6

#define PTP_PDELAY_REQ_OFFSET 34
#define PTP_PDELAY_REQ_LENGTH 20
#define PTP_PDELAY_REQ_SEC_MS(x) x
#define PTP_PDELAY_REQ_SEC_LS(x) x+2
#define PTP_PDELAY_REQ_NSEC(x) x+6

#define PTP_PDELAY_RESP_OFFSET 34
#define PTP_PDELAY_RESP_LENGTH 20
#define PTP_PDELAY_RESP_SEC_MS(x) x
#define PTP_PDELAY_RESP_SEC_LS(x) x+2
#define PTP_PDELAY_RESP_NSEC(x) x+6
#define PTP_PDELAY_RESP_REQ_CLOCK_ID(x) x+10
#define PTP_PDELAY_RESP_REQ_PORT_ID(x) x+18

#define PTP_PDELAY_FOLLOWUP_OFFSET 34
#define PTP_PDELAY_FOLLOWUP_LENGTH 20
#define PTP_PDELAY_FOLLOWUP_SEC_MS(x) x
#define PTP_PDELAY_FOLLOWUP_SEC_LS(x) x+2
#define PTP_PDELAY_FOLLOWUP_NSEC(x) x+6
#define PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID(x) x+10
#define PTP_PDELAY_FOLLOWUP_REQ_PORT_ID(x) x+18

#define PTP_LI_61_BYTE 0
#define PTP_LI_61_BIT 0
#define PTP_LI_59_BYTE 0
#define PTP_LI_59_BIT 1
#define PTP_ASSIST_BYTE 0
#define PTP_ASSIST_BIT 1

enum MessageType {
	SYNC_MESSAGE = 0,
	DELAY_REQ_MESSAGE = 1,
	PATH_DELAY_REQ_MESSAGE = 2,
	PATH_DELAY_RESP_MESSAGE = 3,
	FOLLOWUP_MESSAGE = 8,
	DELAY_RESP_MESSAGE = 9,
	PATH_DELAY_FOLLOWUP_MESSAGE = 0xA,
	ANNOUNCE_MESSAGE = 0xB,
	SIGNALLING_MESSAGE = 0xC,
	MANAGEMENT_MESSAGE = 0xD,
};

enum LegacyMessageType {
	SYNC,
	DELAY_REQ,
	FOLLOWUP,
	DELAY_RESP,
	MANAGEMENT,
	MESSAGE_OTHER
};

enum MulticastType {
	MCAST_NONE,
	MCAST_PDELAY,
	MCAST_OTHER
};

class PTPMessageCommon {
protected:
	unsigned char versionPTP;
	uint16_t versionNetwork;
	MessageType messageType;
	
	PortIdentity *sourcePortIdentity;
	
	uint16_t sequenceId;
	LegacyMessageType control;
	unsigned char flags[2];
	
	uint16_t messageLength;
	char logMeanMessageInterval;
	long long correctionField;
	unsigned char domainNumber;
	
	Timestamp _timestamp;
	unsigned _timestamp_counter_value;
	bool _gc;
	
	PTPMessageCommon(void) { };
 public:
	PTPMessageCommon(IEEE1588Port * port);
	virtual ~PTPMessageCommon(void);

	unsigned char *getFlags(void) {
		return flags;
	}

	uint16_t getSequenceId(void) {
		return sequenceId;
	}
	void setSequenceId(uint16_t seq) {
		sequenceId = seq;
	}

	MessageType getMessageType(void) {
		return messageType;
	}

	long long getCorrectionField(void) {
		return correctionField;
	}
	void setCorrectionField(long long correctionAmount) {
		correctionField = correctionAmount;
	}

	void getPortIdentity(PortIdentity * identity);
	void setPortIdentity(PortIdentity * identity);

	Timestamp getTimestamp(void) {
		return _timestamp;
	}
	uint32_t getTimestampCounterValue(void) {
		return _timestamp_counter_value;;
	}
	void setTimestamp(Timestamp & timestamp) {
		_timestamp = timestamp;
	}

	bool garbage() {
		return _gc;
	}

	bool isSenderEqual(PortIdentity portIdentity);

	virtual void processMessage(IEEE1588Port * port);

	void buildCommonHeader(uint8_t * buf);

	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

#pragma pack(push,1)

class PathTraceTLV {
 private:
	uint16_t tlvType;
	uint16_t lengthField;
	ClockIdentity identity;
 public:
	 PathTraceTLV() {
		tlvType = PLAT_htons(0x8);
		lengthField = PLAT_htons(sizeof(identity));
	}
	void setClockIdentity(ClockIdentity * id) {
		identity = *id;
	}
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}
};

#pragma pack(pop)

class PTPMessageAnnounce:public PTPMessageCommon {
 private:
	uint8_t grandmasterIdentity[PTP_CLOCK_IDENTITY_LENGTH];
	ClockQuality *grandmasterClockQuality;

	PathTraceTLV tlv;

	uint16_t currentUtcOffset;
	unsigned char grandmasterPriority1;
	unsigned char grandmasterPriority2;
	ClockQuality *clockQuality;
	uint16_t stepsRemoved;
	unsigned char timeSource;

	 PTPMessageAnnounce(void);
 public:
	 PTPMessageAnnounce(IEEE1588Port * port);
	~PTPMessageAnnounce();

	bool isBetterThan(PTPMessageAnnounce * msg);

	unsigned char getGrandmasterPriority1(void) {
		return grandmasterPriority1;
	}
	unsigned char getGrandmasterPriority2(void) {
		return grandmasterPriority2;
	}

	ClockQuality *getGrandmasterClockQuality(void) {
		return grandmasterClockQuality;
	}

	uint16_t getStepsRemoved(void) {
		return stepsRemoved;
	}

	void getGrandmasterIdentity(char *identity) {
		memcpy(identity, grandmasterIdentity, PTP_CLOCK_IDENTITY_LENGTH);
	}
	ClockIdentity getGrandmasterClockIdentity() {
		ClockIdentity ret;
		ret.set( grandmasterIdentity );
		return ret;
	}

	void processMessage(IEEE1588Port * port);

	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);

	friend PTPMessageCommon *buildPTPMessage(char *buf, int size,
						 LinkLayerAddress * remote,
						 IEEE1588Port * port);
};

class PTPMessageSync : public PTPMessageCommon {
 private:
	Timestamp originTimestamp;

	PTPMessageSync();
 public:
	PTPMessageSync(IEEE1588Port * port);
	~PTPMessageSync();
	void processMessage(IEEE1588Port * port);

	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}

	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);

	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

#pragma pack(push,1)

class scaledNs {
 private:
	int32_t ms;
	uint64_t ls;
 public:
	 scaledNs() {
		ms = 0;
		ls = 0;
	} void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}
};

class FollowUpTLV {
 private:
	uint16_t tlvType;
	uint16_t lengthField;
	uint8_t organizationId[3];
	uint8_t organizationSubType_ms;
	uint16_t organizationSubType_ls;
	int32_t cumulativeScaledRateOffset;
	uint16_t gmTimeBaseIndicator;
	scaledNs scaledLastGmPhaseChange;
	int32_t scaledLastGmFreqChange;
 public:
	FollowUpTLV() {
		tlvType = PLAT_htons(0x3);
		lengthField = PLAT_htons(28);
		organizationId[0] = '\x00';
		organizationId[1] = '\x80';
		organizationId[2] = '\xC2';
		organizationSubType_ms = 0;
		organizationSubType_ls = PLAT_htons(1);
		cumulativeScaledRateOffset = PLAT_htonl(0);
		gmTimeBaseIndicator = PLAT_htons(0);
		scaledLastGmFreqChange = PLAT_htonl(0);
	}
	void toByteString(uint8_t * byte_str) {
		memcpy(byte_str, this, sizeof(*this));
	}
	int32_t getRateOffset() {
		return cumulativeScaledRateOffset;
	}
};

#pragma pack(pop)

class PTPMessageFollowUp:public PTPMessageCommon {
private:
	Timestamp preciseOriginTimestamp;
	
	FollowUpTLV tlv;
	
	PTPMessageFollowUp(void) { }
public:
	PTPMessageFollowUp(IEEE1588Port * port);
	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);
	void processMessage(IEEE1588Port * port);
	
	Timestamp getPreciseOriginTimestamp(void) {
		return preciseOriginTimestamp;
	}
	void setPreciseOriginTimestamp(Timestamp & timestamp) {
		preciseOriginTimestamp = timestamp;
	}
	
	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

class PTPMessagePathDelayReq : public PTPMessageCommon {
 private:
	Timestamp originTimestamp;

	PTPMessagePathDelayReq() {
		return;
	}
 public:
	~PTPMessagePathDelayReq() {
	}
	PTPMessagePathDelayReq(IEEE1588Port * port);
	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);
	void processMessage(IEEE1588Port * port);
	
	Timestamp getOriginTimestamp(void) {
		return originTimestamp;
	}
	
	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

class PTPMessagePathDelayResp:public PTPMessageCommon {
private:
	PortIdentity * requestingPortIdentity;
	Timestamp requestReceiptTimestamp;
	
	PTPMessagePathDelayResp(void) {	
	}
public:
	~PTPMessagePathDelayResp();
	PTPMessagePathDelayResp(IEEE1588Port * port);
	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);
	void processMessage(IEEE1588Port * port);
	
	void setRequestReceiptTimestamp(Timestamp timestamp) {
		requestReceiptTimestamp = timestamp;
	}
	
	void setRequestingPortIdentity(PortIdentity * identity);
	void getRequestingPortIdentity(PortIdentity * identity);
	
	Timestamp getRequestReceiptTimestamp(void) {
		return requestReceiptTimestamp;
	}
	
	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

class PTPMessagePathDelayRespFollowUp:public PTPMessageCommon {
 private:
	Timestamp responseOriginTimestamp;
	PortIdentity *requestingPortIdentity;

	PTPMessagePathDelayRespFollowUp(void) { }
public:
	 PTPMessagePathDelayRespFollowUp(IEEE1588Port * port);
	~PTPMessagePathDelayRespFollowUp();
	void sendPort(IEEE1588Port * port, PortIdentity * destIdentity);
	void processMessage(IEEE1588Port * port);

	void setResponseOriginTimestamp(Timestamp timestamp) {
		responseOriginTimestamp = timestamp;
	}
	void setRequestingPortIdentity(PortIdentity * identity);

	Timestamp getResponseOriginTimestamp(void) {
		return responseOriginTimestamp;
	}
	PortIdentity *getRequestingPortIdentity(void) {
		return requestingPortIdentity;
	}

	friend PTPMessageCommon *buildPTPMessage
	(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port);
};

#endif
