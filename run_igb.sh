#!/bin/bash
# Simple script to run igb_avb

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_igb.sh eth1"
    exit -1
fi

rmmod igb
modprobe i2c_algo_bit
modprobe dca
modprobe ptp
insmod kmod/igb/igb_avb.ko

ethtool -i $1
