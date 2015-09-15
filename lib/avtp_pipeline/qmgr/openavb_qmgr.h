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
 * MODULE SUMMARY : The AVB Queue Manager sets up Linux network packet
 * scheduling, so that transmitted Ethernet frames for an AVB stream
 * are sent according to their SRP reservation.
 */

#ifndef AVB_QMGR_H
#define AVB_QMGR_H 1

#include "openavb_types.h"

#define INVALID_FWMARK (U16)(-1)
#define DEFAULT_FWMARK (U16)(1)

#define FQTSS_MODE_DISABLED		0
#define FQTSS_MODE_DROP_ALL		1
#define FQTSS_MODE_ALL_NONSR	2
#define FQTSS_MODE_DROP_SR		3
#define FQTSS_MODE_SW			4
#define FQTSS_MODE_HW_CLASS		5

#ifdef AVB_FEATURE_FQTSS

bool openavbQmgrInitialize(int mode,
					   int ifindex,
					   const char* ifname,
					   unsigned mtu,
					   unsigned link_kbit,
					   unsigned nsr_kbit);

void openavbQmgrFinalize();

U16 openavbQmgrAddStream(SRClassIdx_t nClass, 
					 unsigned classRate,
					 unsigned maxIntervalFrames,
					 unsigned maxFrameSize);

void openavbQmgrRemoveStream(U16 fwmark);

#else
/* Dummy versions to use if FQTSS is compiled out
 */
inline bool openavbQmgrInitialize(int mode,
							  int ifindex,
							  const char* ifname,
							  unsigned mtu,
							  unsigned link_kbit,
							  unsigned nsr_kbit)
{ return TRUE; }

inline void openavbQmgrFinalize() 
{}

inline U16 openavbQmgrAddStream(SRClassIdx_t nClass,
							unsigned classRate,
							unsigned maxIntervalFrames,
							unsigned maxFrameSize) 
{ return DEFAULT_FWMARK; }

inline void openavbQmgrRemoveStream(U16 fwmark)

{}
#endif // AVB_FEATURE_FQTSS

#endif // AVB_QMGR_H
