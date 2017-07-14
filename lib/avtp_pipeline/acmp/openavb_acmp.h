/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
 ******************************************************************
 * MODULE : ACMP - AVDECC Connection Management Protocol
 * MODULE SUMMARY : Interface for the 1722.1 (AVDECC) connection management protocol
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_H
#define OPENAVB_ACMP_H 1

#include "openavb_tl.h"
#include "openavb_array.h"
#include "openavb_list.h"
#include "openavb_avdecc.h"
#include "openavb_acmp_pub.h"

#define OPENAVB_ACMP_AVTP_SUBTYPE (0x7C)

#define OPENAVB_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE (0x00)

// message_type field IEEE Std 1722.1-2013 clause 8.2.1.5
#define OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND (0)
#define OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE (1)
#define OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND (2)
#define OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_TX_RESPONSE (3)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_COMMAND (4)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE (5)
#define OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND (6)
#define OPENAVB_ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE (7)
#define OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND (8)
#define OPENAVB_ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE (9)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND (10)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE (11)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_COMMAND (12)
#define OPENAVB_ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE (13)

// status field field IEEE Std 1722.1-2013 clause 8.2.1.6
#define OPENAVB_ACMP_STATUS_SUCCESS (0)
#define OPENAVB_ACMP_STATUS_LISTENER_UNKNOWN_ID (1)
#define OPENAVB_ACMP_STATUS_TALKER_UNKNOWN_ID (2)
#define OPENAVB_ACMP_STATUS_TALKER_DEST_MAC_FAIL (3)
#define OPENAVB_ACMP_STATUS_TALKER_NO_STREAM_INDEX (4)
#define OPENAVB_ACMP_STATUS_TALKER_NO_BANDWIDTH (5)
#define OPENAVB_ACMP_STATUS_TALKER_EXCLUSIVE (6)
#define OPENAVB_ACMP_STATUS_LISTENER_TALKER_TIMEOUT (7)
#define OPENAVB_ACMP_STATUS_LISTENER_EXCLUSIVE (8)
#define OPENAVB_ACMP_STATUS_STATE_UNAVAILABLE (9)
#define OPENAVB_ACMP_STATUS_NOT_CONNECTED (10)
#define OPENAVB_ACMP_STATUS_NO_SUCH_CONNECTION (11)
#define OPENAVB_ACMP_STATUS_COULD_NOT_SEND_MESSAGE (12)
#define OPENAVB_ACMP_STATUS_TALKER_MISBEHAVING (13)
#define OPENAVB_ACMP_STATUS_LISTENER_MISBEHAVING (14)
#define OPENAVB_ACMP_STATUS_CONTROLLER_NOT_AUTHORIZED (16)
#define OPENAVB_ACMP_STATUS_INCOMPATIBLE_REQUEST (17)
#define OPENAVB_ACMP_STATUS_NOT_SUPPORTED (31)

// flags field IEEE Std 1722.1-2013 clause 8.2.1.17
#define OPENAVB_ACMP_FLAG_CLASS_B (0x0001)
#define OPENAVB_ACMP_FLAG_FAST_CONNECT (0x0002)
#define OPENAVB_ACMP_FLAG_SAVED_STATE (0x0004)
#define OPENAVB_ACMP_FLAG_STREAMING_WAIT (0x0008)
#define OPENAVB_ACMP_FLAG_SUPPORTS_ENCRYPTED (0x0010)
#define OPENAVB_ACMP_FLAG_ENCRYPTED_PDU (0x0020)
#define OPENAVB_ACMP_FLAG_TALKER_FAILED (0x0040)

// ACMP command timeouts IEEE Std 1722.1-2013 Table 8.4
#define OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_TX_COMMAND (2000)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_TX_COMMAND (200)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_STATE_COMMAND (200)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_CONNECT_RX_COMMAND (4500)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_DISCONNECT_RX_COMMAND (500)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_GET_RX_STATE_COMMAND (200)
#define OPENAVB_ACMP_COMMAND_TIMEOUT_GET_TX_CONNECTION_COMMAND (200)


typedef struct {
	U8 cd;
	U8 subtype;
	U8 sv;
	U8 version;
	U8 message_type;			// Redefined from control data
	U8 status;
	U16 control_data_length;
	U8 stream_id[8];
} openavb_acmp_control_header_t;

// ACMPCommandResponse type IEEE Std 1722.1-2013 clause 8.2.2.2.1
typedef struct {
	U8 message_type;
	U8 status;
	U8 stream_id[8];
	U8 controller_entity_id[8];
	U8 talker_entity_id[8];
	U8 listener_entity_id[8];
	U16 talker_unique_id;
	U16 listener_unique_id;
	U8 stream_dest_mac[6];
	U16 connection_count;
	U16 sequence_id;
	U16 flags;
	U16 stream_vlan_id;
} openavb_acmp_ACMPCommandResponse_t;

// ListenerStreamInfo type IEEE Std 1722.1-2013 clause 8.2.2.2.2
typedef struct {
	U8 talker_entity_id[8];
	U16 talker_unique_id;
	U8 connected; // Boolean
	U8 stream_id[8];
	U8 stream_dest_mac[6];
	U8 controller_entity_id[8];
	U16 flags;
	U16 stream_vlan_id;
} openavb_acmp_ListenerStreamInfo_t;

// ListenerPair type IEEE Std 1722.1-2013 clause 8.2.2.2.3
typedef struct {
	U8 listener_entity_id[8];
	U16 listener_unique_id;
} openavb_acmp_ListenerPair_t;

// TalkerStreamInfo type IEEE Std 1722.1-2013 clause 8.2.2.2.4
typedef struct {
	U8 stream_id[8];
	U8 stream_dest_mac[6];
	U16 connection_count;
	openavb_list_t connected_listeners;
	U16 stream_vlan_id;

	// Extra information indicating a GET_TX_CONNECTION_RESPONSE command is needed.
	openavb_acmp_ACMPCommandResponse_t *waiting_on_talker;
} openavb_acmp_TalkerStreamInfo_t;

// InflightCommand type IEEE Std 1722.1-2013 clause 8.2.2.2.5
typedef struct {
	struct timespec timer;
	U8 retried;
	openavb_acmp_ACMPCommandResponse_t command;
	U16 original_sequence_id;
} openavb_acmp_InflightCommand_t;

// ACMPCommandParams type IEEE Std 1722.1-2013 clause 8.2.2.2.6
typedef struct {
	U8 message_type;
	U8 talker_entity_id[8];
	U8 listener_entity_id[8];
	U16 talker_unique_id;
	U16 listener_unique_id;
	U16 connection_count;
	U16 flags;
	U16 stream_vlan_id;
} openavb_acmp_ACMPCommandParams_t;

// State machine global variables IEEE Std 1722.1-2013 clause 8.2.2.3
typedef struct {
	U8 my_id[8];
	//openavb_acmp_ACMPCommandResponse_t rcvdCmdResp;		// defined in spec but implemented such that the listener and talker state machines maintain their own.
	openavb_acmp_control_header_t controlHeader;
} openavb_acmp_sm_global_vars_t;

openavbRC openavbAcmpStart(void);
void openavbAcmpStop(void);

#endif // OPENAVB_ACMP_H
