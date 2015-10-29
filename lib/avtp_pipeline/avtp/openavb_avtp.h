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
* HEADER SUMMARY : Declare the main functions for AVTP.  Includes
* functions to create/destroy and AVTP stream, and to send or receive
* data from that AVTP stream.
*/

#ifndef AVB_AVTP_H
#define AVB_AVTP_H 1

#include "openavb_platform.h"
#include "openavb_intf_pub.h"
#include "openavb_map_pub.h"
#include "openavb_rawsock.h"
#include "openavb_timestamp.h"

#define ETHERTYPE_AVTP 0x22F0
#define ETHERTYPE_8021Q 0x8100
#define ETHERNET_8021Q_OCTETS 4
#define TS_PACKET_LEN  188
#define SRC_PACKET_LEN 192
#define CIP_HEADER_LEN 8

// Length of Ethernet frame header, with and without 802.1Q tag
#define ETH_HDR_LEN			14
#define ETH_HDR_LEN_VLAN	18

// AVTP Headers
#define AVTP_COMMON_STREAM_DATA_HDR_LEN	24

//#define OPENAVB_AVTP_REPORT_RX_STATS 1
#define OPENAVB_AVTP_REPORT_INTERVAL 100

typedef struct {
	// These are significant only for RX data
	U32					timestamp;  // delivery timestamp
	bool				bComplete;	// not waiting for more data
#ifdef OPENAVB_AVTP_REPORT_RX_STATS
	U32					rxCnt, lateCnt, earlyCnt;
	U32					maxLate, maxEarly;
	struct timespec		lastTime;
#endif
} avtp_rx_info_t;
	
typedef struct {
	U8						*data;	// pointer to data
	avtp_rx_info_t			rx;		// re-assembly info
} avtp_info_t;

typedef struct {
	media_q_t 				mediaq;
} avtp_state_t;


/* Info associated with an AVTP stream (RX or TX).
 *
 * The void* handle that is returned to the client
 * really is a pointer to an avtp_stream_t.
 * 
 * TODO: This passed around as void * handle can be typed since the avtp_stream_t is
 * now seen by the talker / listern module.
 * 
 */
typedef struct
{
	// TX socket?
	bool tx;
	// Interface name
	char* ifname;
	// Number of rawsock buffers
	U16 nbuffers;
	// The rawsock library handle.  Used to send or receive frames.
	void *rawsock;
	// The actual socket used by the rawsock library.  Used for poll().
	int sock;
	// The streamID - in network form
	U8 streamIDnet[8];
	// The destination address for stream
	struct ether_addr dest_addr;
	// The AVTP subtype; it determines the encapsulation
	U8 subtype;
	// Max Transit - value added to current time to get play time
	U64 max_transit_usec;
	// Max frame size
	U16 frameLen;
	// AVTP sequence number
	U8 avtp_sequence_num;
	// Paused state of the stream
	bool bPause;
	// Encapsulation-specific state information
	avtp_state_t state;
	// RX info for data sample currently being received
	avtp_info_t info;
	// Mapping callbacks
	openavb_map_cb_t *pMapCB;
	// Interface callbacks
	openavb_intf_cb_t *pIntfCB;
	// MediaQ
	media_q_t *pMediaQ;
	bool bRxSignalMode;

	// TX frame buffer
	U8* pBuf;
	// Ethernet header length
	U32 ethHdrLen;
	
	// Timestamp evaluation related
	openavb_timestamp_eval_t tsEval;

	// Stat related	
	// RX frames lost
	int nLost;
	// Bytes sent or recieved
	U64 bytes;
	
} avtp_stream_t;


typedef void (*avtp_listener_callback_fn)(void *pv, avtp_info_t *data);

// tx/rx
openavbRC openavbAvtpTxInit(media_q_t *pMediaQ,
					openavb_map_cb_t *pMapCB,
					openavb_intf_cb_t *pIntfCB,
					char* ifname,
					AVBStreamID_t *streamID,
					U8* destAddr,
					U32 max_transit_usec,
					U32 fwmark,
					U16 vlanID,
					U8  vlanPCP,
					U16 nbuffers,
					void **pStream_out);

openavbRC openavbAvtpTx(void *pv, bool bSend, bool txBlockingInIntf);

openavbRC openavbAvtpRxInit(media_q_t *pMediaQ, 
					openavb_map_cb_t *pMapCB,
					openavb_intf_cb_t *pIntfCB,
					char* ifname,
					AVBStreamID_t *streamID,
					U8* destAddr,
					U16 nbuffers,
					bool rxSignalMode,
					void **pStream_out);

openavbRC openavbAvtpRx(void *handle);

void openavbAvtpConfigTimsstampEval(void *handle, U32 tsInterval, U32 reportInterval, bool smoothing, U32 tsMaxJitter, U32 tsMaxDrift);

void openavbAvtpPause(void *handle, bool bPause);

void openavbAvtpShutdown(void *handle);

int openavbAvtpTxBufferLevel(void *handle);

int openavbAvtpRxBufferLevel(void *handle);

int openavbAvtpLost(void *handle);

U64 openavbAvtpBytes(void *handle);

#endif //AVB_AVTP_H
