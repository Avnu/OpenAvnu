/*************************************************************************************************************
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
*************************************************************************************************************/

/*
* MODULE SUMMARY : A simple logging facility for use during
* development.
*/

#ifndef SHAPER_LOG_H
#define SHAPER_LOG_H 1

// ********
// Merge Issue
// TODO: Restructure to remove #ifdef code.
// ********

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Uncomment SHAPER_LOG_ON to enable logging.
#define SHAPER_LOG_ON	1

// Uncomment SHAPER_LOG_ON_OVERRIDE to override all SHAPER_LOG_ON usage in the app to ensure all logs are off.
//#define SHAPER_LOG_ON_OVERRIDE	1

#ifdef SHAPER_LOG_ON_OVERRIDE
#ifdef SHAPER_LOG_ON
#undef SHAPER_LOG_ON
#endif
#endif

#define SHAPER_LOG_LEVEL_NONE     0
#define SHAPER_LOG_LEVEL_ERROR    1
#define SHAPER_LOG_LEVEL_WARNING  2
#define SHAPER_LOG_LEVEL_INFO     3
#define SHAPER_LOG_LEVEL_STATUS   4
#define SHAPER_LOG_LEVEL_DEBUG    5
#define SHAPER_LOG_LEVEL_VERBOSE  6

// Special case development logging levels for use with SHAPER_LOGF_DEV and SHAPER_LOG_DEV
#define SHAPER_LOG_LEVEL_DEV_ON    SHAPER_LOG_LEVEL_NONE
#define SHAPER_LOG_LEVEL_DEV_OFF   SHAPER_LOG_LEVEL_VERBOSE + 1

// Default log level, can override in source files
#ifndef SHAPER_LOG_LEVEL
//#define SHAPER_LOG_LEVEL SHAPER_LOG_LEVEL_ERROR
//#define SHAPER_LOG_LEVEL SHAPER_LOG_LEVEL_INFO
#define SHAPER_LOG_LEVEL SHAPER_LOG_LEVEL_STATUS
//#define SHAPER_LOG_LEVEL SHAPER_LOG_LEVEL_DEBUG
//#define SHAPER_LOG_LEVEL SHAPER_LOG_LEVEL_VERBOSE
#endif

#ifndef SHAPER_LOG_COMPANY
#define SHAPER_LOG_COMPANY			"SHAPER"
#endif

#ifndef SHAPER_LOG_COMPONENT
#define	SHAPER_LOG_COMPONENT			"SHAPER"
#endif

// Log format options and sizes. Uncomment to include the formatted info.
#define LOG_MSG_LEN 1024

// The length of the full message
#define LOG_FULL_MSG_LEN 1024

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static const int SHAPER_LOG_TIME_INFO = FALSE;
#define LOG_TIME_LEN 9
//#define LOG_TIME_LEN 1

static const int SHAPER_LOG_TIMESTAMP_INFO = TRUE;
#define LOG_TIMESTAMP_LEN 32
//#define LOG_TIMESTAMP_LEN 1

static const int SHAPER_LOG_FILE_INFO = FALSE;
//#define LOG_FILE_LEN 256
#define LOG_FILE_LEN 1

static const int SHAPER_LOG_PROC_INFO = FALSE;
//#define LOG_PROC_LEN 64
#define LOG_PROC_LEN 1

static const int SHAPER_LOG_THREAD_INFO = FALSE;
//#define LOG_THREAD_LEN 64
#define LOG_THREAD_LEN 1

#define LOG_RT_MSG_LEN 256
//#define LOG_RT_MSG_LEN 1

#define SHAPER_LOG_OUTPUT_FD stderr
//#define SHAPER_LOG_OUTPUT_FD stdout

#define SHAPER_LOG_STDOUT_CONSOLE_WIDTH 80

static const int SHAPER_LOG_EXTRA_NEWLINE = TRUE;

// When SHAPER_LOG_FROM_THREAD the message output will be output from a separate thread/task
// Primary intended use is for debugging.
// It is expected that SHAPER_LOG_PULL_MODE will not be used at the same time as this option.
static const int SHAPER_LOG_FROM_THREAD = TRUE;

// When SHAPER_LOG_PULL_MODE the messages will be queued and can be pulled using the
// shaperLogGetMsg() call. This could be from an logger interface module or host application.
// It is expected that SHAPER_LOG_FROM_THREAD will not be used at the same time as this option.
static const int SHAPER_LOG_PULL_MODE = FALSE;

// When using the SHAPER_LOG_FROM_THREAD option. These defines control the behavior of the msg queue
#define LOG_QUEUE_MSG_LEN		256
#define LOG_QUEUE_MSG_SIZE		(LOG_QUEUE_MSG_LEN + 1)
#define LOG_QUEUE_MSG_CNT		82
#define LOG_QUEUE_SLEEP_MSEC	100

// RT (RealTime logging) related defines
#define LOG_RT_QUEUE_CNT		128
#define LOG_RT_BEGIN			TRUE
#define LOG_RT_ITEM				TRUE
#define LOG_RT_END				TRUE
typedef enum {
	LOG_RT_DATATYPE_NONE,
	LOG_RT_DATATYPE_CONST_STR,
	LOG_RT_DATATYPE_NOW_TS,
	LOG_RT_DATATYPE_U16,
	LOG_RT_DATATYPE_S16,
	LOG_RT_DATATYPE_U32,
	LOG_RT_DATATYPE_S32,
	LOG_RT_DATATYPE_U64,
	LOG_RT_DATATYPE_S64,
	LOG_RT_DATATYPE_FLOAT
} log_rt_datatype_t;


