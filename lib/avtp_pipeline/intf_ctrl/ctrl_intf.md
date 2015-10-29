Ctrl interface {#ctrl_intf}
==============

# Description

Control interface module.

This interface module sends and receives control messages. There are
two modes this interface module can be configured for normal mode and mux
mode. In normal mode the control messages are exchanged with the host
application. In mux mode the control messages will be multiplexed from
multiple control streams into one out-going talker control stream. This
is part of the dispatch model for configuring the control message system
in the OPENAVB AVB stack.

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_mux_mode          | If set to 1 the multiplier mode for the control    \
                           interface is enabled. This is used for the dispatch \
                           model of control message configuration. <br>For a   \
                           listener the in mux mode any incoming control       \
                           message is immediatedly placed in the ctrl mux      \
                           talkers media queue for retransmission.<br>         \
                           When using mux mode, the talker MUST be created     \
                           prior to listeners and remain running as long as any\
                           listeners are running.
intf_nv_ignore_timestamp  | If set to 1 timestamps will be ignored during      \
                            processing of frames. This also means stale (old)  \
			    Media Queue items will not be purged.
