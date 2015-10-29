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
 * MODULE SUMMARY : Mpeg2 TS Gstreamer interface module.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include "openavb_types_pub.h"
#include "openavb_log_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"
#include "gst_al.h"

#define APPSINK_NAME "avbsink"
#define APPSRC_NAME "avbsrc"

typedef struct
{
	/////////////
	// Config data
	/////////////
	char *pPipelineStr;

	bool ignoreTimestamp;

	/////////////
	// Variable data
	/////////////
	GstElement		*pipe;
	GstBus			*bus;
	GstAppSink		*appsink;
	GstAppSrc		*appsrc;

	// talker: number of gstreamer buffers waiting to be pulled
	gint			nWaiting;
	// listener: whether gstreamer wants more pushed data now
	bool			srcPaused;

} pvt_data_t;


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfMpeg2tsGstCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ)
	{
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData)
		{
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		char *pEnd;
		unsigned long tmp;
		bool nameOK = TRUE, valueOK = FALSE;

		if (strcmp(name, "intf_nv_gst_pipeline") == 0)
		{
			if (pPvtData->pPipelineStr)
				free(pPvtData->pPipelineStr);
			pPvtData->pPipelineStr = strdup(value);
			valueOK = (value != NULL && strlen(value) > 0);
		}
		else if (strcmp(name, "intf_nv_ignore_timestamp") == 0)
		{
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && pEnd != value && (tmp == 0 || tmp == 1))
			{
				pPvtData->ignoreTimestamp = (tmp == 1);
				valueOK = TRUE;
			}
		}
		else
		{
			AVB_LOGF_WARNING("Unknown configuration item: %s", name);
			nameOK = FALSE;
		}

		if (nameOK && !valueOK)
		{
			AVB_LOGF_WARNING("Bad value for configuration item: %s = %s", name, value);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
}

void openavbIntfMpeg2tsGstGenInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
}

static gboolean
bus_message(GstBus *bus, GstMessage *message, void *pv)
{
	switch (GST_MESSAGE_TYPE(message))
	{
		case GST_MESSAGE_ERROR:
		{
			GError *err = NULL;
			gchar *dbg_info = NULL;

			gst_message_parse_error(message, &err, &dbg_info);
			AVB_LOGF_ERROR("GStreamer ERROR message from element %s: %s",
			               GST_OBJECT_NAME(message->src),
			               err->message);
			AVB_LOGF_ERROR("Additional info: %s\n", (dbg_info) ? dbg_info : "none");
			g_error_free(err);
			g_free(dbg_info);
			break;
		}
		case GST_MESSAGE_WARNING:
		{
			GError *err = NULL;
			gchar *dbg_info = NULL;

			gst_message_parse_warning(message, &err, &dbg_info);
			AVB_LOGF_WARNING("GStreamer WARNING message from element %s: %s",
			                 GST_OBJECT_NAME(message->src),
			                 err->message);
			AVB_LOGF_WARNING("Additional info: %s\n", (dbg_info) ? dbg_info : "none");
			g_error_free(err);
			g_free(dbg_info);
			break;
		}
		case GST_MESSAGE_INFO:
		{
			GError *err = NULL;
			gchar *dbg_info = NULL;

			gst_message_parse_info(message, &err, &dbg_info);
			AVB_LOGF_ERROR("GStreamer INFO message from element %s: %s",
			               GST_OBJECT_NAME(message->src),
			               err->message);
			AVB_LOGF_ERROR("Additional info: %s\n", (dbg_info) ? dbg_info : "none");
			g_error_free(err);
			g_free(dbg_info);
			break;
		}
		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state;
			gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
			AVB_LOGF_DEBUG("Element %s changed state from %s to %s",
			               GST_OBJECT_NAME(message->src),
			               gst_element_state_get_name(old_state),
			               gst_element_state_get_name(new_state));
			break;
		}
		case GST_MESSAGE_STREAM_STATUS:
		{
			// not so valuable
			break;
		}
		case GST_MESSAGE_EOS:
			AVB_LOG_INFO("EOS received");
			break;
		default:
			AVB_LOGF_INFO("GStreamer '%s' message from element %s",
			              gst_message_type_get_name(GST_MESSAGE_TYPE(message)),
			              GST_OBJECT_NAME(message->src));
			break;
	}

	return TRUE;
}

// This callback triggers when appsrc needs data.
static void srcStartFeed (GstAppSrc *source, guint size, gpointer pv)
{
	media_q_t *pMediaQ = (media_q_t *)pv;
	assert(pMediaQ);
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	assert(pPvtData);
	if (pPvtData->srcPaused)
	{
//		AVB_LOG_INFO("Restart");
		pPvtData->srcPaused = FALSE;
	}
}

