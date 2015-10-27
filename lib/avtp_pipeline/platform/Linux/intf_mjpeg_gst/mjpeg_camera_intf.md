MJPEG GStreamer interface {#mjpeg_gst_intf}
=========================

# Description

MJPEG gstreamer interface module.

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_gst_pipeline      |GStreamer pipeline that will be used
intf_nv_async_rx          |If set to 1 sets RX in async mode
intf_nv_blocking_rx       |If set to 1 switches gstreamer into blocking mode
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
			    Media Queue items will not be purged.
