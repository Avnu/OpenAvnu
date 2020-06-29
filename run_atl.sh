#!/bin/bash
# Simple script to run atl_avb

if [ "$#" -eq "0" ]; then
	echo "please enter network interface name as parameter. For example:"
	echo "sudo ./run_atl.sh eth1"
	exit -1
fi

rmmod atlantic
rmmod aqdiag
rmmod atl_tsn
modprobe crc_itu_t
modprobe ptp
insmod lib/atl_avb/kmod/atl_tsn.ko

ethtool -i $1
