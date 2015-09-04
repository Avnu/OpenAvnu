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
* MODULE : Internal AVB Base Types (have no dependencies)
*/

#ifndef AVB_TYPES_BASE_H
#define AVB_TYPES_BASE_H 1

#include <assert.h>
#include "openavb_types_base_pub.h"
#include "openavb_result_codes.h"

typedef struct { // per IEEE 802.1Q-2011 Section 35.2.2.8.4
	U16		maxFrameSize;
	U16		maxIntervalFrames;
} AVBTSpec_t;

// Ethernet Frame overhead
// L2 includes MAC src/dest, VLAN tag, Ethertype
#define OPENAVB_AVTP_L2_OVERHEAD 18
// L1 includes preamble, start-of-frame, FCS, interframe gap
#define OPENAVB_AVTP_L1_OVERHEAD 24
// All of the above
#define OPENAVB_AVTP_ETHER_FRAME_OVERHEAD (OPENAVB_AVTP_L1_OVERHEAD + OPENAVB_AVTP_L2_OVERHEAD)

// Maximum number of streams per class
// (Fixed number for efficiency in FQTSS kernel module)
#define MAX_AVB_STREAMS_PER_CLASS 16
// Maximum number of streams that we handle
#define MAX_AVB_STREAMS (MAX_AVB_SR_CLASSES * MAX_AVB_STREAMS_PER_CLASS)

#define SR_CLASS_IS_VALID(C) (C>=0 && C<MAX_AVB_SR_CLASSES)
#define AVB_CLASS_LABEL(C) ('A'+C)

#define OPENAVB_DEFAULT_CLASSA_MAX_LATENCY (2 * NANOSECONDS_PER_MSEC)
#define OPENAVB_DEFAULT_CLASSB_MAX_LATENCY (50 * NANOSECONDS_PER_MSEC)

// Ethernet MAC Address Length - 6 bytes
#define ETH_MAC_ADDR_LEN  6

// For production builds, use the first define here to turn assert off
// For debug builds, use the second define here to turn assert on
// CAUTION: Any executable code in _AST_ will not be included in production code.
//#define OPENAVB_ASSERT(_AST_)
#define OPENAVB_ASSERT(_AST_)    assert(_AST_)

// Features
//#define OPENAVB_GST_PIPELINE_INSTRUMENTATION 1

#endif // AVB_TYPES_BASE_H
