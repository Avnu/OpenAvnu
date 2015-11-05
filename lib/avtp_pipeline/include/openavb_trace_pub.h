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
* MODULE SUMMARY : Provide a simple tracing facility for use
* during development.
* 
* Based off of the openavb_log facility.
* 
* NOTE: The Tracing uses a high resolution timer and therefore
*   	librt must be linked into any application that uses this
*   	facility.
*/

#ifndef AVB_TRACE_PUB_H
#define AVB_TRACE_PUB_H 	1

#include "openavb_platform_pub.h"
#include <stdio.h>
#include "openavb_types_pub.h"

// Uncomment AVB_TRACE_ON to enable tracing.
//#define AVB_TRACE_ON				1

// Specific reporting modes
#define AVB_TRACE_MODE_NONE			0
#define AVB_TRACE_MODE_MINIMAL		1
#define AVB_TRACE_MODE_NORMAL		2
#define AVB_TRACE_MODE_DELTA_TIME	3
#define AVB_TRACE_MODE_DELTA_STATS	4
#define AVB_TRACE_MODE_FUNC_TIME	5
#define AVB_TRACE_MODE_DOC			6

// One of the above reporting modes must be set here
#define AVB_TRACE_MODE 				AVB_TRACE_MODE_NORMAL

// Delta Stats Interval 
#define AVB_TRACE_OPT_DELTA_STATS_INTERVAL		80000

// Function Time interval
#define AVB_TRACE_OPT_FUNC_TIME_INTERVAL		80000
//#define AVB_TRACE_OPT_FUNC_TIME_INTERVAL		100

// Option to show file name
//#define AVB_TRACE_OPT_SHOW_FILE_NAME			1


//#define AVB_TRACE_PRINTF(FMT, ...) fprintf(stderr, FMT, __VA_ARGS__)
//#define AVB_TRACE_PRINTF(FMT, ...) fprintf(stdout, FMT, __VA_ARGS__)
#define AVB_TRACE_PRINTF(FMT, ...) printf(FMT, __VA_ARGS__)


// Features to trace. 1 is enabled, 0 is disabled.
#define AVB_TRACE_MAP					0
#define AVB_TRACE_MAP_DETAIL			0
#define AVB_TRACE_MAP_LINE				0
#define AVB_TRACE_INTF					0
#define AVB_TRACE_INTF_DETAIL			0
#define AVB_TRACE_INTF_LINE				0
#define AVB_TRACE_HOST					0

#define TRACE_VAR1(x, y) x ## y
#define TRACE_VAR2(x, y) TRACE_VAR1(x, y)

#ifdef AVB_TRACE_OPT_SHOW_FILE_NAME
#define TRACE_FILE_NAME	__FILE__
#else
#define TRACE_FILE_NAME	""
#endif


