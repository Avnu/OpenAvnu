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

/*
* HEADER SUMMARY :
*
* Implementation of IEEE 802.1Q
* Multiple Stream Reservation Protocol
* (limited intial implementation for end stations)
* 
* This file declares the "Public" API.
* 
* THIS IMPLEMENTATION ASSUMES THAT THERE WILL BE NO MORE THAN ONE
* LISTENER THREAD FOR THE SAME STREAM ON A SINGLE END STATION.
* (More than one talker for the same stream makes no sense at all.)
*/

#ifndef OPENAVB_SRP_API_H
#define OPENAVB_SRP_API_H

#include "openavb_types.h"

// TBD - If queue manager can fail to configure a queue for a granted reservation, SRP
//       needs to know about the failure in order to correct reseved bandwidth totals.
//       (we are probably toast if queue manager fails when removing a stream)

// TBD - This implementation handles bandwidth in kilobits per second
//       the specs want bits per second (kbps will waste some bandwidth)

// MSRP Declaration Type - IEEE 802.1Q-2011 Table 35-1
typedef enum openavbSrpAttribType {
	openavbSrp_AtTyp_None            = 0x00, // not per spec - SRP internal use only
	openavbSrp_AtTyp_TalkerAdvertise = 0x01,
	openavbSrp_AtTyp_TalkerFailed    = 0x02,
	openavbSrp_AtTyp_Listener        = 0x03,
	openavbSrp_AtTyp_Domain          = 0x04,
} openavbSrpAttribType_t;

// MSRP Listener Declaration Subtype (aka "FourPacked Event") - IEEE 802.1Q-2011 Table 35-3
typedef enum openavbSrpLsnrDeclSubtype{
	openavbSrp_LDSt_None          = 0x00, // not per spec - SRP internal use (same as ignore is OK)
	openavbSrp_LDSt_Ignore        = 0x00,
	openavbSrp_LDSt_Asking_Failed = 0x01,
	openavbSrp_LDSt_Ready         = 0x02,
	openavbSrp_LDSt_Ready_Failed  = 0x03,
	openavbSrp_LDSt_Stream_Info   = 0xFE, // NOT per IEEE spec - for use to inform talker when MAAP allocated.	
	openavbSrp_LDSt_Interest      = 0xFF, // NOT per IEEE spec - for srpAttachStream() only
} openavbSrpLsnrDeclSubtype_t;

// Stream Reservation Failure Codes - IEEE 802.1Q-2011 Table 35-6
enum openavbSrpFailureCode {
	openavbSrp_FC_NoFail =  0, // 0: No failure,
	// 1: Insufficient bandwidth,
	// 2: Insufficient Bridge resources,
	openavbSrp_FC_NoClassBandwidth  =  3, // Insufficient bandwidth for Traffic Class,
	// 4: StreamID in use by another Talker,
	// 5: Stream destination address already in use,
	// 6: Stream pre-empted by higher rank,
	// 7: Reported latency has changed,
	openavbSrp_FC_NotCapable  =  8, // Egress port is not AVBCapable,
	// 9: Use a different destination_address,
	// 10: Out of MSRP resources,
	// 11: Out of MMRP resources,
	// 12: Cannot store destination_address,
	// 13: Requested priority is not an SR Class priority,
	// 14: MaxFrameSize is too large for media,
	// 15: maxFanInPorts limit has been reached,
	// 16: Changes in FirstValue for a registered StreamID,
	// 17: VLAN is blocked on this egress port (Registration Forbidden),
	// 18: VLAN tagging is disabled on this egress port (untagged set),
	// 19: SR class priority mismatch.
};

typedef struct openavbSrpFailInfo{ // IEEE 802.1Q-2011 Section 35.2.2.8.7
	U8  BridgeID[8];
	U8  FailureCode; // openavbSrpFailureCode
} openavbSrpFailInfo_t;


