#!/bin/bash
# Simple script to run MAAP

if [ "$#" -eq "0" ]; then
    echo "please enter network interface name as parameter. For example:"
    echo "sudo ./run_maap.sh eth1"
    exit -1
fi

./daemons/maap/linux/build/maap_daemon -i $1