#if !defined(AVB_TRACE_ON) || (AVB_TRACE_MODE == AVB_TRACE_MODE_NONE)
#define AVB_TRACE_LINE(FEATURE)
#define AVB_TRACE_ENTRY(FEATURE)
#define AVB_TRACE_EXIT(FEATURE)
#define AVB_TRACE_LOOP_ENTRY(FEATURE)
#define AVB_TRACE_LOOP_EXIT(FEATURE)
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_MINIMAL)
#define AVB_TRACE_LINE(FEATURE)			avbTraceMinimalFn(FEATURE, "TRACE LINE ", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_ENTRY(FEATURE)		avbTraceMinimalFn(FEATURE, "TRACE ENTRY", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_EXIT(FEATURE)			avbTraceMinimalFn(FEATURE, "TRACE EXIT ", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_LOOP_ENTRY(FEATURE)	avbTraceMinimalFn(FEATURE, "TRACE LOOP{", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_LOOP_EXIT(FEATURE)	avbTraceMinimalFn(FEATURE, "TRACE LOOP}", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_NORMAL)
#define AVB_TRACE_LINE(FEATURE)			avbTraceNormalFn(FEATURE, "TRACE LINE ", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_ENTRY(FEATURE)		avbTraceNormalFn(FEATURE, "TRACE ENTRY", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_EXIT(FEATURE)			avbTraceNormalFn(FEATURE, "TRACE EXIT ", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_LOOP_ENTRY(FEATURE)	avbTraceNormalFn(FEATURE, "TRACE LOOP{", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#define AVB_TRACE_LOOP_EXIT(FEATURE)	avbTraceNormalFn(FEATURE, "TRACE LOOP}", __FUNCTION__, TRACE_FILE_NAME, __LINE__)
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_DELTA_TIME)
#define AVB_TRACE_LINE(FEATURE)			avbTraceDeltaTimeFn(FEATURE, "TRACE LINE ", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_ENTRY(FEATURE)		avbTraceDeltaTimeFn(FEATURE, "TRACE ENTRY", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_EXIT(FEATURE)			avbTraceDeltaTimeFn(FEATURE, "TRACE EXIT ", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_LOOP_ENTRY(FEATURE) 	avbTraceDeltaTimeFn(FEATURE, "TRACE LOOP{", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_LOOP_EXIT(FEATURE)	avbTraceDeltaTimeFn(FEATURE, "TRACE LOOP}", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_DELTA_STATS)
#define AVB_TRACE_LINE(FEATURE)			static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceDeltaTimeFn(FEATURE, "TRACE LINE ", __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#define AVB_TRACE_ENTRY(FEATURE)		static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceDeltaTimeFn(FEATURE, "TRACE ENTRY", __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#define AVB_TRACE_EXIT(FEATURE)			static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceDeltaTimeFn(FEATURE, "TRACE EXIT ", __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#define AVB_TRACE_LOOP_ENTRY(FEATURE)	static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceDeltaTimeFn(FEATURE, "TRACE LOOP{", __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#define AVB_TRACE_LOOP_EXIT(FEATURE)	static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceDeltaTimeFn(FEATURE, "TRACE LOOP}", __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_FUNC_TIME)
#define AVB_TRACE_LINE(FEATURE)
#define AVB_TRACE_ENTRY(FEATURE)		static struct timespec TRACE_VAR2(tsFuncEntry,__FUNCTION__); avbTraceFuncTimeFn(FEATURE, TRUE, __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(tsFuncEntry,__FUNCTION__), NULL, NULL)
#define AVB_TRACE_EXIT(FEATURE)			static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceFuncTimeFn(FEATURE, FALSE, __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(tsFuncEntry,__FUNCTION__), &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#define AVB_TRACE_LOOP_ENTRY(FEATURE)	static struct timespec TRACE_VAR2(tsLoopEntry,__FUNCTION__); avbTraceFuncTimeFn(FEATURE, TRUE, __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(tsLoopEntry,__FUNCTION__), NULL, NULL)
#define AVB_TRACE_LOOP_EXIT(FEATURE)	static U64 TRACE_VAR2(traceCnt,__LINE__); static U64 TRACE_VAR2(traceNSec, __LINE__); avbTraceFuncTimeFn(FEATURE, FALSE, __FUNCTION__, TRACE_FILE_NAME, __LINE__, &TRACE_VAR2(tsLoopEntry,__FUNCTION__), &TRACE_VAR2(traceCnt,__LINE__), &TRACE_VAR2(traceNSec, __LINE__))
#elif (AVB_TRACE_MODE == AVB_TRACE_MODE_DOC)
#define AVB_TRACE_LINE(FEATURE)			avbTraceDocFn(FEATURE, "|", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_ENTRY(FEATURE)		avbTraceDocFn(FEATURE, ">", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_EXIT(FEATURE)			avbTraceDocFn(FEATURE, "<", __FUNCTION__, TRACE_FILE_NAME, __LINE__, NULL, NULL)
#define AVB_TRACE_LOOP_ENTRY(FEATURE)
#define AVB_TRACE_LOOP_EXIT(FEATURE)
#endif

#ifdef AVB_TRACE_ON

static inline void avbTraceMinimalFn(int featureOn, const char *tag, const char *function, const char *file, int line)
{
	if (featureOn) {
		AVB_TRACE_PRINTF("%s: %s():%d %s\n", tag, function, line, file);
	}
}

static inline void avbTraceNormalFn(int featureOn, const char *tag, const char *function, const char *file, int line)
{
	if (featureOn) {
		time_t tNow = time(NULL);
		struct tm tmNow;
		localtime_r(&tNow, &tmNow);

		AVB_TRACE_PRINTF("[%2.2d:%2.2d:%2.2d] [P:%5.5d T:%lu] %s: %s():%d %s\n", 
				tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, GET_PID(), THREAD_SELF(), tag, function, line, file);
	}
}

