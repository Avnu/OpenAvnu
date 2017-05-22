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

#ifndef _OPENAVB_GRANDMASTER_OSAL_PUB_H
#define _OPENAVB_GRANDMASTER_OSAL_PUB_H

// Initialize the AVB Grandmaster system for client usage
bool osalAVBGrandmasterInit(void);

// Close the AVB Grandmaster system for client usage
bool osalAVBGrandmasterClose(void);

/* Get the current grandmaster information */
/* Referenced by the IEEE Std 1722.1-2013 AVDECC Discovery Protocol Data Unit (ADPDU) */
bool osalAVBGrandmasterGetCurrent(
	uint8_t gptp_grandmaster_id[],
	uint8_t * gptp_domain_number );

/* Get the grandmaster support for the network interface */
/* Referenced by the IEEE Std 1722.1-2013 AVDECC AVB_INTERFACE descriptor */
bool osalClockGrandmasterGetInterface(
	uint8_t clock_identity[],
	uint8_t * priority1,
	uint8_t * clock_class,
	int16_t * offset_scaled_log_variance,
	uint8_t * clock_accuracy,
	uint8_t * priority2,
	uint8_t * domain_number,
	int8_t * log_sync_interval,
	int8_t * log_announce_interval,
	int8_t * log_pdelay_interval,
	uint16_t * port_number);

#endif // _OPENAVB_GRANDMASTER_OSAL_PUB_H
