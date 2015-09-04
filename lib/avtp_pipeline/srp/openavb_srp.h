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
* This file declares the "Private" portion - see also openavb_srp_api.h
*/

#ifndef OPENAVB_SRP_H
#define OPENAVB_SRP_H

#include "openavb_srp_api.h"
//#include "openavb_srp_osal.h"
#include "openavb_poll_osal.h"

enum {
	SRP_POLL_RAW_SOCK_FD = 0,
	SRP_POLL_FD_COUNT      // Must be the last entry.
};

// TBD - How often to [re]send (tx of the Applicant State Machine) - should be configurable
#define OPENAVB_SRP_TX_PERIOD_SEC  10

#define openavbSrp_PROTO_VERSION  0x00 // IEEE 802.1Q-2011 Section 35.2.2.3
#define openavbSrp_ETHERTYPE  0x22EA // there should be a more general define for this someplace with other ether types

// SR Class Id values are fixed. Priority (PCP) and VLAN Id values
// have defaults specified, but should be configurable - TBD
// Tables referenced here are per IEEE 802.1Q-2011
// SR Class - SR Class Id - SR Class Priority -  SR Class VID
//    -        Table 35-7       Table 6-6         Table 9-2
//    A           6                3                 2
//    B           5                2                 2

// SR Class IDs are defined here - used only in SRP messages
#define SR_CLASS_A_ID 6
#define SR_CLASS_B_ID 5

// SR Class default Priority (PCP) values per IEEE 802.1Q-2011 Table 6-6
#define SR_CLASS_A_DEFAULT_PRIORITY 3
#define SR_CLASS_B_DEFAULT_PRIORITY 2
// SR Class default VLAN Id values per IEEE 802.1Q-2011 Table 9-2
#define SR_CLASS_A_DEFAULT_VID 2
#define SR_CLASS_B_DEFAULT_VID 2

// SR Class default delta bandwidth values per IEEE 802.1Q-2011 Section 34.3.1
#define SR_CLASS_A_DEFAULT_DELTA_BW  75
#define SR_CLASS_B_DEFAULT_DELTA_BW   0

// SR Class Measurement Intervals / Frames Per Second
// - see IEEE 802.1Q-2011 Sections 34.6.1 and 35.2.2.8.4 and Table 35-5
// Class   Class Measurement Interval   Frames per Second
//   A        125 uSec       1 / .000125 = 8000
//   B        250 uSec       1 / .000250 = 4000
#define SR_CLASS_A_FPS  8000
#define SR_CLASS_B_FPS  4000


// Ethernet Frame DA for openavbSrp packets is the
// “Individual LAN Scope group address, Nearest Bridge group address” (per IEEE 802.1Q)
static const U8 openavbSrpDA[ETH_MAC_ADDR_LEN] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E };

enum openavbSrpAttributeLength { // IEEE 802.1Q-2011 Table 35-2
	openavbSrp_AtLen_Lsnr    = 0x08,
	openavbSrp_AtLen_TlkrAdv = 0x19, // (= 25)
	openavbSrp_AtLen_TlkrFld = 0x22, // (= 34)
	openavbSrp_AtLen_Domain  = 0x04
};

// openavbSrp Attribute Event (aka "ThreePacked Events") - IEEE 802.1Q-2011 Section 35.2.2.7.1
typedef enum openavbSrpAtEvt {
	openavbSrp_AtEvt_New    = 0x00,
	openavbSrp_AtEvt_JoinIn = 0x01,
	openavbSrp_AtEvt_In     = 0x02,
	openavbSrp_AtEvt_JoinMt = 0x03,
	openavbSrp_AtEvt_Mt     = 0x04,
	openavbSrp_AtEvt_Leave  = 0x05,
	openavbSrp_AtEvt_None
} openavbSrpAtEvt_t;

typedef struct openavbSrpSockInfo {
	void* rawTxSock;
	void* rawRxSock;
	int   RxSock;
} openavbSrpSockInfo_t;

typedef struct avtpCallbacks {
	strmAttachCb_t  attachCb;
	strmRegCb_t     registerCb;
} avtpCallbacks_t;

typedef struct SrClassParameters {
	U8  priority;
	U16 vid;
	U32 operIdleSlope;  // currently reserved bandwidth - bits per second
	U32 deltaBandwidth; // %
	                    // TBD - deltaBandwidth is supposed to be scaled by 1,000,000
	                    //       (100,000,000 == 100%), but it is NOT in this current implementation
	U32 inverseIntervalSec; // 1/classMeasurementInterval (in seconds)
} SrClassParameters_t;

