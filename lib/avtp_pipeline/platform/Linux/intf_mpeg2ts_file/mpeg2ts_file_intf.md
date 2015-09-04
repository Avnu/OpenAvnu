MPEG2TS file interface {#mpeg2ts_file_intf}
=======================

# Description

Mpeg2 TS File interface module.
Computation of TS packet duration copied from Live555 application
(www.live555.com).

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_file_name         |The fully qualified file name. Used on **talker**   \
                           and on **listener** side
intf_nv_repeat            |If set to 1 it will continually repeat the file     \
                           stream when running as a talker
intf_nv_repeat_seconds    |Delay in seconds which will be skipped when repeating
intf_nv_enable_proper_bitrate_streaming|Setting to 1 will enable tracking of   \
                           the bitrate
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
			    Media Queue items will not be purged.

<br>
# Notes

Additionally the  @ref openavb_intf_cb_t::intf_get_src_bitrate_cb callback function 
can be used to calculate the maximum bitrate of the source.

**Note**: To make those calculations
**intf_nv_enable_proper_bitrate_streaming** has to be enabled.

If this callback is registered (not NULL) it will trigger several actions:
* calculated maximum bitrate will be passed to the maping module via the
function @ref openavb_map_cb_t::map_set_src_bitrate_cb
* callback @ref openavb_map_cb_t::map_get_max_interval_frames_cb will be
called to calculate the **maximum interval frames**
* **maximum frame size** is calculated by calling
@ref openavb_map_cb_t::map_max_data_size_cb.

If this callback function is not registered (is NULL), values taken from
the configuration file for **maximum frame size** and **maximum interval frames**
will be used for calculations.

**maximum frame size** and **maximum interval frames** values are used by
* SRP to calculate bandwidth,
* FQTSS to calculate queueing discipline parameters.

