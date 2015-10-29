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
* HEADER SUMMARY : Circular queue for passing data between interfaces
*  and mappers.
*/

#ifndef OPENAVB_MEDIA_Q_H
#define OPENAVB_MEDIA_Q_H 1

#include "openavb_mediaq_pub.h"

// These are Public APIs. Details in openavb_mediaq_pub.h 
//  However the declarations are included here for easy internal use. 
media_q_t* openavbMediaQCreate();
void openavbMediaQThreadSafeOn(media_q_t *pMediaQ);
bool openavbMediaQSetSize(media_q_t *pMediaQ, int itemCount, int itemSize);
bool openavbMediaQAllocItemMapData(media_q_t *pMediaQ, int itemPubMapSize, int itemPvtMapSize);
bool openavbMediaQAllocItemIntfData(media_q_t *pMediaQ, int itemIntfSize);
bool openavbMediaQDelete(media_q_t *pMediaQ);
void openavbMediaQSetMaxLatency(media_q_t *pMediaQ, U32 maxLatencyUsec);
void openavbMediaQSetMaxStaleTail(media_q_t *pMediaQ, U32 maxStaleTailUsec);
media_q_item_t *openavbMediaQHeadLock(media_q_t *pMediaQ);
void openavbMediaQHeadUnlock(media_q_t *pMediaQ);
bool openavbMediaQHeadPush(media_q_t *pMediaQ);
media_q_item_t* openavbMediaQTailLock(media_q_t *pMediaQ, bool ignoreTimestamp);
void openavbMediaQTailUnlock(media_q_t *pMediaQ);
bool openavbMediaQTailPull(media_q_t *pMediaQ);
bool openavbMediaQUsecTillTail(media_q_t *pMediaQ, U32 *pUsecTill);
bool openavbMediaQIsAvailableBytes(media_q_t *pMediaQ, U32 bytes, bool ignoreTimestamp);

#endif  // OPENAVB_MEDIA_Q_H
