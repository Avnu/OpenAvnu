#! /bin/sh
#
# AVB endpoint clean-up script
#
# Only needs to be run if the endpoint process crashes.
#
# Removes resources created/loaded by the endpoint, so that a new
# instance can run.

IFACES=$(cat /proc/net/dev | grep -- : | cut -d: -f1)

echo "removing endpoint resources"

killall -s SIGINT openavb_endpoint > /dev/null 2>&1
killall -s SIGINT openavb_gptp > /dev/null 2>&1
rm -f /tmp/avb_endpoint > /dev/null 2>&1

for I in ${IFACES}
do
    ./tc qdisc del dev ${I} root > /dev/null 2>&1
done

rmmod sch_avb > /dev/null 2>&1
