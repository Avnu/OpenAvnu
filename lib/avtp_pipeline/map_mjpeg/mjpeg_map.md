mjpeg Mapping {#mjpeg_map}
=============

# Description

Motion Jpeg mapping module conforming to 1722A RTP payload encapsulation.

# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of media queue elements to hold.
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class

# Notes

This module also uses the field media_q_item_map_mjpeg_pub_data_t::lastFragment
in both RX and TX. The usage depends on situation, for:
- TX - stores in the AVTP header information that this is the last fragment of
this frame. The interface module has to set this variable to correct value,
* RX - extracts from the AVTP header information if this fragment is the last one
of current video frame and sets field accordingly. The interface module might use
it later during frame composition.