// This callback triggers when appsrc has enough data and we can stop sending.
static void srcStopFeed (GstAppSrc *source, gpointer pv)
{
	media_q_t *pMediaQ = (media_q_t *)pv;
	assert(pMediaQ);
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	assert(pPvtData);
//	AVB_LOG_INFO("Pause");
	pPvtData->srcPaused = TRUE;
}

static GstFlowReturn sinkNewBufferSample(GstAppSink *sink, gpointer pv)
{
	assert(pv);
	media_q_t *pMediaQ = (media_q_t *)pv;
	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	assert(pPvtData);

	g_atomic_int_add(&pPvtData->nWaiting, 1);

	return GST_FLOW_OK;
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfMpeg2tsGstTxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ)
	{
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData)
		{
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		pPvtData->pipe = (GstElement*)NULL;
		pPvtData->appsink = (GstAppSink*)NULL;
		pPvtData->appsrc = (GstAppSrc*)NULL;
		pPvtData->bus = (GstBus*)NULL;
		pPvtData->nWaiting = 0;

		GError *error = NULL;
		pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
		if (error)
		{
			AVB_LOGF_ERROR("Error creating pipeline: %s", error->message);
			return;
		}

		AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
		pPvtData->appsink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSINK_NAME));
		if (!pPvtData->appsink)
		{
			AVB_LOG_ERROR("Failed to find appsink element");
			return;
		}

		// create bus
		pPvtData->bus = gst_pipeline_get_bus(GST_PIPELINE(pPvtData->pipe));
		if (!pPvtData->bus)
		{
			AVB_LOG_ERROR("Failed to create bus");
			return;
		}

		/* add callback for bus messages */
		gst_bus_add_watch(pPvtData->bus, (GstBusFunc)bus_message, pMediaQ);

		// Setup callback function to handle new buffers delivered to sink
		GstAppSinkCallbacks cbfns;
		memset(&cbfns, 0, sizeof(GstAppSinkCallbacks));

		gst_al_set_callback(&cbfns, sinkNewBufferSample);

		gst_app_sink_set_callbacks(pPvtData->appsink, &cbfns, (gpointer)(pMediaQ), NULL);

		// Set most capabilities in pipeline (config), not code

		// Don't drop buffers
		g_object_set(pPvtData->appsink, "drop", 0, NULL);

		// Start playing
		gst_element_set_state(pPvtData->pipe, GST_STATE_PLAYING);
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
}

