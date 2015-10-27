Viewer interface {#viewer_intf}
================

# Description

Viewer interface module.

The viewer interface module is a listener only module. It is designed 
for testing purposes and will evaluate and display AVTP stream data in a 
number of formats. 


<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_view_type         | Mode of operation. Acceptable values are<ul>       \
                            <li>0 - Full details</li>                          \
                            <li>1 - mapping aware (not implemented)</li>       \
                            <li>2 - Timestamp mode</li>                        \
                            <li>3 - Latency measurement</li>                   \
                            <li>4 - Selective timestamp mode</li>              \
                            <li>5 - Late packet measurement</li>               \
                            <li>6 - Packet gap measurement</li></ul>
intf_nv_view_interval     | Frequency of output (in packet count)
intf_nv_raw_offset        | Offset into the raw frame to output
intf_nv_raw_length        | Length of the raw frame to output (0 = all)
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
                            Media Queue items will not be purged.
