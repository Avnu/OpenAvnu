#!/bin/bash
# Start all daemons

nic=$1

if [ "$1" == "-h" ]; then
        echo "Usage: $0 <network interface>"
        echo "   eg: $0 eth1"
        exit
fi

if [ "$1" == "" ]; then
        nic="eth1" #edit as required
        echo "Network interface not specified, assuming: $nic"
fi

echo "Starting daemons on "$nic

#use false for silence
if true; then
        sudo rmmod igb
        sudo insmod kmod/igb/igb_avb.ko
        sudo groupadd ptp
else
        sudo rmmod igb > /dev/null 2>&1
        sudo insmod kmod/igb/igb_avb.ko > /dev/null 2>&1
        sudo groupadd ptp > /dev/null 2>&1
fi

sudo daemons/gptp/linux/build/obj/daemon_cl $nic &
sudo daemons/mrpd/mrpd -mvs -i $nic &
sudo daemons/maap/linux/maap_daemon -i $nic &

