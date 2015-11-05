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
 *  Gstreamer abstraction layer implementation
 *  for version 1.0
 */
#include "gst_al.h"

void gst_al_set_callback(GstAppSinkCallbacks *cbfns, GstAlCallback callback)
{
	cbfns->new_sample = callback;
}

GstAlBuf* gst_al_pull_buffer(GstAppSink *sink)
{
	GstAlBuf *buf = g_new0(GstAlBuf,1);
	GstMemory *memory;
	GstBuffer * buffer;
	GstSample *sample = gst_app_sink_pull_sample(sink);
	buf->m_sample = sample;
	if(sample)
	{
		buffer = gst_sample_get_buffer(sample);
		buf->m_buffer = buffer;
		if(buffer)
		{
			memory = gst_buffer_get_memory(buffer, 0);
			buf->m_memory = memory;
			if(memory)
			{
				GstMapInfo *info = &buf->m_info;
				if(gst_memory_map(memory, info, GST_MAP_READ))
				{
					buf->m_dptr = info->data;
					buf->m_dlen = info->size;
					goto pull_success;
				}
				gst_memory_unref(memory);
			}
		}
		gst_sample_unref(sample);
	}
	g_free(buf);
	buf = NULL;
pull_success:
	return buf;
}

GstAlBuf* gst_al_alloc_buffer(gint len)
{
	GstAlBuf *buf = g_new0(GstAlBuf,1);
	GstMemory *memory;
	GstBuffer *buffer = gst_buffer_new_and_alloc(len);
	buf->m_buffer = buffer;
	if(buffer)
	{
		memory = gst_buffer_get_memory(buffer, 0);
		buf->m_memory = memory;
		if(memory)
		{
			GstMapInfo *info = &buf->m_info;
			if(TRUE == gst_memory_map(memory, info, GST_MAP_WRITE))
			{
				buf->m_dptr = info->data;
				buf->m_dlen = info->size;
				goto alloc_success;
			}
			gst_memory_unref(memory);
		}
		gst_buffer_unref(buffer);
	}
	g_free(buf);
	buf = NULL;
alloc_success:
	return buf;
}

GstFlowReturn gst_al_push_buffer(GstAppSrc *src, GstAlBuf *buf)
{
	GstFlowReturn gstret = gst_app_src_push_buffer(src, buf->m_buffer);
	gst_memory_unmap(buf->m_memory, &buf->m_info);
	gst_memory_unref(buf->m_memory);
	g_free(buf);
	return gstret;
}

void gst_al_buffer_unref(GstAlBuf *buf)
{
	gst_memory_unmap(buf->m_memory, &buf->m_info);
	gst_memory_unref(buf->m_memory);
	gst_sample_unref(buf->m_sample);
	g_free(buf);
}

GstAlBuf* gst_al_pull_rtp_buffer(GstAppSink *sink)
{

	GstAlBuf *buf = g_new0(GstAlBuf,1);
	GstBuffer *buffer;
	GstSample *sample = gst_app_sink_pull_sample(sink);
	buf->m_sample = sample;

	if(sample)
	{
		buffer = gst_sample_get_buffer(sample);
		buf->m_buffer = buffer;

		if(buffer)
		{
			GstRTPBuffer *rtpbuf = &buf->m_rtpbuf;
			if( gst_rtp_buffer_map(buffer, GST_MAP_READ, rtpbuf))
			{
				buf->m_dptr = gst_rtp_buffer_get_payload(rtpbuf);
				buf->m_dlen = gst_rtp_buffer_get_payload_len(rtpbuf);
				goto pull_rtp_success;
			}
		}
		gst_sample_unref(sample);
	}
	g_free(buf);
	buf = NULL;
pull_rtp_success:
	return buf;
}

GstAlBuf* gst_al_alloc_rtp_buffer(guint payload_len,
                                  guint8 pad_len, guint8 csrc_count)
{
	GstAlBuf *buf = g_new0(GstAlBuf, 1);
	GstBuffer *buffer = gst_rtp_buffer_new_allocate(payload_len, pad_len, csrc_count);
	buf->m_buffer = buffer;
	if(buffer)
	{
		GstRTPBuffer *rtpbuf = &buf->m_rtpbuf;
		if( gst_rtp_buffer_map(buffer, GST_MAP_WRITE, rtpbuf))
		{
			buf->m_dptr = gst_rtp_buffer_get_payload(rtpbuf);
			buf->m_dlen = gst_rtp_buffer_get_payload_len(rtpbuf);
			goto alloc_rtp_success;
		}
		gst_buffer_unref(buffer);
	}
	g_free(buf);
	buf = NULL;
alloc_rtp_success:
	return buf;
}

gboolean gst_al_rtp_buffer_get_marker(GstAlBuf *buf)
{
	return gst_rtp_buffer_get_marker(&buf->m_rtpbuf);
}

void gst_al_rtp_buffer_set_marker(GstAlBuf *buf, gboolean mark)
{
	gst_rtp_buffer_set_marker(&buf->m_rtpbuf, mark);
}

void gst_al_rtp_buffer_set_params(GstAlBuf *buf, gint ssrc,
                                  gint payload_type, gint version,
                                  gint sequence)
{
	GstRTPBuffer *rtpbuf = &buf->m_rtpbuf;
	gst_rtp_buffer_set_ssrc(rtpbuf, ssrc);
	gst_rtp_buffer_set_payload_type(rtpbuf, payload_type);
	gst_rtp_buffer_set_version(rtpbuf, version);
	gst_rtp_buffer_set_seq(rtpbuf, sequence);
}

GstFlowReturn gst_al_push_rtp_buffer(GstAppSrc *src, GstAlBuf *buf)
{
	gst_rtp_buffer_unmap(&buf->m_rtpbuf);
	GstFlowReturn gstret = gst_app_src_push_buffer(src, buf->m_buffer);
	g_free(buf);
	return gstret;
}

void gst_al_rtp_buffer_unref(GstAlBuf *buf)
{
	gst_rtp_buffer_unmap(&buf->m_rtpbuf);
	gst_sample_unref(buf->m_sample);
	g_free(buf);
}
