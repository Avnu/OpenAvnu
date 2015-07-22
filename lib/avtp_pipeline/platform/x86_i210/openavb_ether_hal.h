/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#ifndef HAL_ETHER_H
#define HAL_ETHER_H	1

#include "openavb_platform.h"
#include "openavb_types_base.h"

// Initialize HAL layer Ethernet driver
bool halEthernetInitialize(U8 *macAddr, bool gmacAutoNegotiate);

// Finalize HAL layer Ethernet driver
bool halEthernetFinalize(void);

// Setup for multicast filtering
bool halEtherAddMulticast(U8 *multicastAddr, bool add);

// Get next AVTP Rx packet
U8 *halGetRxBufAVTP(U32 *frameSize);

// Get next AVB Rx packet. pPtpCBUsed will be set to true when rx data was send to the RX cb rather than returning the buffer.
U8 *halGetRxBufAVB(U32 *frameSize, bool *bPtpCBUsed);

// Get next GEN Rx packet
U8 *halGetRxBufGEN(U32 *frameSize);

// Send Tx Packet to driver
bool halSendTxBuffer(U8 *pDataBuf, U32 frameSize, int class);

// Is the link up. Returns TRUE if it is.
bool halIsLinkUp(void);

// Returns pointer to mac address
U8 *halGetMacAddr(void);

// Return the MTU
U32 halGetMTU(void); 

bool halGetLocaltime(U64 *localTime64);

// Add stream to the credit based shaper
void halTrafficShaperAddStream(int class, uint32_t streamsBytesPerSec);

// Remove stream from the credit based shaper
void halTrafficShaperRemoveStream(int class, uint32_t streamsBytesPerSec);

#endif	// HAL_ETHER_H