// Applicant State - IEEE 802.1Q Table 10-3
// State here are simplified becasue:
// - we are suppporting point-to-point only and
// - initial declaration sends are immediate so there is no need for VP and VN
typedef enum openavbSrpAppState {
	openavbSrp_ApSt_VO = 0x00, // no declarations - note that we initialize to 0
	openavbSrp_ApSt_AN = 0x01, // [at least] one new  sent - one more pending
	openavbSrp_ApSt_AA = 0x02, // [at least] one join sent - one more pending
	openavbSrp_ApSt_QA = 0x03, // at least two new or join sent - none pending
	openavbSrp_ApSt_LA = 0x04, // one leave sent - one more pending
	openavbSrp_ApSt_VN = 0x05, // first new pending
} openavbSrpAppState_t;

// Registrar State - IEEE 802.1Q Table 10-4
typedef enum openavbSrpRegState {
	openavbSrp_RgSt_MT = 0x00, // Empty - note that we initialize to 0
	openavbSrp_RgSt_LV = 0x01, // Leave
	openavbSrp_RgSt_IN = 0x02, // In
} openavbSrpRegState_t;


// Element for linked list of declared / registered streams.
// Since both a talker and a listener can exist for the same stream on the same
// end-station, seperate talker and listener lists are kept.
// IMPORTANT NOTE: This implementation assumes that on a given end-station, no
// more than one talker and no more than one listener exist for each stream; if
// this is not the case, individual stream list elements could be corrupted
// because they are not thread-safe; the overall lists are mutex protected.
//
// On talker:
//   a list entry will exist only if the talker has a stream to declare
//   (or is in the process of withdrawing)
//   - declType is what this talker has most recently declared;
//   - declSubType is not used;
//   - regType / regSubType indicate what the talker has most recently received from
//     the listener(s), if anything;
//   - avtpHandle, streamId, DA, tSpec, SRClassId, Rank, and Latency
//     are as received from AVTP via openavbSrpRegisterStream();
//   - failInfo is populated only if this station has insufficient outbound
//     bandwidth for this stream, so must send Talker Failed delcaration;
//   - kbpsReserved is the bandwidth currently reserved for the stream.
//
// On Listener: 
//   a list entry will exist if the listener has either expressed interest in
//   receiving the stream (or is in the process of withdrawing interest) or has
//   received a talker declaration for the stream, or both.
//   - declType is what this listener has most recently declared (via
//     openavbSrpAttachStream()), if anything;
//   - declSubType is the listener declaration subtype, if any;
//   - regType indicates what talker declaration the listener has most recently
//     received for the stream, if anything; (openavbSrp_AtTyp_None indicates that
//     the listener currently has no talker declaration for the stream);
//   - regSubType is not used;
//   - avtpHandle is as recieved from AVTP via openavbSrpAttachStream();
//   - strmId is either as recieved from AVTP via openavbSrpAttachStream() or
//     as recieved via a talker declaration packet (MSRPDU), whichever occurs first;
//   - SRClassIdx is derived from priority received in talker declaration packet;
//   - DA, tSpec, latency, and, if applicable, failInfo are
//     as received in talker declaration packet
//   - rank and kbpsReserved are not applicable on the listener;
//
typedef struct openavbSrpStrm {
	struct openavbSrpStrm*      prev;
	struct openavbSrpStrm*      next;
	AVBStreamID_t           strmId;
	void*                   avtpHandle;
	SRClassIdx_t            SRClassIdx;
	openavbSrpAttribType_t      declType;    // attribute type this station is declaring for this stream, if any;
	                                     // (declType == openavbSrp_AtTyp_None is redundant to appState == openavbSrp_ApSt_VO).
	openavbSrpLsnrDeclSubtype_t declSubType; // listener subtype this station is declaring for this stream, if any;
	                                     // not used on talker; valid only if declType == openavbSrp_AtTyp_Listener.
	openavbSrpAttribType_t      regType;     // attribute type   this station has recieved (registered) for this stream, if any;
	openavbSrpLsnrDeclSubtype_t regSubType;  // listener subtype this station has recieved (registered) for this stream, if any;
	                                     // not used on listener; valid only if regType == openavbSrp_AtTyp_Listener.
	U8                      DA[ETH_MAC_ADDR_LEN];
	AVBTSpec_t              tSpec;
	U32                     kbpsReserved;
	bool                    rank;
	U32                     latency;
	openavbSrpFailInfo_t        failInfo;
	openavbSrpAppState_t        appState; // current applicant state of the stream on this station
	openavbSrpRegState_t        regState;
} openavbSrpStrmElem_t;


// SRP internal routines
int               openavbSrpGetIndexFromId   (U8 SRClassId);
void              openavbSrpRemoveStrm       (openavbSrpStrmElem_t** lstHead, openavbSrpStrmElem_t* pStream);
openavbSrpStrmElem_t* openavbSrpFindOrCreateStrm (openavbSrpStrmElem_t** lstHead, AVBStreamID_t* streamId,
                                          bool allowCreate, bool* created);
