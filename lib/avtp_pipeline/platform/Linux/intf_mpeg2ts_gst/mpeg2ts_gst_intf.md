MPEG2TS GStreamer interface {#mpeg2ts_gst_intf}
============================

# Description

Mpeg2 TS GStreamer interface module.

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_gst_pipeline      |GStreamer pipeline to be used
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
			    Media Queue items will not be purged.
