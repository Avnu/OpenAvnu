Ctrl Mapping {#ctrl_map}
============

# Description

Control mapping module.

The Control mapping is an AVTP control subtype with an vender specific
type defined to carry control command payloads between custom interface
modules. This is compliant with 1722a-D6

# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of media queue elements to hold.
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class
map_nv_max_payload_size| Maximum payload that will be send in one ethernet frame
