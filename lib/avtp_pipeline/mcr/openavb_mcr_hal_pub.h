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
* MODULE : Public interface for media clock recovery
*/

#ifndef OPENAVB_MCR_HAL_PUB_H
#define OPENAVB_MCR_HAL_PUB_H

#include "openavb_platform_pub.h"
#include "openavb_types_base_pub.h"

#define HAL_INIT_MCR_V2(packetRate, pushInterval, timestampInterval, recoveryInterval) halInitMCR(packetRate, pushInterval, timestampInterval, recoveryInterval)
#define HAL_CLOSE_MCR_V2() halCloseMCR()
#define HAL_PUSH_MCR_V2() halPushMCR()

// Initialize HAL MCR
bool halInitMCR(U32 packetRate, U32 pushInterval, U32 timeStampInterval, U32 recoveryInterval);

// Close HAL MCR
bool halCloseMCR(void);

// Push MCR Event
bool halPushMCR(void);

// MCR timer adjustment. Negative value speed up the media clock. Positive values slow the media clock.
// Will take effect during the next clock recovery interval. This is completely indepentant from pure MCR and
// allows for adjustments based on media buffer levels. The value past in works as credit with each 
// MCR timer cycle bump up and down the clock temporarily.
void halAdjustMCRNSec(S32 adjNSec);

// The granularity is used to set coarseness of the values that will be passed into halAdjustMCRNSec.
// This is used to balance if the timestamps or values from halAdjustMCRNSec are used to adjust the clock.
void halAdjustMCRGranularityNSec(U32 adjGranularityNSec);


#endif // OPENAVB_MCR_HAL_PUB_H
