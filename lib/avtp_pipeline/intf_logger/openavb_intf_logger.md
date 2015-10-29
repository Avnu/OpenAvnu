Logger Interface {#logger_intf}
================

# Description

This interface module is a talker only and design to send OPENAVB AVB 
internally log message over AVB for a listener to act on (such as 
display). One use case of this interface module is for low end devices 
that either have no provisions for console output or are restricted in 
CPU cycle to lot messages. 

For this interface module to be used the logging system must be built 
with the option OPENAVB_LOG_PULL_MODE set to TRUE. 

Typically the TX rate for this interface module can be quite low. The 
actual value will depend on how much data is expected to be logged. For 
general purpose use it is recommended to use 1000 packet per second or 
less. 

<br>
# Interface module configuration parameters

There are no configuration parameters for this interface module.

