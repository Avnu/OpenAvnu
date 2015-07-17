NULL Mapping {#null_map}
============

# Description

This NULL mapping does not pack or unpack any Mmedia Queue data in AVB packer.
It does however exercise the various functions and callback and can be
used as an example or a template for new mapping modules.

<br>
# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of media queue elements to hold.
map_nv_tx_rate      |Transmit interval in frames per second. \
                     0 = default for talker class
