## Introduction
This readme covers the **optional** feature to update systemd watchdog during gPTP daemon operation.
System on which the feature will be used must support systemd.

The solution reads WatchdogSec parameter from service configuration file and notifies watchdog
every 0.5*WatchdogSec that gPTP daemon is alive.
If there is no watchdog configuration available or WatchdogSec == 0, watchdog notification won't run.

Watchdog configuration is read once during gPTP daemon startup.


## Build Related

Systemd watchdog support in gPTP is a build time option.

### Systemd headers and library must be present in the system while building gPTP with this feature enabled.


### Building gPTP with Systemd watchdog handling enabled.
- SYSTEMD_WATCHDOG=1 make gptp


## Running 

### gPTP
- Run as normal.