bool openavbIntfMpeg2tsGstTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (!pMediaQ)
	{
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		return FALSE;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}

	if (!pPvtData->appsink)
	{
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		return FALSE;
	}

	media_q_item_t *pMediaQItem;
	GstAlBuf *txBuf;

	while (g_atomic_int_get(&pPvtData->nWaiting) > 0)
	{

		// Get a mediaQItem to hold the buffered data
		pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (!pMediaQItem)
		{
			IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full");
			break;
		}

		/* Retrieve the buffer
		 */
		txBuf = gst_al_pull_buffer(pPvtData->appsink);

		if (txBuf)
		{
			g_atomic_int_add(&pPvtData->nWaiting, -1);
			if ( GST_AL_BUF_SIZE(txBuf) > pMediaQItem->itemSize )
			{
				AVB_LOGF_ERROR("GStreamer buffer too large (size=%d) for mediaQ item (dataLen=%d)",
				               GST_AL_BUF_SIZE(txBuf), pMediaQItem->itemSize);
				pMediaQItem->dataLen = 0;
				openavbMediaQHeadUnlock(pMediaQ);
			}
			else
			{
				memcpy(pMediaQItem->pPubData, GST_AL_BUF_DATA(txBuf), GST_AL_BUF_SIZE(txBuf));
				pMediaQItem->dataLen = GST_AL_BUF_SIZE(txBuf);
				openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
				openavbMediaQHeadPush(pMediaQ);
			}
			gst_al_buffer_unref(txBuf);
		}
		else
		{
			AVB_LOG_ERROR("GStreamer buffer pull failed");
			// assume the pipeline is empty
			g_atomic_int_set(&pPvtData->nWaiting, 0);
			// abandon the mediaq item
			pMediaQItem->dataLen = 0;
			openavbMediaQHeadUnlock(pMediaQ);
			// and get out
			break;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}


// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfMpeg2tsGstRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ)
	{
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData)
		{
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		pPvtData->pipe = (GstElement*)NULL;
		pPvtData->appsink = (GstAppSink*)NULL;
		pPvtData->appsrc = (GstAppSrc*)NULL;
		pPvtData->bus = (GstBus*)NULL;
		pPvtData->srcPaused = FALSE;

		GError *error = NULL;
		pPvtData->pipe = gst_parse_launch(pPvtData->pPipelineStr, &error);
		if (error)
		{
			AVB_LOGF_ERROR("Error creating pipeline: %s", error->message);
			return;
		}

		AVB_LOGF_INFO("Pipeline: %s", pPvtData->pPipelineStr);
		pPvtData->appsrc = GST_APP_SRC(gst_bin_get_by_name(GST_BIN(pPvtData->pipe), APPSRC_NAME));
		if (!pPvtData->appsrc)
		{
			AVB_LOG_ERROR("Failed to find appsrc element");
			return;
		}

		// Make appsrc non-blocking
		g_object_set(G_OBJECT(pPvtData->appsrc), "block", FALSE, NULL);

		// create bus
		pPvtData->bus = gst_pipeline_get_bus(GST_PIPELINE(pPvtData->pipe));
		if (!pPvtData->bus)
		{
			AVB_LOG_ERROR("Failed to create bus");
			return;
		}

		/* add callback for bus messages */
		gst_bus_add_watch(pPvtData->bus, (GstBusFunc)bus_message, pMediaQ);

		// Setup callback function to handle request from src to pause/start data flow
		GstAppSrcCallbacks cbfns;
		memset(&cbfns, 0, sizeof(GstAppSrcCallbacks));
		cbfns.enough_data = srcStopFeed;
		cbfns.need_data = srcStartFeed;
		gst_app_src_set_callbacks(pPvtData->appsrc, &cbfns, (gpointer)(pMediaQ), NULL);

		// Set most capabilities in pipeline (config), not code

		// Don't block
		g_object_set(pPvtData->appsrc, "block", 0, NULL);

		// PLAY
		gst_element_set_state(pPvtData->pipe, GST_STATE_PLAYING);
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
}

// This callback is called when acting as a listener.
bool openavbIntfMpeg2tsGstRxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (!pMediaQ)
	{
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		return FALSE;
	}

	pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
	if (!pPvtData)
	{
		AVB_LOG_ERROR("Private interface module data not allocated.");
		return FALSE;
	}
	if (!pPvtData->appsrc)
	{
		AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
		return FALSE;
	}

	bool moreData = TRUE;
	bool retval = TRUE;

	while (moreData)
	{
		media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
		if (pMediaQItem)
		{
			unsigned long len = pMediaQItem->dataLen;
			if (len > 0)
			{
				GstAlBuf *rxBuf = gst_al_alloc_buffer(len);

				if (rxBuf)
				{
					GST_AL_BUFFER_TIMESTAMP(rxBuf) = GST_CLOCK_TIME_NONE;
					GST_AL_BUFFER_DURATION(rxBuf) = GST_CLOCK_TIME_NONE;

					memcpy(GST_AL_BUF_DATA(rxBuf), pMediaQItem->pPubData, GST_AL_BUF_SIZE(rxBuf));

					GstFlowReturn gstret = gst_al_push_buffer(GST_APP_SRC(pPvtData->appsrc), rxBuf);
					if (gstret != GST_FLOW_OK)
					{
						AVB_LOGF_ERROR("Pushing buffer to gstreamer failed: %d", gstret);
						retval = moreData = FALSE;
					}
				}
				else
				{
					AVB_LOG_ERROR("Failed to get gstreamer buffer");
					retval = moreData = FALSE;
				}
			}
			openavbMediaQTailPull(pMediaQ);
		}
		else
		{
			moreData = FALSE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return retval;
}

// This callback will be called when the interface needs to be closed. All shutdown should
// occur in this function.
void openavbIntfMpeg2tsGstEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ)
	{
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData)
		{
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (pPvtData->pipe)
		{
			gst_element_set_state(GST_ELEMENT(pPvtData->pipe), GST_STATE_NULL);
			if (pPvtData->bus)
			{
				gst_object_unref(pPvtData->bus);
				pPvtData->bus = NULL;
			}
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

	// Media Queue destroy cleans up all of our allocated data.
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
}

void openavbIntfMpeg2tsGstGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfMpeg2tsGstInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ)
	{
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo)
		{
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		//pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfMpeg2tsGstCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfMpeg2tsGstGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfMpeg2tsGstTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfMpeg2tsGstTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfMpeg2tsGstRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfMpeg2tsGstRxCB;
		pIntfCB->intf_end_cb = openavbIntfMpeg2tsGstEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfMpeg2tsGstGenEndCB;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