#define LOG_VARX(x, y) x ## y
#define LOG_VAR(x, y) LOG_VARX(x, y)

// Log a message once. Technically once every 4.2 billion attempts. Usage: IF_LOG_ONCE() SHAPER_LOG_INFO(...)
#define IF_LOG_ONCE() static uint32_t LOG_VAR(logOnce,__LINE__) = 0; if (!LOG_VAR(logOnce,__LINE__)++)

// Log a message at an interval. Usage: IF_LOG_INTERVAL(100) SHAPER_LOG_INFO(...)
#define IF_LOG_INTERVAL(x) static uint32_t LOG_VAR(logOnce,__LINE__) = 0; if (!(LOG_VAR(logOnce,__LINE__)++ % (x)))


#define ETH_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETH_OCTETS(a) (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5]


void shaperLogInit(void);

void shaperLogDisplayAll(void);

void shaperLogExit(void);

void shaperLogFn(
	int level,
	const char *tag,
	const char *company,
	const char *component,
	const char *path,
	int line,
	const char *fmt,
	...);

void shaperLogRT(int level, int bBegin, int bItem, int bEnd, char *pFormat, log_rt_datatype_t dataType, void *pVar);

void shaperLogBuffer(
	int level,
	const uint8_t *pData,
	int dataLen,
	int lineLen,
	const char *company,
	const char *component,
	const char *path,
	int line);


#define shaperLogFn2(level, tag, company, component, path, line, fmt, ...) \
    {\
        if (level <= SHAPER_LOG_LEVEL) \
            shaperLogFn(0, tag, company, component, path, line, fmt, __VA_ARGS__); \
    }

#define shaperLogBuffer2(level, pData, dataLen, lineLen, company, component, path, line) \
    {\
        if (level <= AVB_LOG_LEVEL) \
            shaperLogBuffer(0, pData, dataLen, lineLen, company, component, path, line); \
    }

#ifdef SHAPER_LOG_ON
#define SHAPER_LOGF_DEV(LEVEL, FMT, ...) shaperLogFn2(LEVEL,                 "DEV",     SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_ERROR(FMT, ...)      shaperLogFn2(SHAPER_LOG_LEVEL_ERROR,   "ERROR",   SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_WARNING(FMT, ...)    shaperLogFn2(SHAPER_LOG_LEVEL_WARNING, "WARNING", SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_INFO(FMT, ...)       shaperLogFn2(SHAPER_LOG_LEVEL_INFO,    "INFO",    SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_STATUS(FMT, ...)     shaperLogFn2(SHAPER_LOG_LEVEL_STATUS,  "STATUS",  SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_DEBUG(FMT, ...)      shaperLogFn2(SHAPER_LOG_LEVEL_DEBUG,   "DEBUG",   SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOGF_VERBOSE(FMT, ...)    shaperLogFn2(SHAPER_LOG_LEVEL_VERBOSE, "VERBOSE", SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define SHAPER_LOG_DEV(LEVEL, MSG)       shaperLogFn2(LEVEL,                  "DEV",     SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_ERROR(MSG)            shaperLogFn2(SHAPER_LOG_LEVEL_ERROR,   "ERROR",   SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_WARNING(MSG)          shaperLogFn2(SHAPER_LOG_LEVEL_WARNING, "WARNING", SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_INFO(MSG)             shaperLogFn2(SHAPER_LOG_LEVEL_INFO,    "INFO",    SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_STATUS(MSG)           shaperLogFn2(SHAPER_LOG_LEVEL_STATUS,  "STATUS",  SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_DEBUG(MSG)            shaperLogFn2(SHAPER_LOG_LEVEL_DEBUG,   "DEBUG",   SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOG_VERBOSE(MSG)          shaperLogFn2(SHAPER_LOG_LEVEL_VERBOSE, "VERBOSE", SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define SHAPER_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_ERROR, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_WARNING, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_INFO, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_STATUS, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_DEBUG, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)	shaperLogRT(SHAPER_LOG_LEVEL_VERBOSE, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)   shaperLogBuffer2(LEVEL, DATA, DATALEN, LINELINE, SHAPER_LOG_COMPANY, SHAPER_LOG_COMPONENT, __FILE__, __LINE__)
#else
#define SHAPER_LOGF_DEV(LEVEL, FMT, ...)
#define SHAPER_LOGF_ERROR(FMT, ...)
#define SHAPER_LOGF_WARNING(FMT, ...)
#define SHAPER_LOGF_INFO(FMT, ...)
#define SHAPER_LOGF_STATUS(FMT, ...)
#define SHAPER_LOGF_DEBUG(FMT, ...)
#define SHAPER_LOGF_VERBOSE(FMT, ...)
#define SHAPER_LOG_DEV(LEVEL, FMT, ...)
#define SHAPER_LOG_ERROR(MSG)
#define SHAPER_LOG_WARNING(MSG)
#define SHAPER_LOG_INFO(MSG)
#define SHAPER_LOG_STATUS(MSG)
#define SHAPER_LOG_DEBUG(MSG)
#define SHAPER_LOG_VERBOSE(MSG)
#define SHAPER_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define SHAPER_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)
#endif	// SHAPER_LOG_ON

// Get a queued log message. Intended to be used with the SHAPER_LOG_PULL_MODE option.
// Message will not be null terminated.
uint32_t shaperLogGetMsg(uint8_t *pBuf, uint32_t bufSize);

#endif // SHAPER_LOG_H
