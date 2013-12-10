/* Avbplaybin header file.
 *
 * Copyright (c) <2013>, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */


#ifndef __\_H__
#define __GST_AVBSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gio/gio.h>

G_BEGIN_DECLS


#define GST_TYPE_AVBSRC \
	(gst_avbsrc_get_type())
	
#define GST_AVBSRC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBSRC,GstAVBSrc))

#define GST_AVBSRC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBSRC,GstAVBSrcClass))

#define GST_IS_AVBSRC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBSRC))

#define GST_IS_AVBSRC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBSRC))

#define GST_AVBSRC_CAST(obj) ((GstAVBSrc *)(obj))

typedef struct _GstAVBSrc GstAVBSrc;
typedef struct _GstAVBSrcClass GstAVBSrcClass;

struct _GstAVBSrc {
	GstPushSrc parent;

	/* properties */
	gchar     *interface;
	gchar     *talker_ip;
	gint       port;
};

struct _GstAVBSrcClass {
	GstPushSrcClass parent_class;
};

GType gst_avbsrc_get_type(void);

G_END_DECLS


#endif /* __GST_AVBSRC_H__ */
