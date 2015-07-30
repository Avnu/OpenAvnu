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
* MODULE SUMMARY : Provide a simple tracing facility for use
* during development.
* 
* Based off of the openavb_log facility.
* 
* NOTE: The Tracing uses a high resolution timer and therefore
*   	librt must be linked into any application that uses this
*   	facility.
*/

#ifndef AVB_TRACE_H
#define AVB_TRACE_H 	1

#include "openavb_trace_pub.h"

// Features to trace. 1 is enabled, 0 is disabled.
#define AVB_TRACE_ACMP					0
#define AVB_TRACE_ADP					0
#define AVB_TRACE_AECP					0
#define AVB_TRACE_AEM					0
#define AVB_TRACE_AVDECC				0
#define AVB_TRACE_AVTP					1
#define AVB_TRACE_AVTP_DETAIL			1
#define AVB_TRACE_AVTP_TIME				0
#define AVB_TRACE_AVTP_TIME_DETAIL		0
#define AVB_TRACE_ENDPOINT				0
#define AVB_TRACE_MEDIAQ				0
#define AVB_TRACE_MEDIAQ_DETAIL			0
#define AVB_TRACE_QUEUE_MANAGER			0
#define AVB_TRACE_MAAP					0
#define AVB_TRACE_RAWSOCK				1
#define AVB_TRACE_RAWSOCK_DETAIL		1
#define AVB_TRACE_SRP_PUBLIC			0
#define AVB_TRACE_SRP_PRIVATE			0
#define AVB_TRACE_TL					0
#define AVB_TRACE_TL_DETAIL				0
#define AVB_TRACE_PTP					0
#define AVB_TRACE_FQTSS					0
#define AVB_TRACE_FQTSS_DETAIL			0
#define AVB_TRACE_HR_TMR				0
#define AVB_TRACE_NANOSLEEP				0
#define AVB_TRACE_TIME					0
#define AVB_TRACE_HAL_ETHER				0
#define AVB_TRACE_HAL_ETHER_DETAIL		0
#define AVB_TRACE_HAL_TASK_TIMER		0
#define AVB_TRACE_DEBUG					0
#endif // AVB_TRACE_H
