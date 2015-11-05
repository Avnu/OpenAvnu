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
* MODULE SUMMARY : Header for Timestamp evaluation useful for reporting and smoothing jitter.
*/

#ifndef OPENAVB_TIMESTAMP_H
#define OPENAVB_TIMESTAMP_H 1

#include <stdlib.h>
#include "openavb_types.h"

typedef struct openavb_timestamp_eval * openavb_timestamp_eval_t;

// Create and initialize the timestamp evaluator.
// tsInterval is the expected timestamp interval in nanoseconds.
openavb_timestamp_eval_t openavbTimestampEvalNew(void);

// Delete a timestamp evaluator.
void openavbTimestampEvalDelete(openavb_timestamp_eval_t tsEval);

// Set timestamp interval.
void openavbTimestampEvalInitialize(openavb_timestamp_eval_t tsEval, U32 tsRateInterval);

// Set the report interval. If not set there will be no reporting.
void openavbTimestampEvalSetReport(openavb_timestamp_eval_t tsEval, U32 reportInterval);

// Set the timestamp smoothing parameters.
void openavbTimestampEvalSetSmoothing(openavb_timestamp_eval_t tsEval, U32 tsMaxJitter, U32 tsMaxDrift);

// Record timestamp and optionally smooth it.
U32 openavbTimestampEvalTimestamp(openavb_timestamp_eval_t tsEval, U32 ts);

// Skip cnt number of timestamp intervals
void openavbTimestampEvalTimestampSkip(openavb_timestamp_eval_t tsEval, U32 cnt);














#endif // OPENAVB_TIMESTAMP_H
