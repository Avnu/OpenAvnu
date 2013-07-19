#!/usr/bin/env python
#
##############################################################################
#
#  Copyright (c) 2013, AudioScience, Inc
#  All rights reserved.
#  
#  Redistribution and use in source and binary forms, with or without 
#  modification, are permitted provided that the following conditions are met:
#  
#   1. Redistributions of source code must retain the above copyright notice, 
#      this list of conditions and the following disclaimer.
#  
#   2. Redistributions in binary form must reproduce the above copyright 
#      notice, this list of conditions and the following disclaimer in the 
#      documentation and/or other materials provided with the distribution.
#  
#   3. Neither the name of the Intel Corporation nor the names of its 
#      contributors may be used to endorse or promote products derived from 
#      this software without specific prior written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
##############################################################################
#
# Extracts AVTP AS timestamps from a libpcap capture file and stores them in
# a csv format ascii file.
#

import math
import scapy.all as s

usage = """avtp_astimes.py [options] input.libpcap output.csv

This script extracts AVTP packet timestamps and writes them
in a text file for later processing. Timestamps are modified
so as to be always incrementing (ie the 4s wrap is removed).

"""


wraps = 0
prev_pkt_ts = 0
ts_count = 0
seq = {}

class AVTP(s.Packet):
	name = "AVTP"
	fields_desc=[
		s.XByteField("controlData", None),
		s.XByteField("flags", None),
		s.XByteField("sequence", None),
		s.XByteField("timestampUncertain", None),
		s.IntField("streamID0", None),
		s.IntField("streamID1", None),
		s.IntField("ptpTimestamp", None),
		s.IntField("gateway", None),
		s.ShortField("pktDataLength", None),
		s.XByteField("pkt1394format", None),
		s.XByteField("pkt1394tcode", None),
		s.XByteField("sourceId", None),
		s.XByteField("dataBlockSize", None),
		s.XByteField("packing", None),
		s.XByteField("DBC", None),
		s.XByteField("formatId", None),
		s.XByteField("SYT", None),
		s.IntField("ptpUpper", None),
		s.IntField("ptpLower", None),
	]
s.bind_layers(s.Ether, AVTP, type=0x22f0)

def pkt_avtp(pkt, fout, pkt_count):
	global wraps, prev_pkt_ts, ts_count, ts_accum, seq
	
	n = 0
	avtp = pkt[AVTP]
	if avtp.controlData == 0x0:
		if seq['init'] == False:
			seq['last'] = avtp.sequence
			seq['init'] = True
		else:
			if avtp.sequence < seq['last']:
				if avtp.sequence != 0:
					print "Sequence wrap error at packet number %d" % (pkt_count)
			else:
				if avtp.sequence - seq['last'] != 1:
					print "Sequence error at packet number %d" % (pkt_count)
			seq['last'] = avtp.sequence
				
		if avtp.flags == 0x81:
			if ts_count == 0:
				ts_accum = avtp.ptpTimestamp
			else:
				if avtp.ptpTimestamp > prev_pkt_ts:
					ts_delta = avtp.ptpTimestamp - prev_pkt_ts
				else:
					ts_delta = avtp.ptpTimestamp + 0x100000000 - prev_pkt_ts
				ts_accum = ts_accum + ts_delta
				fout.write("%d, %d\n" % (ts_count, ts_accum))
				n = 1
			prev_pkt_ts = avtp.ptpTimestamp
			ts_count = ts_count + 1
	return n

def main(count, input_cap, output_txt, mac_filter):
	mac_counts = {}
	pkt_count = 0
	seq['last'] = 0
	seq['init'] = False
	capture = s.rdpcap(input_cap)
	foutput = open(output_txt, 'wt')
	for pkt in capture:
		n = 0;
		pkt_count += 1
		try:
			if pkt[s.Ether].dst in mac_counts:
				mac_counts[pkt[s.Ether].dst] = mac_counts[pkt[s.Ether].dst] + 1
			else:
				mac_counts[pkt[s.Ether].dst] = 1
				
			# Deal with AVTP packets
			if pkt[s.Ether].type == 0x22f0:
				# look for the requested MAC
				if pkt[s.Ether].dst == mac_filter:
					n = pkt_avtp(pkt, foutput, pkt_count)
		except IndexError:
			print "Unknown ethernet type"
		count = count - n
		if count == 0:
			break
	foutput.close();
	if count != 0:
		print "Could not find the specified MAC, or MAC count"
		print "Mac counts"
		print mac_counts
	print "Complete"

if __name__ == "__main__":
	from optparse import OptionParser
	
	parser = OptionParser(usage = usage)
	parser.add_option('-m','--mac',type='string',dest='dst_mac',
			help='Destination MAC address of the AVTP stream', default=None)
	parser.add_option('-c','--count',type='int',dest='count',
	                  help='Number of 802.1AS timestamps to extract. Default=%default', default=100)
			
	opts,args = parser.parse_args()
	
	print "Search for %d timestamped AVTP packets sent to dst MAC %s" % (opts.count, opts.dst_mac)
	
	if len(args) == 0:
		parser.print_usage()
		exit()
	
	main(opts.count, args[0], args[1], opts.dst_mac)
