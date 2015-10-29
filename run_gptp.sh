#!/bin/bash
# Simple script to run gptp

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_gptp.sh eth1"
    exit -1
fi

groupadd ptp
daemons/gptp/linux/build/obj/daemon_cl $1