void              openavbSrpLogBw            (void);
void              openavbSrpLogAllStreams    (void);
void              openavbSrpRegState         (openavbSrpAttribType_t atrbType);
void              openavbSrpCheckAsCapable   (void);
void              openavbSrpCheckTalkerFailed(void);
void              openavbSrpResend           (bool tx, openavbSrpAttribType_t atrbType);
void*             openavbSrpTxThread         (void* notUsed);
void*             openavbSrpRcvThread        (void* notUsed);
openavbRC          openavbSrpCheckStartLsnr   (openavbSrpStrmElem_t* stream);
openavbRC          openavbSrpDomainSend       (void);
openavbRC          openavbSrpSend             (openavbSrpStrmElem_t* pStream, openavbSrpAtEvt_t atrbEvent);
void              openavbSrpReceive          (U8* msrpdu,  unsigned int pduLen);
void              openavbSrpCalcKbps         (int SRClassIndex, AVBTSpec_t* tSpec, U32* kbps);
openavbRC          openavbSrpCheckAvlBw       (int SRClassIndex, U32 kbps);
openavbRC          openavbSrpReserve          (openavbSrpStrmElem_t* pStream);
void              openavbSrpRelease          (openavbSrpStrmElem_t* pStream);

openavbRC          openavbSrpOpenPtpSharedMemory   (void);
void              openavbSrpReleasePtpSharedMemory(void);
bool              openavbSrpAsCapable             (void);

int               openavbSrpInitializePfd    (OPENAVB_POLLFD_TYPE *pfd);

// First Value content and size depend on the Declaration Type (openavbSrpAttribType_t)
// see IEEE 802.1Q-2011 Section 35.2.2.8.1
// (left column indicates byte index to start of parameter)
// 	FirstValue ::= 
// 	 0	StreamID: 8 bytes
// 	 0		[Src] MAC: 6 bytes (ETH_MAC_ADDR_LEN)
// 	 6		UniqueID: 2 bytes, U16
// 		end of the Listener attribute 
// 	 8	DataFrameParameters: 8 bytes
// 	 8		DA: 6 bytes (ETH_MAC_ADDR_LEN)
// 	14		VID: 2 bytes
// 	16	TSpec: 4 bytes
// 	16		MaxFrameSize: 2 bytes, U16
// 	18		MaxIntervalFrames: 2 bytes, U16
// 	20	PriorityAndRank: 1 byte
// 	20		Priority: 3 bits
// 	20		Rank: 1 bit
// 	20		Reserved: 4 bits
// 	21	AccumulatedLatency: 4 bytes, U32
// 		end of the Talker Advertise attribute
// 	25	FailureInformation: 9 bytes
// 	25		BridgeID: 8 bytes
// 	33		FailureCode: 1 byte
// 		end of the Talker Failed attribute


/* description of the limited MSRPDU being supported here
 * (currently, only one declaration per packet is supported)
 * openavbSrpDU ::= ProtocolVersion, Message
 * 	ProtocolVersion BYTE ::= 0x00
 * 	Message ::= AttributeType, AttributeLength [, AttributeListLength], AttributeList
 * 		AttributeType BYTE ::= Talker Advertise | Talker Failed | Listener
 * 			Talker Advertise::= 0x01
 * 			Talker Failed::= 0x02
 * 			Listener::= 0x03
 * 		AttributeLength BYTE ::= 0x19 | 0x34 | 0x08
 * 		AttributeListLength SHORT ::= AttributeLength  + 3 | 4 (4 only if AttributeType  = Listener)
 * 		AttributeList ::= VectorAttribute {, VectorAttribute}, EndMark
 * 			VectorAttribute ::= VectorHeader, FirstValue, Vector
 * 				VectorHeader SHORT ::= 0x0001
 * 				FirstValue ::= 
 * 					StreamID: 8 bytes
 * 						[Src] MAC: 6 bytes
 * 						UniqueID: 2 bytes, U16
 * 					end of the Listener attribute 
 * 					DataFrameParameters: 8 bytes
 * 						DA: 6 bytes
 * 						VID: 2 bytes
 * 					TSpec: 4 bytes
 * 						MaxFrameSize: 2 bytes, U16
 * 						MaxIntervalFrames: 2 bytes, U16
 * 					PriorityAndRank: 1 byte
 * 						Priority: 3 bits
 * 						Rank: 1 bit
 * 						Reserved: 4 bits
 * 					AccumulatedLatency: 4 bytes, U32
 * 					end of the Talker Advertise attribute
 * 					FailureInformation: 9 bytes
 * 						BridgeID: 8 bytes
 * 						FailureCode: 1 byte
 * 					end of the Talker Failed attribute
 * 				Vector ::= ThreePackedEvent, [FourPackedEvent]
 * 					ThreePackedEvents BYTE ::= New |Lv
 * 							New ::= 0
 * 							Lv ::= 5
 * 					FourPackedEvents BYTE ::=  Asking Failed | Ready | Ready Failed
 * 					 (FourPackedEvents  is included only if AttributeType  = Listener)
 * 						Asking Failed ::= 1
 * 						Ready ::= 2
 * 						Ready Failed ::= 3
 */

#endif // OPENAVB_SRP_H

