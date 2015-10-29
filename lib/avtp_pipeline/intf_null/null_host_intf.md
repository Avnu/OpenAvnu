NULL interface {#null_host_intf}
==============

# Description

This NULL interface module neither sends or receives data but will
exercise the various functions and callback and can be used as an example
or a template for new interfaces.

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
			    Media Queue items will not be purged.
