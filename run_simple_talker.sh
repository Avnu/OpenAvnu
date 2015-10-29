#!/bin/bash
# Simple script to the simple_talker

if [ "$#" -eq "0" ]; then 
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_simple_talker.sh eth1"
    exit -1
fi

examples/simple_talker/simple_talker -i $1 -t 2
