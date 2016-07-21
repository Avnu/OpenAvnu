#!/bin/bash
# Stop all daemons

sudo killall maap_daemon
sudo killall mrpd
sudo killall daemon_cl

# possibly add rmmod igb_avb here

