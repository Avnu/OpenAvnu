#!/bin/bash
# Simple script to run echo_talker

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_echo_talker eth1"
    exit -1
fi

# TODO_OPENAVB : Currently assumes a bin directory. 
cd ../build/bin
exec ./openavb_host echo_talker.ini,ifname=$1
