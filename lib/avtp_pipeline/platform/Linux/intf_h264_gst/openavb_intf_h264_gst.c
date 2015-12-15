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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtp/gstrtpbuffer.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"
#include "openavb_map_h264_pub.h"
#include "gst_al.h"

#define	AVB_LOG_COMPONENT	"H264 Interface"
#include "openavb_log_pub.h"

#define APPSINK_NAME "avbsink"
#define APPSRC_NAME "avbsrc"
#define RTP_PAYLOADER_NAME "avbrtppay"
#define PACKETS_PER_RX_CALL 20

#define NBUFS 256

typedef struct pvt_data_t
{
	char *pPipelineStr;

	bool ignoreTimestamp;

	GstElement       *pipe;
	GstAppSink       *appsink;
	GstAppSrc       *appsrc;

	U32 bufwr;
	U32 bufrd;
	U32 seq;
	GstAlBuf *rxBufs[NBUFS];
	bool asyncRx;
	bool blockingRx;

	gint			nWaiting;
} pvt_data_t;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfH264RtpGstCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("H264Rtp-gst cfgCB: no mediaQ!");
		return;
	}

	char *pEnd;
	long tmp;

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}

	pPvtData->asyncRx = FALSE;

	if (strcmp(name, "intf_nv_gst_pipeline") == 0)
	{
		if (pPvtData->pPipelineStr)
		{
			free(pPvtData->pPipelineStr);
		}
		pPvtData->pPipelineStr = strdup(value);
	}
	else if (strcmp(name, "intf_nv_async_rx") == 0)
	{
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1)
		{
			pPvtData->asyncRx = (tmp == 1);
		}
	}
	else if (strcmp(name, "intf_nv_blocking_rx") == 0)
	{
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1)
		{
			pPvtData->blockingRx = (tmp == 1);
		}
	}
	else if (strcmp(name, "intf_nv_ignore_timestamp") == 0)
	{
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1)
		{
			pPvtData->ignoreTimestamp = (tmp == 1);
		}
	}
}

void openavbIntfH264RtpGstGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("H264Rtp-gst initCB: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return;
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

static GstFlowReturn sinkNewBufferSample(GstAppSink *sink, gpointer pv)
{
	media_q_t *pMediaQ = (media_q_t *)pv;
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

	g_atomic_int_add(&pPvtData->nWaiting, 1);

	return GST_FLOW_OK;
}

static int openavbMediaQGetItemSize(media_q_t *pMediaQ)
{
	int itemSize = 1412;
	media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
	if (pMediaQItem)
	{
		itemSize = pMediaQItem->itemSize;
		openavbMediaQHeadUnlock(pMediaQ);
	}
	else
	{
		AVB_LOG_ERROR("pMediaQ item NULL in getMediaQItemSize");
	}

	return itemSize;
}

static void createTxPipeline(media_q_t *pMediaQ)
{
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	GError *error = NULL;
	pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
	if (error)
	{
		AVB_LOGF_ERROR("Unable to create pipeline: %s", error->message);
	}

	AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
	pPvtData->appsink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSINK_NAME));
	if (!pPvtData->appsink)
	{
		AVB_LOG_ERROR("Failed to find appsink element");
	}

	// Setup callback function to handle new buffers delivered to sink
	GstAppSinkCallbacks cbfns;
	memset(&cbfns, 0, sizeof(GstAppSinkCallbacks));

	gst_al_set_callback(&cbfns, sinkNewBufferSample);

	gst_app_sink_set_callbacks(pPvtData->appsink, &cbfns, (gpointer)(pMediaQ), NULL);

	//No limits for internal sink buffers. This may cause large memory consumption.
	g_object_set(pPvtData->appsink, "max-buffers", 0, "drop", 0, NULL);

	GstElement *rtpPayloader = gst_bin_get_by_name(GST_BIN(pPvtData->pipe), RTP_PAYLOADER_NAME);
	if (rtpPayloader)
	{
		g_object_set(rtpPayloader, "mtu", openavbMediaQGetItemSize(pMediaQ), NULL);
		gst_object_unref(rtpPayloader);
	}
	else
	{
		AVB_LOG_ERROR("Cannot set mtu on rtppayloader. Make sure that its name is avbrtppay in the pipeline.");
	}

	if (GST_STATE_CHANGE_FAILURE == gst_element_set_state(pPvtData->pipe, GST_STATE_PLAYING)) {
		AVB_LOG_ERROR("Failed to change pipeline state to PLAYING.");
	}
}

