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
* MODULE SUMMARY : Control interface module.
* 
* This interface module sends and receives control messages. There are
* two modes this interface module can be configured for normal mode and mux
* mode. In normal mode the control messages are exchanged with the host
* application. In mux mode the control messages will be multiplexed from
* multiple control streams into one out-going talker control stream. This
* is part of the dispatch model for configuring the control message system
* in the OPENAVB AVB stack.
*/

// WORK IN PROGRESS

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"
#include "openavb_intf_ctrl_pub.h"

#define	AVB_LOG_COMPONENT	"Control Interface"
#include "openavb_log_pub.h" 

// Forward Declarations
bool openavbIntfCtrlSendControl(void *pIntfHandle, U8 *pData, U32 dataLength, U32 usecDelay);


typedef struct {
	/////////////
	// Config data
	/////////////
	// Multiplexing mode. Multiple lister streams flowing into one talker stream.
	bool muxMode;

	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	/////////////
	// Variable data
	/////////////
	// Callback into the hosting application when control messages are received.
	openavb_intf_ctrl_receive_cb_t openavbIntfCtrlReceiveControlFn;

	// Handle to pass back to the hosting application.
	void *pTLHandle;

	// Flag indication that the mux talker is owned by this interface instance.
	bool bOwnsCtrlMuxMediaQ;

} pvt_data_t;

// This is the talker mux media queue. The media queue will be set as thread safe mode and loccking for head and tail operations
// will be handled within the MediaQ. This interface module is designed such that there can only be 1 ctrl mux talker in a process.
media_q_t *pCtrlMuxMediaQ = NULL;

openavb_intf_host_cb_list_t openavbIntfHostCBList;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfCtrlCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (pMediaQ) {
		char *pEnd;
		long tmp;

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (strcmp(name, "intf_nv_mux_mode") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->muxMode = (tmp == 1);
			}
		}
		else if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->ignoreTimestamp = (tmp == 1);
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfCtrlGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfCtrlTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		// Talker control interfaces will always enable MediaQ thread safety. 
		openavbMediaQThreadSafeOn(pMediaQ);

		if (pPvtData->muxMode) {
			if (!pPvtData->bOwnsCtrlMuxMediaQ) {
				pCtrlMuxMediaQ = pMediaQ;
				pPvtData->bOwnsCtrlMuxMediaQ = TRUE;
			}
			else {
				AVB_LOG_ERROR("Only one MUX talker is allowed per process.");
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Control transmission is handled in openavbIntfCtrlSendControl()
bool openavbIntfCtrlTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfCtrlRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfCtrlRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = NULL;

		pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
		if (pMediaQItem) {
			if (pMediaQItem->dataLen) {
				if (pPvtData->muxMode) {
					if (pCtrlMuxMediaQ) {
						if (!openavbIntfCtrlSendControl(pCtrlMuxMediaQ, pMediaQItem->pPubData, pMediaQItem->dataLen, 0)) {
							openavbMediaQTailPull(pMediaQ);
							AVB_LOG_ERROR("Failed to retransmit the control message.");
							return FALSE;
						}
						else {
							openavbMediaQTailPull(pMediaQ);
							return TRUE;
						}
					}
					else {
						openavbMediaQTailPull(pMediaQ);
						AVB_LOG_ERROR("Control mux talker not found.");
						return FALSE;
					}
				}
				else {
					if (pPvtData->openavbIntfCtrlReceiveControlFn) {
						pPvtData->openavbIntfCtrlReceiveControlFn(pPvtData->pTLHandle, pMediaQItem->pPubData, pMediaQItem->dataLen);
						openavbMediaQTailPull(pMediaQ);
						return TRUE;
					}
					else {
						openavbMediaQTailPull(pMediaQ);
						AVB_LOG_ERROR("Control receive callback not defined.");
						return FALSE;
					}

				}
			}
			openavbMediaQTailPull(pMediaQ);
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// This callback will be called when the stream is closing. 
void openavbIntfCtrlEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		// If this instance of interface owns the mux talker remove it now
		if (pPvtData->bOwnsCtrlMuxMediaQ) {
			pCtrlMuxMediaQ = NULL;
			pPvtData->bOwnsCtrlMuxMediaQ = FALSE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// General shutdown callback regardless if a talker or listener. Called once during openavbTLClose()
void openavbIntfCtrlGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Host side callback 
extern bool DLL_EXPORT openavbIntfCtrlRegisterReceiveControlCB(void *pIntfHandle, void *pTLHandle, openavb_intf_ctrl_receive_cb_t openavbIntfCtrlReceiveControlFn)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	media_q_t *pMediaQ = pIntfHandle;

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		pPvtData->openavbIntfCtrlReceiveControlFn = openavbIntfCtrlReceiveControlFn;
		pPvtData->pTLHandle = pTLHandle;

		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return TRUE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return FALSE;
}

extern bool DLL_EXPORT openavbIntfCtrlUnregisterReceiveControlCB(void *pIntfHandle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	media_q_t *pMediaQ = pIntfHandle;

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		pPvtData->openavbIntfCtrlReceiveControlFn = NULL;
		pPvtData->pTLHandle = NULL;

		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return TRUE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return FALSE;
}

// Host side callback 
extern bool DLL_EXPORT openavbIntfCtrlSendControl(void *pIntfHandle, U8 *pData, U32 dataLength, U32 usecDelay)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	media_q_t *pMediaQ = pIntfHandle;

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			if (pMediaQItem->itemSize >= dataLength) {
				memcpy(pMediaQItem->pPubData, pData, dataLength);
				pMediaQItem->dataLen = dataLength;
			}
			else {
				AVB_LOG_ERROR("Control data too large for media queue.");
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
				return FALSE;
			}

			openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
			openavbAvtpTimeAddUSec(pMediaQItem->pAvtpTime, usecDelay);
			openavbMediaQHeadPush(pMediaQ);
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return TRUE;
		}
		else {
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return FALSE;	// Media queue full
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// Main initialization entry point into the interface module
extern bool DLL_EXPORT openavbIntfCtrlInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfCtrlCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfCtrlGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfCtrlTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfCtrlTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfCtrlRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfCtrlRxCB;
		pIntfCB->intf_end_cb = openavbIntfCtrlEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfCtrlGenEndCB;

		openavbIntfHostCBList.register_receive_control_cb = openavbIntfCtrlRegisterReceiveControlCB;
		openavbIntfHostCBList.unregister_receive_control_cb = openavbIntfCtrlUnregisterReceiveControlCB;
		openavbIntfHostCBList.send_control_cb = openavbIntfCtrlSendControl;
		pIntfCB->intf_host_cb_list = (void *)&openavbIntfHostCBList;

		pPvtData->muxMode = FALSE;
		pPvtData->ignoreTimestamp = FALSE;
		pPvtData->openavbIntfCtrlReceiveControlFn = NULL;
		pPvtData->pTLHandle = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}

