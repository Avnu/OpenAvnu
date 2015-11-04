#!/bin/bash
# Simple script to run srp

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_srp.sh eth1"
    exit -1
fi

daemons/mrpd/mrpd -s -i $1
