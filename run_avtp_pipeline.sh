#!/bin/bash

# Script to start the AVTP Pipeline talker/listener with 8-channel, 48K/24-bit IEC 61883-6 audio.
# For more details, refer to the lib/avtp_pipeline/README.md file.

if [ "$#" -eq "0" ]; then
	echo "Please enter network interface name as parameter. For example:"
	echo "sudo $0 eth1"
	echo ""
	exit -1
fi

nic=$1
echo "Starting AVTP Pipeline on "$nic

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

pushd .
cd $scriptdir/lib/avtp_pipeline/build/bin
./openavb_avdecc -I pcap:$nic example_talker.ini example_listener.ini &
sleep 5
./openavb_host -I pcap:$nic example_talker.ini example_listener.ini &
popd

