/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : Implementation of Timestamp evaluation useful for reporting and smoothing jitter.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openavb_printbuf.h"
#include "openavb_timestamp.h"

OPENAVB_CODE_MODULE_PRI

#define OPENAVB_TIMESTAMP_PRINT_BUFFER_SIZE 		2048
#define OPENAVB_TIMESTAMP_DUMP_PRINTBUF_INTERVAL 	30

// CORE_TODO: This should be enhanced to account for dropped packet detection and perhaps PTP time adjusts

struct openavb_timestamp_eval {
	// Flags
	bool started;

	// Settings
	U32 tsRateInterval;
	bool smoothing;
	U32 tsSmoothingMaxJitter;
	U32 tsSmoothingMaxDrift;
	U32 reportInterval;

	// Data
	U32 tsCnt;
	U32 tsPrev;
	U32 tsInterval;
	U32 tsJitter;
	U64 tsAccumJitter;
	U32 tsDrift;
	U64 tsTtlReal;
	U64 tsTtlCalc;
	U32 tsMaxJitter;
	U32 tsMaxDrift;
	openavb_printbuf_t printbuf;
};


openavb_timestamp_eval_t openavbTimestampEvalNew(void)
{
	return calloc(1, sizeof(struct openavb_timestamp_eval));
}

void openavbTimestampEvalDelete(openavb_timestamp_eval_t tsEval)
{
    if (tsEval) {
		free(tsEval);
		tsEval = NULL;
    }
}

void openavbTimestampEvalInitialize(openavb_timestamp_eval_t tsEval, U32 tsRateInterval)
{
    if (tsEval) {
		tsEval->started = FALSE;
		tsEval->tsRateInterval = tsRateInterval;
		tsEval->printbuf = openavbPrintbufNew(OPENAVB_TIMESTAMP_PRINT_BUFFER_SIZE, OPENAVB_TIMESTAMP_DUMP_PRINTBUF_INTERVAL);
    }
}

void openavbTimestampEvalSetReport(openavb_timestamp_eval_t tsEval, U32 reportInterval)
{
    if (tsEval) {
		tsEval->reportInterval = reportInterval;
    }
}

void openavbTimestampEvalSetSmoothing(openavb_timestamp_eval_t tsEval, U32 tsMaxJitter, U32 tsMaxDrift)
{
    if (tsEval) {
		tsEval->smoothing = TRUE;
		tsEval->tsSmoothingMaxJitter = tsMaxJitter;
		tsEval->tsSmoothingMaxDrift = tsMaxDrift;
    }
}

U32 openavbTimestampEvalTimestamp(openavb_timestamp_eval_t tsEval, U32 ts)
{
	U32 tsRet = ts;

    if (tsEval) {
		// Determine the Jitter
		if (tsEval->tsPrev > ts) {
			tsEval->tsInterval = (((U32)-1) - tsEval->tsPrev) + ts;
		}
		else {
			tsEval->tsInterval = ts - tsEval->tsPrev;
		}

		// Save real ts for not interval
		tsEval->tsPrev = ts;

		// Increment the main timestamp counter
		tsEval->tsCnt++;

		// Accumulate totals
		if (!tsEval->started) {
			// First timestamp interval
			tsEval->tsTtlCalc = 0;
			tsEval->tsTtlReal = 0;
			tsEval->started = TRUE;
		}
		else {
			// All but first timestamp
			tsEval->tsTtlCalc += tsEval->tsRateInterval;
			tsEval->tsTtlReal += tsEval->tsInterval;

			tsEval->tsJitter = abs(tsEval->tsRateInterval - tsEval->tsInterval);
			if (tsEval->tsJitter > tsEval->tsMaxJitter) {
				tsEval->tsMaxJitter = tsEval->tsJitter;
			}
			tsEval->tsAccumJitter += tsEval->tsJitter;

			tsEval->tsDrift = abs(tsEval->tsTtlCalc - tsEval->tsTtlReal);

			// Reporting
			if (tsEval->reportInterval) {
				if ((tsEval->tsCnt % tsEval->reportInterval) == 0) {
					//printf("Jitter:%9u   AvgJitter:%9u   MaxJitter:%9u   Drift:%9u\n", tsEval->tsJitter, (U32)(tsEval->tsAccumJitter / tsEval->tsCnt), tsEval->tsMaxJitter, tsEval->tsDrift); 
					openavbPrintbufPrintf(tsEval->printbuf, "Jitter:%9u   AvgJitter:%9u   MaxJitter:%9u   Drift:%9u\n", tsEval->tsJitter, (U32)(tsEval->tsAccumJitter / tsEval->tsCnt), tsEval->tsMaxJitter, tsEval->tsDrift);
				}
			}
		}
    }

	return tsRet;
}


void openavbTimestampEvalTimestampSkip(openavb_timestamp_eval_t tsEval, U32 cnt)
{
    if (tsEval) {
    }
}