static inline void avbTraceDeltaTimeFn(int featureOn, const char *tag, const char *function, const char *file, int line, U64 *pCnt, U64 *pNSec)
{
	if (featureOn) {
		static struct timespec traceDeltaTimePrevTS;

		struct timespec nowTS;
		CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &nowTS);

		time_t tNow = time(NULL);
		struct tm tmNow;
		localtime_r(&tNow, &tmNow);

		U64 prevNSec = ((U64)traceDeltaTimePrevTS.tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)traceDeltaTimePrevTS.tv_nsec;
		U64 nowNSec = ((U64)nowTS.tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)nowTS.tv_nsec;
		U64 deltaNSec = nowNSec - prevNSec;

		if (pCnt && pNSec) {
			// Delta Stats
			(*pCnt)++;
			*pNSec += deltaNSec;

			if (*pCnt % AVB_TRACE_OPT_DELTA_STATS_INTERVAL == 0) {
				AVB_TRACE_PRINTF("[%2.2d:%2.2d:%2.2d] [P:%5.5d T:%lu] [cnt:%6llu deltaUS:%10llu totalUS:%10llu] %s: %s():%d %s\n", 
					tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, GET_PID(), THREAD_SELF(), (unsigned long long)(*pCnt), (unsigned long long)(deltaNSec / 1000), (unsigned long long)(*pNSec / 1000), tag, function, line, file);
			}
		}
		else {
			AVB_TRACE_PRINTF("[%2.2d:%2.2d:%2.2d] [P:%5.5d T:%lu] [deltaUS:%10llu] %s: %s():%d %s\n", 
					tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, GET_PID(), THREAD_SELF(), (unsigned long long)(deltaNSec / 1000), tag, function, line, file);
		}

		CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &traceDeltaTimePrevTS);
	}
}

static inline void avbTraceFuncTimeFn(int featureOn, bool bEntry, const char *function, const char *file, int line, struct timespec *pTS, U64 *pCnt, U64 *pNSec)
{
	if (featureOn) {
		if (bEntry) {
			CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, pTS);
		}
		else {
			struct timespec nowTS;
			CLOCK_GETTIME(OPENAVB_CLOCK_REALTIME, &nowTS);

			time_t tNow = time(NULL);
			struct tm tmNow;
			localtime_r(&tNow, &tmNow);

			U64 entryNSec = ((U64)pTS->tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)pTS->tv_nsec;
			U64 nowNSec = ((U64)nowTS.tv_sec * (U64)NANOSECONDS_PER_SECOND) + (U64)nowTS.tv_nsec;
			U64 deltaNSec = nowNSec - entryNSec;

			(*pCnt)++;
			*pNSec += deltaNSec;

			if (*pCnt % AVB_TRACE_OPT_FUNC_TIME_INTERVAL == 0) {
				AVB_TRACE_PRINTF("[%2.2d:%2.2d:%2.2d] [P:%5.5d T:%lu] [cnt:%6llu lastUS:%10llu totalUS:%10llu avgUS:%10llu] %s():%d %s\n", 
					tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, GET_PID(), THREAD_SELF(), (unsigned long long)(*pCnt), (unsigned long long)(deltaNSec / 1000), (unsigned long long)(*pNSec / 1000), (unsigned long long)((*pNSec / *pCnt) / 1000), function, line, file);
			}
		}
	}
}

static inline void avbTraceDocFn(int featureOn, const char *tag, const char *function, const char *file, int line, U64 *pCnt, U64 *pNSec)
{
	if (featureOn) {
		static int depthTrace = 0;
		if (tag[0] == '>') {
			AVB_TRACE_PRINTF("%*s%s %s  [%s]\n", depthTrace * 4, "", tag, function, file);
			depthTrace++;
		}
		else if (tag[0] == '<') {
			depthTrace--;
			AVB_TRACE_PRINTF("%*s%s %s  [%s]\n", depthTrace * 4, "", tag, function, file);
		}
		else {
			AVB_TRACE_PRINTF("%*s%s %s  [%s]\n", depthTrace * 4, "", tag, function, file);
		}
	}
}

#endif	// AVB_TRACE_ON
#endif // AVB_TRACE_PUB_H
