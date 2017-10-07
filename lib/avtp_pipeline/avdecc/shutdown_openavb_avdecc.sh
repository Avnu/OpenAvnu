#! /bin/sh
#
# AVB AVDECC clean-up script
#
# Only needs to be run if the AVDECC process crashes.
#
# Removes resources created/loaded by AVDECC, so that a new
# instance can run.

IFACES=$(cat /proc/net/dev | grep -- : | cut -d: -f1)

echo "removing AVDECC resources"

killall -s SIGINT openavb_avdecc > /dev/null 2>&1
killall -s SIGINT openavb_harness > /dev/null 2>&1
killall -s SIGINT openavb_host > /dev/null 2>&1
rm -f /tmp/avdecc_msg > /dev/null 2>&1

