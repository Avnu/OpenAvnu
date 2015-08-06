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
 *  for version 0.10
 */

#include "gst_al.h"

void gst_al_set_callback(GstAppSinkCallbacks *cbfns, GstAlCallback callback)
{
	cbfns->new_buffer = callback;
}

GstAlBuf* gst_al_pull_buffer(GstAppSink *sink)
{
	GstAlBuf *buf = g_new(GstAlBuf,1);
	buf->m_buffer = gst_app_sink_pull_buffer(sink);
	buf->m_dptr = GST_BUFFER_DATA(buf->m_buffer);
	buf->m_dlen = GST_BUFFER_SIZE(buf->m_buffer);
	return buf;
}

GstAlBuf* gst_al_alloc_buffer(gint len)
{
	GstAlBuf *buf = g_new(GstAlBuf,1);
	buf->m_buffer = gst_buffer_new_and_alloc(len);
	buf->m_dptr = GST_BUFFER_DATA(buf->m_buffer);
	buf->m_dlen = GST_BUFFER_SIZE(buf->m_buffer);
	return buf;
}

GstFlowReturn gst_al_push_buffer(GstAppSrc *src, GstAlBuf *buf)
{
	GstFlowReturn gstret = gst_app_src_push_buffer(src, buf->m_buffer);
	g_free(buf);
	return gstret;
}

void gst_al_buffer_unref(GstAlBuf *buf)
{
	gst_buffer_unref(buf->m_buffer);
	g_free(buf);
}

GstAlBuf* gst_al_pull_rtp_buffer(GstAppSink *sink)
{
	GstAlBuf *buf = g_new(GstAlBuf,1);
	buf->m_buffer = gst_app_sink_pull_buffer(sink);
	buf->m_dptr = gst_rtp_buffer_get_payload(buf->m_buffer);
	buf->m_dlen = gst_rtp_buffer_get_payload_len(buf->m_buffer);
	return buf;
}

gboolean gst_al_rtp_buffer_get_marker(GstAlBuf *buf)
{
	return gst_rtp_buffer_get_marker(buf->m_buffer);
}

void gst_al_rtp_buffer_set_marker(GstAlBuf *buf, gboolean mark)
{
	gst_rtp_buffer_set_marker(buf->m_buffer, mark);
}

void gst_al_rtp_buffer_set_params(GstAlBuf *buf, gint ssrc,
                                  gint payload_type, gint version, gint sequence)
{
	GstBuffer *buffer = buf->m_buffer;
	gst_rtp_buffer_set_ssrc(buffer, ssrc);
	gst_rtp_buffer_set_payload_type(buffer, payload_type);
	gst_rtp_buffer_set_version(buffer, version);
	gst_rtp_buffer_set_seq(buffer, sequence);
}

GstFlowReturn gst_al_push_rtp_buffer(GstAppSrc *src, GstAlBuf *buf)
{
	GstFlowReturn gstret = gst_app_src_push_buffer(src, buf->m_buffer);
	g_free(buf);
	return gstret;
}

GstAlBuf* gst_al_alloc_rtp_buffer(guint payload_len, guint8 pad_len, guint8 csrc_count)
{
	GstAlBuf *buf = g_new(GstAlBuf,1);
	buf->m_buffer = gst_rtp_buffer_new_allocate(payload_len, pad_len, csrc_count);
	buf->m_dptr = gst_rtp_buffer_get_payload(buf->m_buffer);
	buf->m_dlen = gst_rtp_buffer_get_payload_len(buf->m_buffer);
	return buf;
}

void gst_al_rtp_buffer_unref(GstAlBuf *buf)
{
	gst_buffer_unref(buf->m_buffer);
	g_free(buf);
}
