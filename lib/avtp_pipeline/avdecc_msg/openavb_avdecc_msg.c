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
 * MODULE SUMMARY :
 *
 * Stream clients (talkers or listeners) must connect to the central
 * "avdecc_msg" process to create a reservation for their traffic.
 *
 * This code implements functions used by both sides of the IPC.
 */


// We are accessed from multiple threads, so need a mutex
MUTEX_HANDLE(gAvdeccMsgStateMutex);

static avdecc_msg_state_t *gAvdeccMsgStateList[MAX_AVDECC_MSG_CLIENTS] = { 0 };
static int nNumInitialized = 0;

EXTERN_DLL_EXPORT bool openavbAvdeccMsgInitialize(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (nNumInitialized++ == 0) {
		MUTEX_ATTR_HANDLE(mta);
		MUTEX_ATTR_INIT(mta);
		MUTEX_ATTR_SET_TYPE(mta, MUTEX_ATTR_TYPE_DEFAULT);
		MUTEX_ATTR_SET_NAME(mta, "gAvdeccMsgStateMutex");
		MUTEX_CREATE_ERR();
		MUTEX_CREATE(gAvdeccMsgStateMutex, mta);
		MUTEX_LOG_ERR("Error creating mutex");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}

// General cleanup of the talker and listener library. Should be called after all Talker and Listeners are closed.
EXTERN_DLL_EXPORT bool openavbAvdeccMsgCleanup()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (--nNumInitialized <= 0) {
		nNumInitialized = 0;

		if (AvdeccMsgStateListGetIndex(0)) {
			AVB_LOG_WARNING("AvdeccMsgStateList not empty on exit");
		}

		MUTEX_CREATE_ERR();
		MUTEX_DESTROY(gAvdeccMsgStateMutex);
		MUTEX_LOG_ERR("Error destroying mutex");
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return TRUE;
}


bool AvdeccMsgStateListAdd(avdecc_msg_state_t * pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState) {
		AVB_LOG_ERROR("AvdeccMsgStateListAdd invalid param");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	AVDECC_MSG_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSG_CLIENTS; i1++) {
		if (!gAvdeccMsgStateList[i1]) {
			gAvdeccMsgStateList[i1] = pState;
			AVDECC_MSG_UNLOCK();
			AVB_LOGF_DEBUG("AvdeccMsgStateListAdd %d succeeded", pState->avdeccMsgHandle);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return TRUE;
		}
	}
	AVDECC_MSG_UNLOCK();
	AVB_LOGF_WARNING("AvdeccMsgStateListAdd %d out of space", pState->avdeccMsgHandle);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return FALSE;
}

bool AvdeccMsgStateListRemove(avdecc_msg_state_t * pState)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!pState) {
		AVB_LOG_ERROR("AvdeccMsgStateListRemove invalid param");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		return FALSE;
	}

	AVDECC_MSG_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSG_CLIENTS; i1++) {
		if (gAvdeccMsgStateList[i1] == pState) {
			gAvdeccMsgStateList[i1] = NULL;
			AVDECC_MSG_UNLOCK();
			AVB_LOGF_DEBUG("AvdeccMsgStateListRemove %d succeeded", pState->avdeccMsgHandle);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
			return TRUE;
		}
	}
	AVDECC_MSG_UNLOCK();
	AVB_LOGF_WARNING("AvdeccMsgStateListRemove %d not found", pState->avdeccMsgHandle);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return FALSE;
}

avdecc_msg_state_t * AvdeccMsgStateListGet(int avdeccMsgHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	if (!avdeccMsgHandle) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
		AVB_LOG_ERROR("AvdeccMsgStateListGet invalid param");
		return NULL;
	}

	AVDECC_MSG_LOCK();
	int i1;
	for (i1 = 0; i1 < MAX_AVDECC_MSG_CLIENTS; i1++) {
		if (gAvdeccMsgStateList[i1]) {
			avdecc_msg_state_t *pState = (avdecc_msg_state_t *)gAvdeccMsgStateList[i1];
			if (pState->avdeccMsgHandle == avdeccMsgHandle) {
				AVDECC_MSG_UNLOCK();
				AVB_LOGF_DEBUG("AvdeccMsgStateListGet found index %d, handle %d", i1, pState->avdeccMsgHandle);
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
				return pState;
			}
		}
	}
	AVDECC_MSG_UNLOCK();
	AVB_LOGF_DEBUG("AvdeccMsgStateListGet %d not found", avdeccMsgHandle);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}

avdecc_msg_state_t * AvdeccMsgStateListGetIndex(int nIndex)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC_MSG);

	AVDECC_MSG_LOCK();
	int i1, found = 0;
	for (i1 = 0; i1 < MAX_AVDECC_MSG_CLIENTS; i1++) {
		if (gAvdeccMsgStateList[i1]) {
			if (found++ == nIndex) {
				avdecc_msg_state_t *pState = (avdecc_msg_state_t *)gAvdeccMsgStateList[i1];
				AVDECC_MSG_UNLOCK();
				AVB_LOGF_DEBUG("AvdeccMsgStateListGetIndex found index %d, handle %d for supplied index %d", i1, pState->avdeccMsgHandle, nIndex);
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
				return pState;
			}
		}
	}
	AVDECC_MSG_UNLOCK();
	AVB_LOGF_DEBUG("AvdeccMsgStateListGetIndex no match for index %d", nIndex);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC_MSG);
	return NULL;
}