static void destroyPipeline(media_q_t *pMediaQ)
{
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

	if (pPvtData->pipe)
	{
		gst_element_set_state(pPvtData->pipe, GST_STATE_NULL);
		if (pPvtData->appsink)
		{
			gst_object_unref(pPvtData->appsink);
			pPvtData->appsink = NULL;
		}
		if (pPvtData->appsrc)
		{
			gst_object_unref(pPvtData->appsrc);
			pPvtData->appsrc = NULL;
		}
		gst_object_unref(pPvtData->pipe);
		pPvtData->pipe = NULL;
	}
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfH264RtpGstTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("H264Rtp-gst txinit: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}

	createTxPipeline(pMediaQ);

	AVB_TRACE_EXIT(AVB_TRACE_INTF);

	return;
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfH264RtpGstTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("No MediaQ in H264RtpGstTxCB");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return FALSE;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}

	if (gst_app_sink_is_eos(GST_APP_SINK(pPvtData->appsink))) {
		AVB_LOG_INFO("Rewinding stream...");
		destroyPipeline(pMediaQ);
		createTxPipeline(pMediaQ);
	}

	while (g_atomic_int_get(&pPvtData->nWaiting) > 0)
	{
		//Transmit data --BEGIN--
		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem)
		{
			U32 paySize = 0;

			GstAlBuf *txBuf = NULL;

			txBuf = gst_al_pull_rtp_buffer(GST_APP_SINK(pPvtData->appsink));

			if (!txBuf)
			{
				pMediaQItem->dataLen = 0;
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_LOG_ERROR("Gstreamer buffer pull problem");
				AVB_TRACE_EXIT(AVB_TRACE_INTF);
				return FALSE;
			}

			g_atomic_int_add(&pPvtData->nWaiting, -1);
			paySize = GST_AL_BUF_SIZE(txBuf);

			if(paySize > pMediaQItem->itemSize){

				AVB_LOGF_ERROR("PaySize (%d) exceeds pMediaQItem itemSize (%d).", paySize, pMediaQItem->itemSize);

				pMediaQItem->dataLen = 0;
				openavbMediaQHeadUnlock(pMediaQ);
				gst_al_rtp_buffer_unref(txBuf);

				return FALSE;
			}

			pMediaQItem->dataLen = paySize;
			memcpy(pMediaQItem->pPubData, GST_AL_BUF_DATA(txBuf), paySize);
			if (gst_al_rtp_buffer_get_marker(txBuf))
			{
				((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket = TRUE;
			}
			else
			{
				((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket = FALSE;
			}
			openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
			openavbMediaQHeadPush(pMediaQ);

			gst_al_rtp_buffer_unref(txBuf);
		}
		else
		{
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			//AVB_LOG_INFO("MediaQ full");
			return FALSE;	// Media queue full
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}

// Async stuff...
static pthread_t asyncRxThread;
static pthread_mutex_t asyncRxMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t asyncReadMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t asyncReadCond = PTHREAD_COND_INITIALIZER;
#define LOCK() pthread_mutex_lock(&asyncRxMutex)
#define UNLOCK() pthread_mutex_unlock(&asyncRxMutex)
static bool bAsyncRXStreaming;
static media_q_t *pAsyncRxMediaQ;
//static media_q_item_t *pAsyncRxMediaQItem;
//static bool bAsyncRXDoneWithItem;

static void *openavbIntfH264RtpGstRxThreadfn(void *pv)
{
	pvt_data_t *pPvtData;

	if (!pAsyncRxMediaQ)
	{
		AVB_LOG_ERROR("No async mediaQ");
		return 0;
	}
	pPvtData = pAsyncRxMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("No async RX private data.");
		return 0;
	}

	bAsyncRXStreaming = TRUE;
	while (bAsyncRXStreaming)
	{
		U32 bufwr = pPvtData->bufwr;
		U32 bufrd = pPvtData->bufrd;
		if (bufwr == bufrd)
		{
			pthread_mutex_lock(&asyncReadMutex);
			pthread_cond_wait(&asyncReadCond, &asyncReadMutex);
			pthread_mutex_unlock(&asyncReadMutex);
		}
		else if(bufwr > bufrd)
		{
			GstAlBuf *rxBuf = pPvtData->rxBufs[pPvtData->bufrd%NBUFS];
			if (rxBuf)
			{
				pPvtData->rxBufs[pPvtData->bufrd%NBUFS] = NULL;
				__sync_fetch_and_add(&pPvtData->bufrd, 1);
				GstFlowReturn ret = gst_al_push_rtp_buffer(GST_APP_SRC(pPvtData->appsrc), rxBuf);

				if (ret != GST_FLOW_OK)
				{
					AVB_LOGF_ERROR("Pushing buffer to appsrc failed with code %d", ret);
				}
			}
			else
			{
				AVB_LOGF_INFO("The Buf %p  %d skipped, OO!!", rxBuf, bufrd);
			}
		}
		else
		{
			AVB_LOGF_INFO("There is a BUG as bufwr=%d < bufrd=%d", bufwr, bufrd);
		}
	}
	return 0;
}
// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfH264RtpGstRxInitCB(media_q_t *pMediaQ)
{
	AVB_LOG_DEBUG("Rx Init callback.");
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("No MediaQ in H264RtpGstRxInitCB");
		return;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}

	GError *error = NULL;
	pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
	if (error)
	{
		AVB_LOGF_ERROR("Unable to create pipeline: %s", error->message);
	}

	AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
	pPvtData->appsrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSRC_NAME));
	if (!pPvtData->appsrc)
	{
		AVB_LOG_ERROR("Failed to find appsrc element");
	}

	// caps can be set from pipeline
	if (pPvtData->blockingRx)
	{
		AVB_LOG_DEBUG("Switching gstreamer into blocking mode");
		g_object_set(pPvtData->appsrc, "block", 1, NULL); // and now we have to do async rx :)
	}

	//FIXME: Check if state change was successful
	gst_element_set_state(pPvtData->pipe, GST_STATE_PLAYING);

	pPvtData->seq = 1;

	if (pPvtData->asyncRx)
	{
		pAsyncRxMediaQ = pMediaQ;
		int err = pthread_mutex_init(&asyncRxMutex, 0);
		if (err)
			AVB_LOG_ERROR("Mutex init failed");
		pthread_attr_t attr;
		struct sched_param param;
		pthread_attr_init(&attr);
		pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
		param.sched_priority = 0;
		pthread_attr_setschedparam(&attr, &param);
		pthread_create(&asyncRxThread, &attr, openavbIntfH264RtpGstRxThreadfn, NULL);
	}
}

// This callback is called when acting as a listener.
bool openavbIntfH264RtpGstRxCB(media_q_t *pMediaQ)
{
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("RxCB: no mediaQ!");
		return TRUE;
	}
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}

	bool moreSourcePackets = TRUE;

	while (moreSourcePackets)
	{
		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
        // there are no packets available or they are from the future
		if (!pMediaQItem)
		{
			moreSourcePackets = FALSE;
			continue;
		}
		if (!pMediaQItem->dataLen)
		{
			AVB_LOG_DEBUG("No dataLen");
			openavbMediaQTailPull(pMediaQ);
			continue;
		}
		if (pPvtData->asyncRx)
		{
			U32 bufwr = pPvtData->bufwr;
			U32 bufrd = pPvtData->bufrd;
			U32 mdif = bufwr - bufrd;
			if (mdif >= NBUFS)
			{
				openavbMediaQTailPull(pMediaQ);
				AVB_LOGF_INFO("Rx async queue full, dropping (%" PRIu32 " - %" PRIu32 " = %" PRIu32 ")", bufwr, bufrd, mdif);
				moreSourcePackets = FALSE;
				continue;
			}
		}
		GstAlBuf *rxBuf = gst_al_alloc_rtp_buffer(pMediaQItem->dataLen, 0,0);

		if (!rxBuf)
		{
			AVB_LOG_ERROR("gst_rtp_buffer_allocate failed!");
			openavbMediaQTailUnlock(pMediaQ);
			return FALSE;
		}
		memcpy(GST_AL_BUF_DATA(rxBuf), pMediaQItem->pPubData, pMediaQItem->dataLen);

		GST_AL_BUFFER_TIMESTAMP(rxBuf) = GST_CLOCK_TIME_NONE;
		GST_AL_BUFFER_DURATION(rxBuf) = GST_CLOCK_TIME_NONE;
		if ( ((media_q_item_map_h264_pub_data_t *)pMediaQItem->pPubMapData)->lastPacket )
		{
			gst_al_rtp_buffer_set_marker(rxBuf,TRUE);
		}

		gst_al_rtp_buffer_set_params(rxBuf, 5, 96, 2, pPvtData->seq++);

		if (pPvtData->asyncRx)
		{
			pPvtData->rxBufs[pPvtData->bufwr%NBUFS] = rxBuf;
			__sync_fetch_and_add(&pPvtData->bufwr, 1);
			pthread_mutex_lock(&asyncReadMutex);
			pthread_cond_signal(&asyncReadCond);
			pthread_mutex_unlock(&asyncReadMutex);
		}
		else
		{
			// appsrc manages this buffer at this point
			GstFlowReturn ret = gst_al_push_rtp_buffer(GST_APP_SRC(pPvtData->appsrc), rxBuf);
			if (ret != GST_FLOW_OK)
			{
				AVB_LOGF_ERROR("Pushing buffer to appsrc failed with code %d", ret);
			}
		}
		openavbMediaQTailPull(pMediaQ);
	}
	return TRUE;
}

