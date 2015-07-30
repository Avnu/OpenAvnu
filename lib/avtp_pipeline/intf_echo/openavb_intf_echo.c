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
* MODULE SUMMARY : Echo interface module.
* 
* This interface module as a talker to push a configured string out. As
*  a listener it will echo the data to stdout. This is strickly for
*  testing purposes and is generally intented to work with the Pipe
*  mapping module.
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"Echo Interface"
#include "openavb_log_pub.h" 

typedef struct {
	/////////////
	// Config data
	/////////////
	// intf_nv_echo_string: sets a string that will be sent by the talker.
	char *pEchoString;
	
	// String repeat
	U32 echoStringRepeat;

	// calculated value from pEchoString.
	U32 echoStringLen;

	// Append an incrementing number to the string.
	bool increment;

	// Output locally at the talker.
	bool txLocalEcho;

	// No new line.
	bool noNewline;

	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	/////////////
	// Variable data
	/////////////
	// When increment is enable this is the counter
	U32 Counter;
} pvt_data_t;


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfEchoCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
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

		if (strcmp(name, "intf_nv_echo_string") == 0) {
			if (pPvtData->pEchoString)
				free(pPvtData->pEchoString);
			pPvtData->pEchoString = strdup(value);
			if (pPvtData->pEchoString) {
				pPvtData->echoStringLen = strlen(pPvtData->pEchoString);
			}
		}
		else if (strcmp(name, "intf_nv_echo_string_repeat") == 0) {
			pPvtData->echoStringRepeat = strtol(value, &pEnd, 10);

			// Repeat the string if needed
			if (pPvtData->echoStringRepeat > 1 && pPvtData->pEchoString) {
				char *pEchoStringRepeat = calloc(1, (pPvtData->echoStringLen * pPvtData->echoStringRepeat) + 1); 

				int i1;
				for (i1 = 0; i1 < pPvtData->echoStringRepeat; i1++) {
					strcat(pEchoStringRepeat, pPvtData->pEchoString);
				}

				free(pPvtData->pEchoString);
				pPvtData->pEchoString = pEchoStringRepeat;
				pPvtData->echoStringLen = strlen(pPvtData->pEchoString);
			}
		}
		else if (strcmp(name, "intf_nv_echo_increment") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->increment = (tmp == 1);
			}
		}
		else if (strcmp(name, "intf_nv_tx_local_echo") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->txLocalEcho = (tmp == 1);
			}
		}
		else if (strcmp(name, "intf_nv_echo_no_newline") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->noNewline = (tmp == 1);
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

void openavbIntfEchoGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfEchoTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		pPvtData->Counter = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfEchoTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			if (pMediaQItem->itemSize >= pPvtData->increment ? pPvtData->echoStringLen + 16 : pPvtData->echoStringLen) {
				if (pPvtData->increment) {
					int len = sprintf(pMediaQItem->pPubData, "%s %u", pPvtData->pEchoString, pPvtData->Counter++);
					pMediaQItem->dataLen = len;
				}
				else {
					memcpy(pMediaQItem->pPubData, pPvtData->pEchoString, pPvtData->echoStringLen);
					pMediaQItem->dataLen = pPvtData->echoStringLen;
				}
			}
			else {
				memcpy(pMediaQItem->pPubData, pPvtData->pEchoString, pMediaQItem->itemSize);
				pMediaQItem->dataLen = pMediaQItem->itemSize;
			}

			if (pPvtData->txLocalEcho) {
				if (pPvtData->noNewline)
					printf("%s ", (char *)pMediaQItem->pPubData); 
				else
					printf("%s\n\r", (char *)pMediaQItem->pPubData); 
			}

			openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
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

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfEchoRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		pPvtData->Counter = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfEchoRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {
		bool moreItems = TRUE;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		media_q_item_t *pMediaQItem = NULL;
		while (moreItems) {
			pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
			if (pMediaQItem) {
				if (pMediaQItem->dataLen) {
					if (pMediaQItem->itemSize > pMediaQItem->dataLen) {
						*((char*)pMediaQItem->pPubData + pMediaQItem->dataLen) = 0; //End string with 0
					}
					if (pPvtData->noNewline) {
						printf("%s ", (char *)pMediaQItem->pPubData); 
					}
					else {
						if (pPvtData->increment) {
							printf("%s : %u\n\r", (char *)pMediaQItem->pPubData, pPvtData->Counter++); 
						}
						else {
							printf("%s\n\r", (char *)pMediaQItem->pPubData); 
						}
					}
				}
				openavbMediaQTailPull(pMediaQ);
			}
			else {
				moreItems = FALSE;
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// This callback will be called when the stream is closing. 
void openavbIntfEchoEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// General shutdown callback regardless if a talker or listener. Called once during openavbTLClose()
void openavbIntfEchoGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (pPvtData->pEchoString) {
			free(pPvtData->pEchoString);
			pPvtData->pEchoString = NULL;
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern bool DLL_EXPORT openavbIntfEchoInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfEchoCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfEchoGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfEchoTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfEchoTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfEchoRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfEchoRxCB;
		pIntfCB->intf_end_cb = openavbIntfEchoEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfEchoGenEndCB;

		pPvtData->pEchoString = NULL;
		pPvtData->echoStringRepeat = 1; 
		pPvtData->echoStringLen = 0;
		pPvtData->increment = FALSE;
		pPvtData->txLocalEcho = FALSE;
		pPvtData->noNewline = FALSE;
		pPvtData->ignoreTimestamp = FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
