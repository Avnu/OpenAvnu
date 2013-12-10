 /*
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

#include <stdio.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include<string.h>
#include<unistd.h>


/* Macro defining buffer size*/
#define BUFF_SIZE	1024

/* External functions from which gstreamer will receive the buffers */
extern int read_data_from_queue( void* ptr);
extern int send_data_status(int);
extern void flag_exit_app(int);

 /* Structure to contain all our information, so we can pass it to callbacks */
typedef struct 
{
	GstElement *playbin;
	GstAppSrc *src;
	GMainLoop *loop;
	guint sourceid;
	FILE *file;
} gst_app_t;

static gst_app_t gst_app;

 /* Function to read data*/
static gboolean read_data(gst_app_t *app)
{
	GstBuffer *buffer;
	guint8 *ptr;
	gint size;
	GstFlowReturn ret;

	ptr = g_malloc(BUFF_SIZE);
	g_assert(ptr);

	size = read_data_from_queue(ptr);
	if(size == 0) {
		ret = gst_app_src_end_of_stream(app->src);
		g_debug("eos returned %d at %d\n", ret, __LINE__);
		return FALSE;
	}
	buffer = gst_buffer_new();
	buffer = gst_buffer_new_wrapped(ptr, size);
	ret = gst_app_src_push_buffer(app->src, buffer);

	if(ret !=  GST_FLOW_OK) {
		g_debug("push buffer returned %d for %d bytes \n", ret, size);
		return FALSE;
	}

	if(size != BUFF_SIZE) {
		ret = gst_app_src_end_of_stream(app->src);
		g_debug("eos returned %d at %d\n", ret, __LINE__);
		return FALSE;
	}
	
	return TRUE;
}
 
 /* This signal callback triggers when audio appsrc needs data. Here, we add an idle handler
 * to the mainloop to start pushing data into the audio appsrc */
 
static void start_feed (GstElement * pipeline, guint size, gst_app_t *app)
{
	if (app->sourceid == 0) {
		GST_DEBUG("\nGSTREAMER CALLBACK : 1\n");
		send_data_status(1);
		GST_DEBUG ("start feeding");
		app->sourceid = g_idle_add ((GSourceFunc) read_data, app);
	}
}
 
 /* This callback triggers when audio appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop */
static void stop_feed (GstElement * pipeline, gst_app_t *app)
{
	if (app->sourceid != 0) {
		send_data_status(0);
		GST_DEBUG("\nGSTREAMER CALLBACK : 0\n");
		GST_DEBUG ("stop feeding");
		g_source_remove (app->sourceid);
		app->sourceid = 0;
	}
}

 /* Function to Parse Errors */
static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer *ptr)
{
	gst_app_t *app = (gst_app_t*)ptr;
	
	switch(GST_MESSAGE_TYPE(message))
	{
		case GST_MESSAGE_ERROR:
		{
			gchar *debug;
			GError *err;
			
			gst_message_parse_error(message, &err, &debug);
			g_print("Error %s\n", err->message);
			g_error_free(err);
			g_free(debug);
			g_main_loop_quit(app->loop);
		}
		break;

		case GST_MESSAGE_WARNING:
		{
			gchar *debug;
			GError *err;
			gchar *name;

			gst_message_parse_warning(message, &err, &debug);
			g_print("Warning %s\nDebug %s\n", err->message, debug);
			
			name = GST_MESSAGE_SRC_NAME(message);
			
			g_print("Name of src %s\n", name ? name : "nil");
			g_error_free(err);
			g_free(debug);
		}
		break;

		case GST_MESSAGE_EOS:
			g_print("End of stream\n");
			g_main_loop_quit(app->loop);
			flag_exit_app(1);
		break;

		case GST_MESSAGE_STATE_CHANGED:
		break;

		default:
			GST_DEBUG("got message %s\n", \
			gst_message_type_get_name (GST_MESSAGE_TYPE (message)));
		break;
	}

	return TRUE;
}

/* Function to be executed when source is found */
static void found_source (GObject * object, GObject * orig, GParamSpec * pspec, gst_app_t *app)
{
	/* get a handle to the appsrc */
	g_object_get (orig, pspec->name, &app->src, NULL);

	GST_DEBUG ("got appsrc %p", app->src);

	/* configure the appsrc, we will push a buffer to appsrc when it needs more data */
	g_signal_connect (app->src, "need-data", G_CALLBACK (start_feed), app);
	g_signal_connect (app->src, "enough-data", G_CALLBACK (stop_feed), app);
}

int gstreamer_main(void)
{
	gst_app_t *app = &gst_app;
	GstBus *bus;
	GstStateChangeReturn state_ret;
	
	/* GStreamer Initialization */
	gst_init(NULL, NULL);

	/* Create The Elements */
	app->src = (GstAppSrc*)gst_element_factory_make("appsrc", "mysrc");
	app->playbin = gst_element_factory_make("playbin", "myplaybin");

	/* Assert if Element Creation failed */
	g_assert(app->src);
	g_assert(app->playbin);

	/* Add the bus Handler */
	bus = gst_pipeline_get_bus(GST_PIPELINE (app->playbin));
	gst_bus_add_watch(bus, (GstBusFunc)bus_callback, app);

	/* Set uri property of playbin */
	g_object_set (app->playbin, "uri", "appsrc://",NULL);

	/* Connect the source to playbin when notification is arrived */
	g_signal_connect (app->playbin, "deep-notify::source",(GCallback) found_source, app);

	/* Set GStreamer Pipeline for Playing */
	state_ret = gst_element_set_state((GstElement*)app->playbin, GST_STATE_PLAYING);
	g_print("set state returned %d\n", state_ret);

	app->loop = g_main_loop_new(NULL, FALSE);
        gst_app_src_set_max_bytes(app->src, 1024 * 1024 * 120);
	g_main_loop_run(app->loop);
	
	/* Set GStreamer Pipeline to NULL */
	state_ret = gst_element_set_state((GstElement*)app->playbin, GST_STATE_NULL);
	gst_object_unref (bus);
	g_main_loop_unref (app->loop);

	return 0;
}
