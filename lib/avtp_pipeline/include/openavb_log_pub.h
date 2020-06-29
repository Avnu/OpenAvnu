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
* MODULE SUMMARY : A simple logging facility for use during
* development.
*/

#ifndef OPENAVB_LOG_PUB_H
#define OPENAVB_LOG_PUB_H 1

// ********
// Merge Issue
// TODO: Restructure to remove #ifdef code.
// ********

#include "openavb_platform_pub.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "openavb_types_pub.h"

// Uncomment AVB_LOG_ON to enable logging.
#define AVB_LOG_ON	1

// Uncomment AVB_LOG_ON_OVERRIDE to override all AVB_LOG_ON usage in the stack to ensure all logs are off.
//#define AVB_LOG_ON_OVERRIDE	1

#ifdef AVB_LOG_ON_OVERRIDE
#ifdef AVB_LOG_ON
#undef AVB_LOG_ON
#endif
#endif

#define AVB_LOG_LEVEL_NONE	   0
#define AVB_LOG_LEVEL_ERROR	   1
#define AVB_LOG_LEVEL_WARNING  2
#define AVB_LOG_LEVEL_INFO     3
#define AVB_LOG_LEVEL_STATUS   4
#define AVB_LOG_LEVEL_DEBUG    5
#define AVB_LOG_LEVEL_VERBOSE  6

// Special case development logging levels for use with AVB_LOGF_DEV and AVB_LOG_DEV
#define AVB_LOG_LEVEL_DEV_ON    AVB_LOG_LEVEL_NONE
#define AVB_LOG_LEVEL_DEV_OFF   AVB_LOG_LEVEL_VERBOSE + 1

// Default log level, can override in source files
#ifndef AVB_LOG_LEVEL
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_ERROR 
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_INFO 
#define AVB_LOG_LEVEL AVB_LOG_LEVEL_STATUS 
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_VERBOSE
#endif

#ifndef AVB_LOG_COMPANY
#define AVB_LOG_COMPANY				"OPENAVB"
#endif

#ifndef AVB_LOG_COMPONENT
#define	AVB_LOG_COMPONENT			"AVB Stack"
#endif

// Log format options and sizes. Uncomment to include the formatted info.
#define LOG_MSG_LEN 1024

// The length of the full message
#define LOG_FULL_MSG_LEN 1024

static const bool OPENAVB_LOG_TIME_INFO = FALSE;
#define LOG_TIME_LEN 9
//#define LOG_TIME_LEN 1

static const bool OPENAVB_LOG_TIMESTAMP_INFO = TRUE;
#define LOG_TIMESTAMP_LEN 32
//#define LOG_TIMESTAMP_LEN 1

static const bool OPENAVB_LOG_FILE_INFO = FALSE;
//#define LOG_FILE_LEN 256
#define LOG_FILE_LEN 1

static const bool OPENAVB_LOG_PROC_INFO = FALSE;
//#define LOG_PROC_LEN 64
#define LOG_PROC_LEN 1

static const bool OPENAVB_LOG_THREAD_INFO = FALSE;
//#define LOG_THREAD_LEN 64
#define LOG_THREAD_LEN 1

#define LOG_RT_MSG_LEN 256
//#define LOG_RT_MSG_LEN 1

#define AVB_LOG_OUTPUT_FD stderr
//#define AVB_LOG_OUTPUT_FD stdout

// When OPENAVB_LOG_FROM_THREAD the message output will be output from a separate thread/task
// Primary intended use is for debugging.
// It is expected that OPENAVB_LOG_PULL_MODE will not be used at the same time as this optoin.
static const bool OPENAVB_LOG_FROM_THREAD = TRUE;

// When OPENAVB_LOG_PULL_MODE the messages will be queued and can be pulled using the
// avbLogGetMsg() call. This could be from an logger interface module or host application.
// It is expected that OPENAVB_LOG_FROM_THREAD will not be used at the same time as this option.
static const bool OPENAVB_LOG_PULL_MODE = FALSE;

// When using the OPENAVB_LOG_FROM_THREAD option. These defines control the behavior of the msg queue
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

// Log a message once. Technically once every 4.2 billion attempts. Usage: IF_LOG_ONCE() AVB_LOG_INFO(...)
#define IF_LOG_ONCE() static U32 LOG_VAR(logOnce,__LINE__) = 0; if (!LOG_VAR(logOnce,__LINE__)++)

// Log a message at an interval. Usage: IF_LOG_INTERVAL(100) AVB_LOG_INFO(...)
#define IF_LOG_INTERVAL(x) static U32 LOG_VAR(logOnce,__LINE__) = 0; if (!(LOG_VAR(logOnce,__LINE__)++ % (x)))


#define ETH_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETH_OCTETS(a) (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5]

#define STREAMID_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x/%u"
#define STREAMID_ARGS(s)   (s)->addr[0],(s)->addr[1],(s)->addr[2],(s)->addr[3],(s)->addr[4],(s)->addr[5],(s)->uniqueID

