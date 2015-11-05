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
* HEADER SUMMARY : Listener
*/

#ifndef OPENAVB_TL_LISTENER_H
#define OPENAVB_TL_LISTENER_H 1

#include "openavb_tl.h"

typedef struct {
	U64 totalCalls;
	U64 totalFrames;
	U64 totalLost;
	U64 totalBytes;
} listener_stats_t;

typedef struct {
	// Data from callback
	char			ifname[IFNAMSIZ];
	AVBStreamID_t 	streamID;
	U8				destAddr[ETH_ALEN];
	AVBTSpec_t		tSpec;
	long			nFramesRx;

	// State info for streaming
	void			*avtpHandle;
	unsigned long	nReportFrames;
	unsigned long	nReportCalls;
	U64 			nextReportNS;
	U64				nextSecondNS;
	listener_stats_t stats;
} listener_data_t;

void openavbTLRunListener(tl_state_t *pTLState);
void openavbTLPauseListener(tl_state_t *pTLState, bool bPause);
void openavbListenerClearStats(tl_state_t *pTLState);
void openavbListenerAddStat(tl_state_t *pTLState, tl_stat_t stat, U64 val);
U64 openavbListenerGetStat(tl_state_t *pTLState, tl_stat_t stat);
bool openavbTLRunListenerInit(int h, AVBStreamID_t *streamID);
bool listenerStartStream(tl_state_t *pTLState);
void listenerStopStream(tl_state_t *pTLState);

#endif  // OPENAVB_TL_LISTENER_H
