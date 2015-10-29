Pipe Mapping {#pipe_map}
============

# Description

The Pipe Mapping module is an AVTP vendor specific mapping format. It will pass through
any data from interface modules unchanged. This can be useful for development,
testing or custom solutions

<br>
# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of Media Queue items to hold.
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class
map_nv_max_payload_size| Maximum payload that will be send in one Ethernet frame
map_nv_push_header  |If set to 1 the Ethernet header should be pushed to the   \
                     Media Queue <br>                                          \
                     <b>Note</b>:RX side only - Listener
map_nv_pull_header  |If set to 1 data in Media Queue is with Ethernet header   \
                     <br>                                                      \
                     <b>Note</b>:TX side only - Talker
