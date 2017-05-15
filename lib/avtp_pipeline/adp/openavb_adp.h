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
 * MODULE : ADP - AVDECC Discovery Protocol Internal Interface
 * MODULE SUMMARY : Internal Interface for the 1722.1 (AVDECC) discovery protocol
 ******************************************************************
 */

#ifndef OPENAVB_ADP_H
#define OPENAVB_ADP_H 1

#include "openavb_avdecc.h"
#include "openavb_adp_pub.h"

#define OPENAVB_ADP_AVTP_SUBTYPE (0x7a)

typedef struct {
	U8 cd;
	U8 subtype;
	U8 sv;
	U8 version;
	U8 message_type;			// Redefined from control data
	U8 valid_time;				// Redefined from status
	U16 control_data_length;
	U8 entity_id[8];			// Redefined from stream_id
} openavb_adp_control_header_t;

typedef struct {
	U8 entity_model_id[8];
	U32 entity_capabilities;
	U16 talker_stream_sources;
	U16 talker_capabilities;
	U16 listener_stream_sinks;
	U16 listener_capabilities;
	U32 controller_capabilities;
	U32 available_index;
	U8 gptp_grandmaster_id[8];
	U8 gptp_domain_number;
	U8 reserved0[3];
	U16 identify_control_index;
	U16 interface_index;
	U8 association_id[8];
	U8 reserved1[4];
} openavb_adp_data_unit_t;

typedef struct {
	openavb_adp_control_header_t header;
	openavb_adp_data_unit_t pdu;
} openavb_adp_entity_info_t;

// IEEE Std 1722.1-2013 clause 6.2.3.
typedef struct {
	U64 currentTime;
	openavb_adp_entity_info_t entityInfo;
} openavb_adp_sm_global_vars_t;


openavbRC openavbAdpStart(void);
void openavbAdpStop(void);

openavbRC openavbAdpHaveTL(bool bHaveTL);

#endif // OPENAVB_ADP_H
