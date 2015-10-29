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
* HEADER SUMMARY : Talker Listener header
*/

#ifndef OPENAVB_TL_H
#define OPENAVB_TL_H 1

#include "openavb_types.h"
#include "openavb_osal.h"
#include "openavb_mediaq_pub.h"
#include "openavb_tl_pub.h"

typedef enum OPENAVB_TL_AVB_VER_STATE 
{
	OPENAVB_TL_AVB_VER_UNKNOWN = 0,
	OPENAVB_TL_AVB_VER_INVALID,
	OPENAVB_TL_AVB_VER_VALID,
} openavbTLAVBVerState_t;

typedef struct {
	U64 totalCalls;
	U64 totalFrames;
	U64 totalLate;
	U64 totalBytes;
} talker_stats_t;

THREAD_TYPE(TLThread);

typedef struct {
	// Running flag. (assumed atomic)
	bool bRunning;

	// Connected to endpoint flag. (assumed atomic)
	bool bConnected;

	// Streaming data flag. (assumed atomic)
	bool bStreaming;

	// The status of the version check to make sure endpoint and TL are running the same version.
	openavbTLAVBVerState_t AVBVerState;

	// Configuration settings. (Values are set once from a single thread no lock needed).
	openavb_tl_cfg_t cfg;

	// Handle to the endpoint. (Set once from a single thread no lock needed.)
	int endpointHandle;

	// Media queue struct.
	media_q_t *pMediaQ;

	// Private talker data.
	void *pPvtTalkerData;

	// Private listener data.
	void *pPvtListenerData;

	// Thread for talker or listener
	THREAD_DEFINITON(TLThread);

	// Per stream Stats Mutex
	MUTEX_HANDLE(statsMutex);

	LINK_LIB(mapLib);

	LINK_LIB(intfLib);
} tl_state_t;

// Clock that we use for all timers in TL
//#define TIMER_CLOCK	CLOCK_MONOTONIC

////////////////
// TL state mutex
////////////////
extern MUTEX_HANDLE(gTLStateMutex);
#define TL_LOCK() { MUTEX_CREATE_ERR(); MUTEX_LOCK(gTLStateMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define TL_UNLOCK() { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(gTLStateMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

////////////////
// TL stats mutex
////////////////
#define LOCK_STATS()        { MUTEX_CREATE_ERR(); MUTEX_LOCK(pTLState->statsMutex); MUTEX_LOG_ERR("Mutex lock failure"); }
#define UNLOCK_STATS()      { MUTEX_CREATE_ERR(); MUTEX_UNLOCK(pTLState->statsMutex); MUTEX_LOG_ERR("Mutex unlock failure"); }

////////////////
// timespec support functions
////////////////
void timespec_add_usec(struct timespec *t, unsigned long us);
void timespec_sub_usec(struct timespec *t, unsigned long us);
unsigned long timespec_usec_diff(struct timespec *t1, struct timespec *t2);
int timespec_cmp(struct timespec *a, struct timespec *b);

////////////////
// TL Handle List functions. The TL handle list is created in the implemntation of openavbTLInitialize.
////////////////
// Get a tl_handle_t from the TLHandleList given an endpointHandle.
extern U32 gMaxTL;
extern tl_handle_t *gTLHandleList;
tl_handle_t TLHandleListGet(int endpointHandle);

bool TLHandleListRemove(tl_handle_t handle);
void openavbTLUnconfigure(tl_state_t *pTLState);

// Remove a tl_handle_t from the TL handle list.
bool TLHandleRemove(tl_handle_t handle);


////////////////
// AVDECC Integration functions.
////////////////
// Run a single talker or listener. At this point data can be sent or recieved. Used in place of the public openavbTLRun
bool openavbTLAVDECCRunListener(tl_handle_t handle, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidListenerStreamInfo);
bool openavbTLAVDECCRunTalker(tl_handle_t handle, U16 configIdx, U16 descriptorType, U16 descriptorIdx, void *pVoidTalkerStreamInfo);

// Stop a single talker or listener. At this point data will not be sent or recieved. Used in place of the public openavbTLStop.
bool openavbTLAVDECCStopListener(tl_handle_t handle, U16 configIdx, void *pVoidListenerStreamInfo);
bool openavbTLAVDECCStopTalker(tl_handle_t handle, U16 configIdx, void *pVoidTalkerStreamInfo);

// Get talker stream details. Structure members in TalkerStrreamInfo will be filled.
bool openavbTLAVDECCGetTalkerStreamInfo(tl_handle_t handle, U16 configIdx, void *pVoidTalkerStreamInfo);


////////////////
// OSAL implementation functions
////////////////
bool openavbTLThreadFnOsal(tl_state_t *pTLState);
bool openavbTLOpenLinkLibsOsal(tl_state_t *pTLState);
bool openavbTLCloseLinkLibsOsal(tl_state_t *pTLState);

/* These were in openavb_endpoint.h, but was moved here
 * for implementations that do not have endpoint */
bool openavbEptClntService(int h, int timeout);
bool openavbEptClntStopStream(int h, AVBStreamID_t *streamID);

#endif  // OPENAVB_TL_H
