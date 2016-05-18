#!/bin/bash
# Start all daemons

nic=$1
 
if [ "$1" == "" ];
then
    nic="eth1" #edit as required
    echo "Assuming: "$0" "$nic
fi
echo "Starting daemons on "$nic

#use false for silence
if true; then
    sudo rmmod igb
    sudo insmod kmod/igb/igb_avb.ko
else
    sudo rmmod igb > /dev/null 2>&1
    sudo insmod kmod/igb/igb_avb.ko > /dev/null 2>&1
fi

groupadd ptp
sudo daemons/gptp/linux/build/obj/daemon_cl $nic &
sudo daemons/mrpd/mrpd -mvs -i $nic &
sudo daemons/maap/linux/maap_daemon -i $nic &

