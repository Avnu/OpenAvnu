Test tools
==========

This directory contains AVB related tools

AVTP Timestamps
---------------

This section covers tools and scripts related to AVTP timestamp analysis.

Extracting timestamps
.....................

The python script *avtp_astimes.py* extracts AVTP AS presentation times from
AVTP packets in a Wireshark capture file. The input capture file should
be stored in libpcap format. If there is more than one active AVB stream
in the packet capture, the python script can be run twice to extract multiple
timestamp sequences.

The below operation
::
   $python avtp_astimes.py -c 60000 -m 91:e0:f0:00:15:20 capture.libpcap a.csv
   $python avtp_astimes.py -c 60000 -m 91:e0:f0:00:35:80 capture.libpcap b.csv

extracts 60,000 AS presentation times from capture.libpcap and stores the times
in a.csv and b.csv. Recall that for an AVB stream running with a 48 kHz sample
rate, AS timestamps are inserted every 8 samples, making 6,000 AS timestamps
per second. The count of 60000 timestamps would therefore correspond to 10 seconds
of audio. The -m option specifies the destination multicast MAC that the AVB stream
is being sent to. The first three octets of the MAC address are 91:e0:f0 as reserved
by IEEE to 1722 AVTP transport. The destination MAC address is determined by manually
reading the packet capture and determining what destination MAC addresses are of interest.

Note that avtp_astimes.py does not check for missing packets and does not use the
packet capture timestamp at all. Additionally avtp_astimes.py "unwraps" the AS times
so that the output times are monotonically increasing.

Tested using python 2.6.5.

Line fitting
............

The python script *astime_fitline.py* fits a line to a .csv file that contains
AVTP presentation times. Least mean square error is used to perform the line fit.
The script can optionally fit two lines to two separate .csv sequences
and output the difference in slope and y intercept between the two lines.

The below operation
::
   $python avtp_fitline.py -c 60000 a.csv b.csv
   
Fits lines to the a.csv AS presentation sequence and the b.csv presentation time
sequence. The first 60,000 AS presentation times are used for both lines.

Use the -p option to plot the distrubition of AS time deviations from the fitted line.

Tested using python 2.6.5.