// AVTP provided callbacks passed into openavbSrpInitialize:
//
// Callback for SRP to notify AVTP Talker that a Listener Declaration has been
// registered (or de-registered)
// - avtpHandle is provided to SRP by AVTP via openavbSrpRegisterStream()
//   and is passed back to AVTP in this call;
// - [listener declaration] subtype is from the listener declaration.
typedef openavbRC (*strmAttachCb_t) (void* avtpHandle,
                                    openavbSrpLsnrDeclSubtype_t subtype);
// Callback for SRP to notify AVTP Listener that a Talker Declaration has been
// registered (or de-registered)
// - avtpHandle is provided to SRP by AVTP via openavbSrpAttachStream()
//   and is passed back to AVTP in this call;
// - declType, DA, tSpec and latency are from the Talker Declaration;
// - SRClassId is derived from the priority attribute of the Talker
//   Declaration;
// - pFailInfo is valid for only if declType is openavbSrp_AtTyp_TalkerFailed.
typedef openavbRC (*strmRegCb_t)    (void* avtpHandle,
                                    openavbSrpAttribType_t declType,
                                    U8 DA[],
                                    AVBTSpec_t* tSpec,
                                    SRClassIdx_t SRClassIdx,
                                    U32 latency,
                                    openavbSrpFailInfo_t* pFailInfo);

// SRP API

// Called by AVTP on talker or listener end station to start SRP.
// TxRateKbps is the maximum rate which the interface indicated by ifname
// can transmit, in kilobits per second.  Set bypassAsCapableCheck true
// to ignore the IEEE802.1ba section 6.4 requirement the SRP create
// streams only on [IEEE802.1AS / gPTP] asCapable links.
openavbRC openavbSrpInitialize       (strmAttachCb_t attachCb, strmRegCb_t registerCb,
                                 char* ifname, U32 TxRateKbps, bool bypassAsCapableCheck);

// Called by AVTP on talker or listener end station to stop SRP
void     openavbSrpShutdown         (void);

// Called by AVTP on talker end station to advertise the indicated stream with
// the supplied attributes.  avtpHandle should be provided; it is not used by
// SRP but is returned to AVTP as a parameter in calls to avtpStrmCb.attachCb()
// and avtpStrmCb.detachCb().
openavbRC openavbSrpRegisterStream   (void* avtpHandle,
                                 AVBStreamID_t* streamId,
                                 U8 DA[],
                                 AVBTSpec_t* tSpec,
								 SRClassIdx_t SRClassIdx,
                                 bool Rank,
                                 U32 Latency);

// Called by AVTP on talker end station to withdraw the indicated stream
openavbRC openavbSrpDeregisterStream (AVBStreamID_t* streamId);

// Called by AVTP on listener to either express interest in the indicated stream
// or to send a listener declaration back to the indicated stream's talker.
// subType must be openavbSrp_LDSt_Interest, openavbSrp_LDSt_Ready, or openavbSrp_LDSt_Asking_Failed.
// If subType == openavbSrp_LDSt_Interest, avtpHandle should be provided; it is not used
// by SRP but is returned to AVTP as a parameter in calls to avtpStrmCb.registerCb()
// and avtpStrmCb.deregisterCb().
openavbRC openavbSrpAttachStream     (void* avtpHandle,
                                 AVBStreamID_t* streamId,
                                 openavbSrpLsnrDeclSubtype_t type);

// Called by AVTP on listener to withdraw both interest in,
// and, if any, listener declaration for, the indicated stream.
openavbRC openavbSrpDetachStream     (AVBStreamID_t* streamId);

// Get the Priority Code Point (PCP), VLAN Id and 1/classMeasurementInterval
// (in seconds) for the indicated SR Class
openavbRC openavbSrpGetClassParams   (SRClassIdx_t SRClassIdx, U8* priority, U16* vid, U32* inverseIntervalSec);

#endif // OPENAVB_SRP_API_H

