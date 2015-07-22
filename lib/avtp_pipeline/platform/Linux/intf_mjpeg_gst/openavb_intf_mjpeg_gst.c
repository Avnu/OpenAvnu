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

/*
* MODULE SUMMARY : MJPEG File interface module.
*/

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
#include "openavb_map_mjpeg_pub.h"

#define	AVB_LOG_COMPONENT	"MJPEG Interface"
#include "openavb_log_pub.h" 

#define APPSINK_NAME "avbsink"
#define APPSRC_NAME "avbsrc"
#define PACKETS_PER_RX_CALL 20

#define NBUFS 256

typedef struct {
	char *pPipelineStr;

	bool ignoreTimestamp;

	GstElement       *pipe;
	GstElement       *appsink;
	GstElement       *appsrc;

	U32 bufwr;
	U32 bufrd;
	U32 seq;
	GstBuffer *rxBufs[NBUFS];
	bool asyncRx;
	bool blockingRx;

} pvt_data_t;

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfMjpegGstCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	if (!pMediaQ) 
	{
		AVB_LOG_DEBUG("mjpeg-gst cfgCB: no mediaQ!");
		return;
	}

	char *pEnd;
	long tmp;

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
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
	else if (strcmp(name, "intf_nv_async_rx") == 0) {
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1) {
			pPvtData->asyncRx = (tmp == 1);
		}
	}
	else if (strcmp(name, "intf_nv_blocking_rx") == 0) {
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1) {
			pPvtData->blockingRx = (tmp == 1);
		}
	}
	else if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && tmp == 1) {
			pPvtData->ignoreTimestamp = (tmp == 1);
		}
	}
}

void openavbIntfMjpegGstGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (!pMediaQ) 
	{
		AVB_LOG_DEBUG("mjpeg-gst initCB: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return;
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfMjpegGstTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (!pMediaQ) 
	{
		AVB_LOG_DEBUG("mjpeg-gst txinit: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}

	GError *error = NULL;
	pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
	if (error) {
		AVB_LOGF_ERROR("Unable to create pipeline: %s", error->message);
	}

	AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
	pPvtData->appsink = gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSINK_NAME);
	if (!pPvtData->appsink) {
		AVB_LOG_ERROR("Failed to find appsink element");
	}
	GstCaps *sinkCaps = gst_caps_from_string("application/x-rtp");
	//No limits for internal sink buffers. This may cause large memory consumption.
	g_object_set(pPvtData->appsink, "max-buffers", 0, "drop", 0, "caps", sinkCaps, NULL);
	gst_caps_unref(sinkCaps);
	//FIXME: Check if state change was successful
	gst_element_set_state(pPvtData->pipe, GST_STATE_PLAYING);

	AVB_TRACE_EXIT(AVB_TRACE_INTF);

	return;
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfMjpegGstTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("No MediaQ in MjpegGstTxCB");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return FALSE;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}

	U32 paySize = 0;
	GstBuffer *txBuf = gst_app_sink_pull_buffer(GST_APP_SINK(pPvtData->appsink));

	if (!txBuf) {
		AVB_LOG_ERROR("Gstreamer buffer pull problem");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return FALSE;
	}
	paySize = gst_rtp_buffer_get_payload_len(txBuf);

	//Transmit data --BEGIN--
	media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
	if (pMediaQItem) {
		pMediaQItem->dataLen = paySize;
		memcpy(pMediaQItem->pPubData, gst_rtp_buffer_get_payload(txBuf), paySize);
		if (gst_rtp_buffer_get_marker(txBuf))
		{
			((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment = TRUE;
		}
		else
		{
			((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment = FALSE;
		}
		openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
		openavbMediaQHeadPush(pMediaQ);
		gst_buffer_unref(txBuf);
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		return TRUE;
	}
	else {
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		AVB_LOG_INFO("GStreamer returned NULL buffer, pipeline stopped");
		return FALSE;	// Media queue full
	}
	// never here....
	gst_buffer_unref(txBuf);
	openavbMediaQHeadUnlock(pMediaQ);
	pMediaQItem->dataLen = 0;
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

static void *openavbIntfMjpegGstRxThreadfn(void *pv)
{
	pvt_data_t *pPvtData;

	if (!pAsyncRxMediaQ)
	{
		AVB_LOG_ERROR("No async mediaQ");
		return 0;
	}
	pPvtData = pAsyncRxMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("No async RX private data.");
		return 0;
	}

	bAsyncRXStreaming = TRUE;
	while (bAsyncRXStreaming)
	{
		LOCK();
		if (pPvtData->bufwr <= pPvtData->bufrd)
		{
			UNLOCK();
			pthread_mutex_lock(&asyncReadMutex);
		 	pthread_cond_wait(&asyncReadCond, &asyncReadMutex);
			pthread_mutex_unlock(&asyncReadMutex);
		}
		else
		{
			UNLOCK();
		}
		LOCK();
		GstBuffer *rxBuf = pPvtData->rxBufs[pPvtData->bufrd%NBUFS];
		pPvtData->bufrd++;
		UNLOCK();
		if (rxBuf && (pPvtData->bufwr - pPvtData->bufrd) < NBUFS)
		{
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(pPvtData->appsrc), rxBuf);

			if (ret != GST_FLOW_OK) 
			{
				AVB_LOGF_ERROR("Pushing buffer to appsrc failed with code %d", ret);
			}
		}
		else
		{
			AVB_LOGF_INFO("Th Buf %x  %d skipped, OO!!", rxBuf, pPvtData->bufrd);
//			gst_buffer_unref(rxBuf);
		}
	}
	return 0;
}
// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfMjpegGstRxInitCB(media_q_t *pMediaQ) 
{
	AVB_LOG_DEBUG("Rx Init callback.");
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("No MediaQ in MjpegGstRxInitCB");
		return;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}

	GError *error = NULL;
	pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
//	pPvtData->pipe = gst_parse_launch("appsrc name=avbsrc ! application/x-rtp,media=video,clock-rate=90000,encoding-name=JPEG,payload=96,ssrc=5,clock-base=1,seqnum-base=1 ! rtpjpegdepay ! jpegdec ! ffmpegcolorspace ! omx_scaler ! omx_ctrl display-mode=OMX_DC_MODE_1080P_60 ! omx_videosink sync=false", &error);
//	pPvtData->pipe = gst_parse_launch("appsrc name=avbsrc ! application/x-rtp,media=video,clock-rate=90000,encoding-name=JPEG,payload=96,ssrc=5,clock-base=1,seqnum-base=1 ! rtpjpegdepay ! jpegdec ! autovideosink", &error);
	if (error) {
		AVB_LOGF_ERROR("Unable to create pipeline: %s", error->message);
	}

	AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
	pPvtData->appsrc = gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSRC_NAME);
	if (!pPvtData->appsrc) {
		AVB_LOG_ERROR("Failed to find appsrc element");
	}
	GstCaps *srcCaps = gst_caps_from_string("application/x-rtp");
	if (!srcCaps)
	{
		AVB_LOGF_DEBUG("Caps=%x",srcCaps);
	}
	gst_app_src_set_caps ((GstAppSrc *)pPvtData->appsrc, srcCaps);
	gst_caps_unref(srcCaps);
	gst_app_src_set_max_bytes((GstAppSrc *)pPvtData->appsrc, 3000);

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
		pthread_create(&asyncRxThread, &attr, openavbIntfMjpegGstRxThreadfn, NULL);
	}
}

// This callback is called when acting as a listener.
bool openavbIntfMjpegGstRxCB(media_q_t *pMediaQ) 
{
	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("RxCB: no mediaQ!");
		return TRUE;
	}
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}

	bool moreSourcePackets = TRUE;

		while (moreSourcePackets) 
		{
			media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
		if (!pMediaQItem)
			{
				moreSourcePackets = FALSE;
			openavbMediaQTailPull(pMediaQ);
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
					LOCK();
					unsigned long mdif = pPvtData->bufwr - pPvtData->bufrd;
					if (pPvtData->bufwr > pPvtData->bufrd && mdif >= NBUFS)
					{
						openavbMediaQTailPull(pMediaQ);
						AVB_LOGF_INFO("Rx async queue full, dropping (%lu - %lu = %lu)",pPvtData->bufwr,pPvtData->bufrd,mdif);
						UNLOCK();
						continue;
					}
						UNLOCK();
					}
		GstBuffer *rxBuf = gst_rtp_buffer_new_allocate (pMediaQItem->dataLen, 0,0);
		if (!rxBuf)
		{
			AVB_LOG_ERROR("gst_rtp_buffer_allocate failed!");
			openavbMediaQTailUnlock(pMediaQ);
			return FALSE;
		}
		memcpy(gst_rtp_buffer_get_payload(rxBuf), pMediaQItem->pPubData, pMediaQItem->dataLen);

					GST_BUFFER_TIMESTAMP(rxBuf) = GST_CLOCK_TIME_NONE;
					GST_BUFFER_DURATION(rxBuf) = -1;

		if ( ((media_q_item_map_mjpeg_pub_data_t *)pMediaQItem->pPubMapData)->lastFragment )
		{
			gst_rtp_buffer_set_marker(rxBuf,TRUE);
		}

		gst_rtp_buffer_set_ssrc(rxBuf,5);
		gst_rtp_buffer_set_payload_type(rxBuf,96);
		gst_rtp_buffer_set_version(rxBuf,2);
		gst_rtp_buffer_set_seq(rxBuf,pPvtData->seq++);
		
		if (pPvtData->asyncRx)
		{
					LOCK();
					pPvtData->rxBufs[pPvtData->bufwr%NBUFS] = rxBuf;
					pPvtData->bufwr++;
					UNLOCK();
					if (pPvtData->bufwr > pPvtData->bufrd)
					{
						pthread_mutex_lock(&asyncReadMutex);
						pthread_cond_signal(&asyncReadCond);
						pthread_mutex_unlock(&asyncReadMutex);
			}
				}
				else
				{
			GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(pPvtData->appsrc), rxBuf);
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
void openavbIntfMjpegGstEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	bAsyncRXStreaming = FALSE;
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData) {
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return;
	}
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
	if (pPvtData->asyncRx)
	{
		pthread_mutex_lock(&asyncReadMutex);
		pthread_cond_signal(&asyncReadCond);
		pthread_mutex_unlock(&asyncReadMutex);
		pthread_join(asyncRxThread, NULL);
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfMjpegGstGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfMjpegGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (!pMediaQ)
	{
		AVB_LOG_DEBUG("mjpeg-gst GstInitialize: no mediaQ!");
		AVB_TRACE_EXIT(AVB_TRACE_INTF);
		return TRUE;
	}

	pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

	pIntfCB->intf_cfg_cb = openavbIntfMjpegGstCfgCB;
	pIntfCB->intf_gen_init_cb = openavbIntfMjpegGstGenInitCB;
	pIntfCB->intf_tx_init_cb = openavbIntfMjpegGstTxInitCB;
	pIntfCB->intf_tx_cb = openavbIntfMjpegGstTxCB;
	pIntfCB->intf_rx_init_cb = openavbIntfMjpegGstRxInitCB;
	pIntfCB->intf_rx_cb = openavbIntfMjpegGstRxCB;
	pIntfCB->intf_end_cb = openavbIntfMjpegGstEndCB;
	pIntfCB->intf_gen_end_cb = openavbIntfMjpegGstGenEndCB;

	pPvtData->ignoreTimestamp = FALSE;

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
