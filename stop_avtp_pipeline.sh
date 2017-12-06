#!/bin/bash

killall -2 openavb_host
sleep 1
killall -2 openavb_avdecc

# Code below this point is to recover in case one of the applications crashes.

sleep 5

killall -9 openavb_host
killall -9 openavb_avdecc
sleep 1

scriptdir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

$scriptdir/lib/avtp_pipeline/build/bin/shutdown_openavb_endpoint.sh
$scriptdir/lib/avtp_pipeline/build/bin/shutdown_openavb_avdecc.sh