#define ENTITYID_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"
#define ENTITYID_ARGS(a)   (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6],(a)[7]

void avbLogInitEx(FILE *file);

void avbLogInit(void);

void avbLogExit(void);

void __avbLogFn(
	int level, 
	const char *tag, 
	const char *company,
	const char *component,
	const char *path,
	int line,
	const char *fmt, 
	va_list args);

void avbLogFn(
	int level,
	const char *tag,
	const char *company,
	const char *component,
	const char *path,
	int line,
	const char *fmt,
	...);

void avbLogRT(int level, bool bBegin, bool bItem, bool bEnd, char *pFormat, log_rt_datatype_t dataType, void *pVar);

void avbLogBuffer(
	int level,
	const U8 *pData,
	int dataLen,
	int lineLen,
	const char *company,
	const char *component,
	const char *path,
	int line);


#define avbLogFn2(level, tag, company, component, path, line, fmt, ...) \
    {\
        if (level <= AVB_LOG_LEVEL) \
            avbLogFn(0, tag, company, component, path, line, fmt, __VA_ARGS__); \
    }

#define avbLogRT2(level, bBegin, bItem, bEnd, pFormat, dataType, pVar) \
    {\
        if (level <= AVB_LOG_LEVEL) \
            avbLogRT(0, bBegin, bItem, bEnd, pFormat, dataType, pVar); \
    }

#define avbLogBuffer2(level, pData, dataLen, lineLen, company, component, path, line) \
    {\
        if (level <= AVB_LOG_LEVEL) \
            avbLogBuffer(0, pData, dataLen, lineLen, company, component, path, line); \
    }

#ifdef AVB_LOG_ON
#define AVB_LOGF_DEV(LEVEL, FMT, ...) avbLogFn2(LEVEL,                 "DEV",     AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_ERROR(FMT, ...)      avbLogFn2(AVB_LOG_LEVEL_ERROR,   "ERROR",   AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_WARNING(FMT, ...)    avbLogFn2(AVB_LOG_LEVEL_WARNING, "WARNING", AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_INFO(FMT, ...)       avbLogFn2(AVB_LOG_LEVEL_INFO,    "INFO",    AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_STATUS(FMT, ...)     avbLogFn2(AVB_LOG_LEVEL_STATUS,  "STATUS",  AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_DEBUG(FMT, ...)      avbLogFn2(AVB_LOG_LEVEL_DEBUG,   "DEBUG",   AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOGF_VERBOSE(FMT, ...)    avbLogFn2(AVB_LOG_LEVEL_VERBOSE, "VERBOSE", AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__)
#define AVB_LOG_DEV(LEVEL, MSG)       avbLogFn2(LEVEL,                 "DEV",     AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_ERROR(MSG)            avbLogFn2(AVB_LOG_LEVEL_ERROR,   "ERROR",   AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_WARNING(MSG)          avbLogFn2(AVB_LOG_LEVEL_WARNING, "WARNING", AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_INFO(MSG)             avbLogFn2(AVB_LOG_LEVEL_INFO,    "INFO",    AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_STATUS(MSG)           avbLogFn2(AVB_LOG_LEVEL_STATUS,  "STATUS",  AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_DEBUG(MSG)            avbLogFn2(AVB_LOG_LEVEL_DEBUG,   "DEBUG",   AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOG_VERBOSE(MSG)          avbLogFn2(AVB_LOG_LEVEL_VERBOSE, "VERBOSE", AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG)
#define AVB_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_ERROR, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_WARNING, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_INFO, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_STATUS, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_DEBUG, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)	avbLogRT2(AVB_LOG_LEVEL_VERBOSE, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)   avbLogBuffer2(LEVEL, DATA, DATALEN, LINELINE, AVB_LOG_COMPANY, AVB_LOG_COMPONENT, __FILE__, __LINE__)
#else
#define AVB_LOGF_DEV(LEVEL, FMT, ...)
#define AVB_LOGF_ERROR(FMT, ...)
#define AVB_LOGF_WARNING(FMT, ...)
#define AVB_LOGF_INFO(FMT, ...)
#define AVB_LOGF_STATUS(FMT, ...)
#define AVB_LOGF_DEBUG(FMT, ...)
#define AVB_LOGF_VERBOSE(FMT, ...)
#define AVB_LOG_DEV(LEVEL, FMT, ...)
#define AVB_LOG_ERROR(MSG)
#define AVB_LOG_WARNING(MSG)
#define AVB_LOG_INFO(MSG)
#define AVB_LOG_STATUS(MSG)
#define AVB_LOG_DEBUG(MSG)
#define AVB_LOG_VERBOSE(MSG)
#define AVB_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define AVB_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)
#endif	// AVB_LOG_ON

// Get a queued log message. Intended to be used with the OPENAVB_LOG_PULL_MODE option.
// Message will not be null terminated.
U32 avbLogGetMsg(U8 *pBuf, U32 bufSize);

#endif // OPENAVB_LOG_PUB_H
