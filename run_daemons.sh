#!/bin/bash
# Start all daemons

if [ "$1" == "-h" ]; then
        echo "Usage: $0 <network interface>"
        echo "   eg: $0 eth1"
        echo ""
        echo "If you are using IGB, call \"sudo ./run_igb.sh\" before running this script."
        echo ""
        exit
fi

if [ "$1" == "" ]; then
        echo "Please enter network interface name as parameter. For example:"
        echo "sudo $0 eth1"
        echo ""
        echo "If you are using IGB, call \"sudo ./run_igb.sh\" before running this script."
        echo ""
        exit -1
fi

nic=$1
echo "Starting daemons on "$nic

groupadd ptp > /dev/null 2>&1
daemons/gptp/linux/build/obj/daemon_cl $nic &
daemons/mrpd/mrpd -mvs -i $nic &
daemons/maap/linux/build/maap_daemon -i $nic -d /dev/null
daemons/shaper/shaper_daemon -d &
