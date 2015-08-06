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

/**
 * \brief - Gstreamer Abstraction Layer interface
 *          for handling different versions
 *          (currently 0.10 and 1.0)
 */

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/rtp/gstrtpbuffer.h>

typedef GstFlowReturn (*GstAlCallback)(GstAppSink *sink, gpointer pv);

/**
 * \brief - Gstreamer Abstraction Layer buffer
 */
typedef struct _GstAlBuf
{
// public
	gpointer m_dptr;        //<! data pointer
	guint m_dlen;           //<! data length
// private
	GstBuffer *m_buffer;    //<! gstreamer buffer
#if GSTREAMER_1_0
	GstSample *m_sample;    //<! gstreamer sample
	GstMemory *m_memory;    //<! gstreamer memory
	GstRTPBuffer m_rtpbuf;  //<! gstreamer RTP buffer
	GstMapInfo m_info;      //<! gstreamer map info
#endif
}
GstAlBuf;

/** Gets buffer data pointer */
#define GST_AL_BUF_DATA(buf)    (buf->m_dptr)
/** Gets buffer data length */
#define GST_AL_BUF_SIZE(buf)    (buf->m_dlen)

#define GST_AL_BUFFER_TIMESTAMP(buf) GST_BUFFER_TIMESTAMP(buf->m_buffer)
#define GST_AL_BUFFER_DURATION(buf) GST_BUFFER_DURATION(buf->m_buffer)

/**
 * \brief - sets a buffer callback
 *
 * \param cbfns - a pointer to a callbacks structure
 * \param callback - a callback to set
 */
void gst_al_set_callback(GstAppSinkCallbacks *cbfns, GstAlCallback callback);
/**
 * \brief - pulls a buffer from a sink
 *
 * \param sink - a sink to pull a buffer from
 *
 * \return - a buffer taken from a sink
 */
GstAlBuf* gst_al_pull_buffer(GstAppSink *sink);
/**
 * \brief - pushes a buffer to source
 *
 * \param src - source to push a buffer to
 */
GstFlowReturn gst_al_push_buffer(GstAppSrc *src, GstAlBuf *buf);
/**
 * \brief - allocates a buffer
 *
 * \param len - length of a buffer
 *
 * \return - a newly allocated buffer
 */
GstAlBuf* gst_al_alloc_buffer(gint len);
/**
 * \brief - unrefs a buffer
 *
 * \param buf - a buffer to unref
 */
void gst_al_buffer_unref(GstAlBuf *buf);
/**
 * \brief - pulls a RTP buffer from sink
 *
 * \param sink - a sink to pull buffer from
 *
 * \return - a buffer taken from sink
 */
GstAlBuf* gst_al_pull_rtp_buffer(GstAppSink *sink);
/**
 * \brief - gets a RTP buffer marker
 *
 * \param buf - a RTP buffer
 *
 * \return - a RTP buffer marker
 */
gboolean gst_al_rtp_buffer_get_marker(GstAlBuf *buf);
/**
 * \brief - sets a RTP buffer marker
 *
 * \param buf - a RTP buffer
 * \param mark - a marker
 */
void gst_al_rtp_buffer_set_marker(GstAlBuf *buf, gboolean mark);
/**
 * \brief - set a RTP buffer params
 *
 * \param buf - a RTP buffer
 * \param ssrc - a ssrc param
 * \param payload_type - a payload type param
 * \param version - a version param
 * \param sequence - a sequence param
 */
void gst_al_rtp_buffer_set_params(GstAlBuf *buf, gint ssrc,
                                  gint payload_type, gint version, gint sequence);
/**
 * \brief - pushes a RTP buffer to source
 *
 * \param src - s source to push buffer to
 */
GstFlowReturn gst_al_push_rtp_buffer(GstAppSrc *src, GstAlBuf *buf);
/**
 * \brief - allocates a RTP buffer
 *
 * \param packet_len - length of a RTP buffer
 * \param pad_len - pad length of a RTP buffer
 * \param csrct_count - csrc count of a RTP buffer
 *
 * \return - a newly allocated RTP buffer
 */
GstAlBuf* gst_al_alloc_rtp_buffer(guint packet_len, guint8 pad_len, guint8 csrc_count);
/**
 * \brief - unrefs a RTP buffer
 *
 * \param buf - a RTP buffer to unref
 */
void gst_al_rtp_buffer_unref(GstAlBuf *buf);
