#!/bin/bash

INTERFACE=p2p1
export INTERFACE

rmmod igb
rmmod igb_avb
insmod ./igb_avb.ko
sleep 1
ifconfig $INTERFACE down
echo 0 > /sys/class/net/$INTERFACE/queues/tx-0/xps_cpus
echo 0 > /sys/class/net/$INTERFACE/queues/tx-1/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-2/xps_cpus
echo f > /sys/class/net/$INTERFACE/queues/tx-3/xps_cpus
ifconfig $INTERFACE up
