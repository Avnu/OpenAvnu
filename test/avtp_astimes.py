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

usage = """avtp_astimes.py [options] input.libpcap

This script extracts AVTP packet timestamps and writes them
in a text file for later processing. Timestamps are modified
so as to be always incrementing (ie the 4s wrap is removed).

"""

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

def pkt_avtp(pkt, fout, pkt_count, info):
	
	n = 0
	avtp = pkt[AVTP]
	if info['seq']['init'] == False:
		info['seq']['last'] = avtp.sequence
		info['seq']['init'] = True
	else:
		if avtp.sequence < info['seq']['last']:
			if avtp.sequence != 0:
				print "Sequence wrap error at packet number %d" % (pkt_count)
		else:
			if avtp.sequence - info['seq']['last'] != 1:
				print "Sequence error at packet number %d (MAC %s)" % (pkt_count, info['this_mac'])
		info['seq']['last'] = avtp.sequence
			
	if avtp.flags == 0x81:
		if info['ts_count'] == 0:
			info['ts_accum'] = avtp.ptpTimestamp
		else:
			if avtp.ptpTimestamp > info['prev_pkt_ts']:
				ts_delta = avtp.ptpTimestamp - info['prev_pkt_ts']
			else:
				ts_delta = avtp.ptpTimestamp + 0x100000000 - info['prev_pkt_ts']
			info['ts_accum'] = info['ts_accum'] + ts_delta
			fout.write("%d, %d\n" % (info['ts_count'], info['ts_accum']))
			n = 1
		info['prev_pkt_ts'] = avtp.ptpTimestamp
		info['ts_count'] = info['ts_count'] + 1
	return n

def main(count, input_cap):
	mac_data = {}
	n_avtp_streams = 0
	foutput = []
	pkt_count = 0
	ts_good = 0
	capture = s.rdpcap(input_cap)
	for pkt in capture:
		n = 0;
		pkt_count += 1
		try:
			# Deal with AVTP packets
			if (pkt[s.Ether].type == 0x8100 and pkt[s.Dot1Q].type == 0x22f0):
				avtp = pkt[AVTP]
				if avtp.controlData == 0x0:
					if pkt[s.Ether].dst in mac_data:
						mac_data[pkt[s.Ether].dst]['avtp_count'] = mac_data[pkt[s.Ether].dst]['avtp_count'] + 1
					else:
						print "Packet %d, found AVTP stream with destination MAC %s" % (pkt_count, pkt[s.Ether].dst)
						mac_data[pkt[s.Ether].dst] = {}
						mac_data[pkt[s.Ether].dst]['this_mac'] = pkt[s.Ether].dst
						mac_data[pkt[s.Ether].dst]['avtp_count'] = 1
						mac_data[pkt[s.Ether].dst]['fname'] = 'seq%d.csv' % n_avtp_streams
						mac_data[pkt[s.Ether].dst]['fout'] = open(mac_data[pkt[s.Ether].dst]['fname'], 'wt')
						mac_data[pkt[s.Ether].dst]['seq'] = {}
						mac_data[pkt[s.Ether].dst]['seq']['last'] = 0
						mac_data[pkt[s.Ether].dst]['seq']['init'] = False
						mac_data[pkt[s.Ether].dst]['wraps'] = 0
						mac_data[pkt[s.Ether].dst]['prev_pkt_ts'] = 0
						mac_data[pkt[s.Ether].dst]['ts_count'] = 0
						mac_data[pkt[s.Ether].dst]['ts_accum'] = 0
						mac_data[pkt[s.Ether].dst]['ts_uncertain_count'] = 0
						n_avtp_streams = n_avtp_streams + 1
					
					if avtp.timestampUncertain:
						ts_good = 0
						mac_data[pkt[s.Ether].dst]['ts_uncertain_count'] = mac_data[pkt[s.Ether].dst]['ts_uncertain_count'] + 1
					else:
						ts_good = ts_good + 1

					# when we have 2 MACs, process the packet
					if n_avtp_streams == 2 and ts_good > 2:
						if mac_data[pkt[s.Ether].dst]['ts_count'] == 0:
							print "At packet %d start unpacking AVTP dest MAC %s" % (pkt_count, pkt[s.Ether].dst)
						n = pkt_avtp(pkt, mac_data[pkt[s.Ether].dst]['fout'], pkt_count, mac_data[pkt[s.Ether].dst])
		except IndexError:
			print "Unknown ethernet type packet %d" % pkt_count
		count = count - n
		if count == 0:
			break
	for k, v in mac_data.items():
		v['fout'].close();
		print "MAC %s %d AVTP timestamps stored to %s" % (k, v['ts_count'], v['fname'])
		print "         Timestamp uncertain count: %d" % v['ts_uncertain_count']

	if count != 0:
		print "Could not find the specified packets counts"
	
	print "Complete"

if __name__ == "__main__":
	from optparse import OptionParser
	
	parser = OptionParser(usage = usage)
	parser.add_option('-c','--count',type='int',dest='count',
	                  help='Number of 802.1AS timestamps to extract. Default=%default', default=100)
			
	opts,args = parser.parse_args()
	
	print "Search for %d timestamped AVTP packets" % (opts.count)
	
	if len(args) == 0:
		parser.print_usage()
		exit()
	
	main(opts.count * 2, args[0])
