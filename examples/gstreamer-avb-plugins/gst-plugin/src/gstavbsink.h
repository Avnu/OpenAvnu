/*
 * Gstreamer AvbSink plugin header file
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



#ifndef __GST_AVBSINK_H__
#define __GST_AVBSINK_H__

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define GST_TYPE_AVBSINK (gst_avbsink_get_type())
#define GST_AVBSINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_AVBSINK,GstAvbSink))
#define GST_AVBSINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_AVBSINK,GstAvbSinkClass))
#define GST_IS_AVBSINK(obj) G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_AVBSINK))
#define GST_IS_AVBSINK_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_AVBSINK))

typedef struct _GstAvbSink GstAvbSink;
typedef struct _GstAvbSinkClass GstAvbSinkClass;


struct _GstAvbSink {
  GstBaseSink parent;

    /* properties */
  gchar *interface;
  guint sourceid;
};

struct _GstAvbSinkClass {
  GstBaseSinkClass parent_class;

  };

GType gst_avbsink_get_type(void);

G_END_DECLS

#endif /* __GST_AVBSINK_H__ */







