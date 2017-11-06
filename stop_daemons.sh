#!/bin/bash
# Stop all daemons

killall shaper_daemon
killall maap_daemon
killall mrpd
killall daemon_cl

# possibly add rmmod igb_avb here

