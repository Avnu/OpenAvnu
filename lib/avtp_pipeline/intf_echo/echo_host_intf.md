Echo interface {#echo_host_intf}
==============

# Description

This interface module as a talker will push a configured string into the Media 
Queue for transmission. As a listener it will echo the received data to stdout. 
This is strictly for testing purposes and is generally intended to work with the 
[Pipe mapping](@ref pipe_map) module 

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_echo_string       | String that will be sent by the talker
intf_nv_echo_string_repeat| Number of copies of the string to send in each     \
                            packet. <br>                                       \
                            The repeat setting if used must come after the     \
                            intf_nv_echo_string in this file
intf_nv_echo_increment    | If set to 1 an incrementing number will be appended\
                            to the string
intf_nv_tx_local_echo     | If set to 1 locally output the string to stdout    \
                            at the talker
intf_nv_echo_no_newline   | If set to 1 a newline will not be printed to the   \
                            stdout
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
                            Media Queue items will not be purged.
