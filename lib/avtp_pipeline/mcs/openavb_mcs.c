/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
* MODULE SUMMARY : Media clock timestamp synthesis for stored or generated media
*/

#include "openavb_platform_pub.h"
#include "openavb_types_pub.h"
#include "openavb_log_pub.h"
#include "openavb_mcs.h"

void openavbMcsInit(mcs_t *mediaClockSynth, U64 nsPerAdvance, S32 correctionAmount, U32 correctionInterval)
{
	mediaClockSynth->firstTimeSet = FALSE;
	mediaClockSynth->nsPerAdvance = nsPerAdvance;
	mediaClockSynth->correctionAmount = correctionAmount;
	mediaClockSynth->correctionInterval = correctionInterval;
	mediaClockSynth->startTime = 0;
	mediaClockSynth->tickCount = 0;
	mediaClockSynth->edgeTime = 0;
}

void openavbMcsAdvance(mcs_t *mediaClockSynth)
{
	if (mediaClockSynth->firstTimeSet == FALSE) {
		CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &mediaClockSynth->edgeTime);
		if (mediaClockSynth->edgeTime) {
			mediaClockSynth->firstTimeSet = TRUE;
			mediaClockSynth->startTime = mediaClockSynth->edgeTime;
		}
	}
	else
	{
		mediaClockSynth->edgeTime += mediaClockSynth->nsPerAdvance;
		mediaClockSynth->tickCount++;
		if (mediaClockSynth->tickCount % mediaClockSynth->correctionInterval == 0) {
			mediaClockSynth->edgeTime += mediaClockSynth->correctionAmount;
		}
#if !IGB_LAUNCHTIME_ENABLED
		IF_LOG_INTERVAL(8000) {
			U64 nowNS;
			CLOCK_GETTIME64(OPENAVB_CLOCK_WALLTIME, &nowNS);
			S64 fixedRealDelta = mediaClockSynth->edgeTime - nowNS;
			AVB_LOGF_INFO("Fixed/Real TS Delta: %lld", fixedRealDelta);
		}
#endif
	}
}
