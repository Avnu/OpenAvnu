MPEG2 TS Mapping {#mpeg2ts_map}
================

# Description

Mpeg2 TS mapping module conforming to AVB 61883-4 encapsulation.

Refer to IEC 61883-4 for details of the "source packet" structure.

This mapping module module as a talker requires an interface module to push
one source packet of 192 octets into the media queue along with a media queue
time value when the first data block of the source packet was obtained.
There is no need to place the timestamp within the source packet header,
this will be done within the mapping module when the maximum transit time is 
added. The mapping module may bundle multiple source packets into one AVTP
packet before sending it.

This mapping module module as a listener will parse source packets from the AVTP
packet and place each source packet of 192 octets into the media queue along
with the correct timestamp from the source packet header. The interface module
will pull each source packet from the media queue and present has needed.

The protocol_specific_header, CIP header and the mpeg2 ts source packet header
are all taken care of in the mapping module.

<br>
# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of Media Queue items to hold.
map_nv_item_size    |Size of data in each Media Queue item
map_nv_ts_packet_size|Size of transport stream packets passed to/from interface\
                      module (188 or 192)
map_nv_num_source_packets|Number of source packets to send an in AVTP frame     \
                          (**Talker only**)
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class