// This callback will be called when the interface needs to be closed. All shutdown should
// occur in this function.
void openavbIntfH264RtpGstEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	bAsyncRXStreaming = FALSE;
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}
	destroyPipeline(pMediaQ);
	if (pPvtData->asyncRx)
	{
		pthread_mutex_lock(&asyncReadMutex);
		pthread_cond_signal(&asyncReadCond);
		pthread_mutex_unlock(&asyncReadMutex);
		pthread_join(asyncRxThread, NULL);
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfH264RtpGstGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfH264RtpGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("H264Rtp-gst GstInitialize: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return TRUE;
	}

	pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

	pIntfCB->intf_cfg_cb = openavbIntfH264RtpGstCfgCB;
	pIntfCB->intf_gen_init_cb = openavbIntfH264RtpGstGenInitCB;
	pIntfCB->intf_tx_init_cb = openavbIntfH264RtpGstTxInitCB;
	pIntfCB->intf_tx_cb = openavbIntfH264RtpGstTxCB;
	pIntfCB->intf_rx_init_cb = openavbIntfH264RtpGstRxInitCB;
	pIntfCB->intf_rx_cb = openavbIntfH264RtpGstRxCB;
	pIntfCB->intf_end_cb = openavbIntfH264RtpGstEndCB;
	pIntfCB->intf_gen_end_cb = openavbIntfH264RtpGstGenEndCB;

	pPvtData->ignoreTimestamp = FALSE;

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
