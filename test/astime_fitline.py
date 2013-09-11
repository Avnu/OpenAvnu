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
# Fits a line to a sequence of AVTP AS timestamps stored
# in a csv format ascii file.
#



usage = """astime_fitline.py -c 100 ts_file.txt
"""

from sys import exit, platform
from time import sleep
from struct import unpack
import matplotlib.pyplot as plt
import scipy
import scipy.fftpack
import numpy as np
import csv


def fit_line(count, plots, file1):
	# see http://en.wikipedia.org/wiki/Simple_linear_regression
	SX = 0.0
	SY = 0.0
	SXX = 0.0
	SXY = 0.0
	n = 0.0
	res = {}
	with open(file1, 'rU') as csvfile:
		vals = list(csv.reader(csvfile))
		for s in vals[0:count]:
			x = float(s[0])
			y = float(s[1])
			SX = SX + x
			SXX = SXX + (x * x)
			SY = SY + y
			SXY = SXY + (x * y)
			n = n + 1
		m = (n * SXY - SX * SY)/(n * SXX - SX * SX)
		b = SY/n - m * SX/n
		x_vals = np.array([float(vals[i][0]) for i in range(count)])
		y_line_array = x_vals * m + b
		y_array = np.array([float(vals[i][1]) for i in range(count)])
		line_deviation = y_line_array - y_array
		max_error = np.amax(abs(line_deviation))
		rnd_max_error = np.int_(max_error)
		histogram_bins = range((rnd_max_error + 1) * 2) - (rnd_max_error + 1);
		hist, bins = np.histogram(line_deviation, histogram_bins)
		res['histogram'] = hist
		res['bins'] = bins
		res['file'] = file1
		res['line_slope'] = m
		res['line_y_intercept'] = b
		res['line_max deviataion'] = max_error
		res['astimestamps'] = y_array
		if plots == True:
			if True:	# Distribution bar graph, switch to True to run
				figh = plt.figure()
				center = (bins[:-1]+bins[1:])/2
				plt.bar(center, hist, align = 'center', width = 0.7)
				plt.xlabel('deviation from fitted line in ns')
				plt.ylabel('count')
				plt.title(file1 + ' AS timestamp distribution\nof deviations from fitted line')
				res['hist_fig'] = figh

			if False:	# Fitted line plot, switch to True to run
				figl = plt.figure()
				plt.plot(range(count), y_array, 'ro', range(count), y_line_array, 'b-')
				plt.xlabel('AS timestamp count')
				plt.ylabel('AS timestamp')
				plt.title(file1 + ' fitted line')
				res['line_fig'] = figl

			if True:	# Deviation from fitted line plot, switch to True to run
				figl = plt.figure()
				plt.plot(range(count), line_deviation, 'ro')
				plt.xlabel('AS timestamp count')
				plt.ylabel('AS timestamp deviation')
				plt.title(file1 + ' deviation from fitted line')
				res['line_err'] = figl

			if count > 32768:
				if False:	# Frequency transform of deviation from fitted line, switch to True to run
					figf = plt.figure()
					fft_count = 32678
					# fft bin spacing is Fs/N, where Fs = 48000/8 and N = fft_count
					fft_bin_spacing = float(48000/8) / float(fft_count)
					freq_bins = [float(i) * fft_bin_spacing for i in range(fft_count/2 + 1)] 
					FFT = abs(np.fft.rfft(line_deviation[range(fft_count)]))
					plt.plot(freq_bins, np.log10(FFT))
					plt.xlabel('Hz')
					plt.title(file1 + ' deviation periodicity')
					res['fft_fig'] = figf
				
		csvfile.close()
	return res

def print_line(res):
	print 'File: ' + res['file']
	print 'Line slope: '  + str(res['line_slope'])
	print 'Line y intercept: '  + str(res['line_y_intercept'])
	print 'Maximum deviation from fitted line (ns): ' + str(res['line_max deviataion'])
	print 'Histgram of deviations (ns): '
	print '\tBins (ns): '
	print res['bins']
	print '\tCounts: '
	print res['histogram']

if __name__ == '__main__':
	from optparse import OptionParser

	parser = OptionParser(usage = usage)
	parser.add_option('-c','--count',type='int',dest='count',
	                  help='Number of sample points to fit. Default=%default', default=100)
	parser.add_option('-p', action='store_true', dest='plot', 
			help='Plot lines, histogram and fft. Default=%default', default=False)

	opts,args = parser.parse_args()

	if len(args) == 0:
		parser.print_usage()
		exit()
	# compute
	result1 = fit_line(opts.count, opts.plot, args[0])
	if len(args) == 2:
		result2 = fit_line(opts.count, opts.plot, args[1])

	#plot
	if opts.plot:
		plt.show()

	# print
	print_line(result1)
	if len(args) == 2:
		print_line(result2)
		print "=== The following output compares fitted lines and data between the two input data sets ==="
		ppb_freq = abs(result1['line_slope'] - result2['line_slope'])/result1['line_slope'] * 1000000000
		print "PPM freq difference is: %f" % ppb_freq
		ASTime_diff = abs(result1['line_y_intercept'] - result2['line_y_intercept'])
		time_diff_in_samples = ASTime_diff * 48000 / 1000000000
		print "AStime offset in is %f ns, or %f samples" % (ASTime_diff, time_diff_in_samples)
		floor_time_diff = np.int(time_diff_in_samples + 0.5)
		adj_time_diff = float(floor_time_diff) * 1000000000 / 48000
		print "AStime offset rounded to nearest sample is %d samples, or %f ns" % (floor_time_diff, adj_time_diff)
		if result1['line_y_intercept'] < result2['line_y_intercept']:
			adj_time_diff = adj_time_diff * -1
		time_astimestamps_diff = np.amax(abs((result1['astimestamps'] - result2['astimestamps']) - adj_time_diff))
		print "Maximum instaneous offset per AS timestamp is %f ns (%f of sample period)" % \
				(time_astimestamps_diff, time_astimestamps_diff / (1000000000 / 48000))
 		print "\tfor reference 0.25 of a sample at 48 kHz is %f ns" % ( 1000000000 / 48000 / 4)

