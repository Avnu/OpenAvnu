#!/bin/bash
# Simple script to run echo_talker

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_echo_talker eth1"
    exit -1
fi

cd lib/avtp_pipeline/build/bin
exec ./openavb_host -I $1 echo_talker.ini,stream_addr=ba:bc:1a:ba:bc:1a
