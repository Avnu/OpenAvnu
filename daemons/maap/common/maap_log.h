/*************************************************************************************
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
*************************************************************************************/

/*
* MODULE SUMMARY : A simple logging facility for use during
* development.
*/

#ifndef MAAP_LOG_H
#define MAAP_LOG_H 1

// ********
// Merge Issue
// TODO: Restructure to remove #ifdef code.
// ********

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// Uncomment MAAP_LOG_ON to enable logging.
#define MAAP_LOG_ON	1

// Uncomment MAAP_LOG_ON_OVERRIDE to override all MAAP_LOG_ON usage in the app to ensure all logs are off.
//#define MAAP_LOG_ON_OVERRIDE	1

#ifdef MAAP_LOG_ON_OVERRIDE
#ifdef MAAP_LOG_ON
#undef MAAP_LOG_ON
#endif
#endif

#define MAAP_LOG_LEVEL_NONE     0
#define MAAP_LOG_LEVEL_ERROR    1
#define MAAP_LOG_LEVEL_WARNING  2
#define MAAP_LOG_LEVEL_INFO     3
#define MAAP_LOG_LEVEL_STATUS   4
#define MAAP_LOG_LEVEL_DEBUG    5
#define MAAP_LOG_LEVEL_VERBOSE  6

// Special case development logging levels for use with MAAP_LOGF_DEV and MAAP_LOG_DEV
#define MAAP_LOG_LEVEL_DEV_ON    MAAP_LOG_LEVEL_NONE
#define MAAP_LOG_LEVEL_DEV_OFF   MAAP_LOG_LEVEL_VERBOSE + 1

// Default log level, can override in source files
#ifndef MAAP_LOG_LEVEL
//#define MAAP_LOG_LEVEL MAAP_LOG_LEVEL_ERROR
//#define MAAP_LOG_LEVEL MAAP_LOG_LEVEL_INFO
#define MAAP_LOG_LEVEL MAAP_LOG_LEVEL_STATUS
//#define MAAP_LOG_LEVEL MAAP_LOG_LEVEL_DEBUG
//#define MAAP_LOG_LEVEL MAAP_LOG_LEVEL_VERBOSE
#endif

#ifndef MAAP_LOG_COMPANY
#define MAAP_LOG_COMPANY			"MAAP"
#endif

#ifndef MAAP_LOG_COMPONENT
#define	MAAP_LOG_COMPONENT			"MAAP"
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

static const int MAAP_LOG_TIME_INFO = FALSE;
#define LOG_TIME_LEN 9
//#define LOG_TIME_LEN 1

static const int MAAP_LOG_TIMESTAMP_INFO = TRUE;
#define LOG_TIMESTAMP_LEN 32
//#define LOG_TIMESTAMP_LEN 1

static const int MAAP_LOG_FILE_INFO = FALSE;
//#define LOG_FILE_LEN 256
#define LOG_FILE_LEN 1

static const int MAAP_LOG_PROC_INFO = FALSE;
//#define LOG_PROC_LEN 64
#define LOG_PROC_LEN 1

static const int MAAP_LOG_THREAD_INFO = FALSE;
//#define LOG_THREAD_LEN 64
#define LOG_THREAD_LEN 1

#define LOG_RT_MSG_LEN 256
//#define LOG_RT_MSG_LEN 1

#define MAAP_LOG_OUTPUT_FD stderr
//#define MAAP_LOG_OUTPUT_FD stdout

#define MAAP_LOG_STDOUT_CONSOLE_WIDTH 80

static const int MAAP_LOG_EXTRA_NEWLINE = TRUE;

// When MAAP_LOG_FROM_THREAD the message output will be output from a separate thread/task
// Primary intended use is for debugging.
// It is expected that MAAP_LOG_PULL_MODE will not be used at the same time as this option.
static const int MAAP_LOG_FROM_THREAD = TRUE;

// When MAAP_LOG_PULL_MODE the messages will be queued and can be pulled using the
// maapLogGetMsg() call. This could be from an logger interface module or host application.
// It is expected that MAAP_LOG_FROM_THREAD will not be used at the same time as this option.
static const int MAAP_LOG_PULL_MODE = FALSE;

// When using the MAAP_LOG_FROM_THREAD option. These defines control the behavior of the msg queue
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

// Log a message once. Technically once every 4.2 billion attempts. Usage: IF_LOG_ONCE() MAAP_LOG_INFO(...)
#define IF_LOG_ONCE() static uint32_t LOG_VAR(logOnce,__LINE__) = 0; if (!LOG_VAR(logOnce,__LINE__)++)

// Log a message at an interval. Usage: IF_LOG_INTERVAL(100) MAAP_LOG_INFO(...)
#define IF_LOG_INTERVAL(x) static uint32_t LOG_VAR(logOnce,__LINE__) = 0; if (!(LOG_VAR(logOnce,__LINE__)++ % (x)))


#define ETH_FORMAT    "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETH_OCTETS(a) (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5]


void maapLogInit(void);

void maapLogExit(void);

void maapLogFn(
	int level,
	const char *tag,
	const char *company,
	const char *component,
	const char *path,
	int line,
	const char *fmt,
	...);

void maapLogRT(int level, int bBegin, int bItem, int bEnd, char *pFormat, log_rt_datatype_t dataType, void *pVar);

void maapLogBuffer(
	int level,
	const uint8_t *pData,
	int dataLen,
	int lineLen,
	const char *company,
	const char *component,
	const char *path,
	int line);


#ifdef MAAP_LOG_ON
#define MAAP_LOGF_DEV(LEVEL, FMT, ...) do { if (LEVEL <= MAAP_LOG_LEVEL)                  maapLogFn(0, "DEV",     MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_ERROR(FMT, ...)      do { if (MAAP_LOG_LEVEL_ERROR <= MAAP_LOG_LEVEL)   maapLogFn(0, "ERROR",   MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_WARNING(FMT, ...)    do { if (MAAP_LOG_LEVEL_WARNING <= MAAP_LOG_LEVEL) maapLogFn(0, "WARNING", MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_INFO(FMT, ...)       do { if (MAAP_LOG_LEVEL_INFO <= MAAP_LOG_LEVEL)    maapLogFn(0, "INFO",    MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_STATUS(FMT, ...)     do { if (MAAP_LOG_LEVEL_STATUS <= MAAP_LOG_LEVEL)  maapLogFn(0, "STATUS",  MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_DEBUG(FMT, ...)      do { if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL)   maapLogFn(0, "DEBUG",   MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOGF_VERBOSE(FMT, ...)    do { if (MAAP_LOG_LEVEL_VERBOSE <= MAAP_LOG_LEVEL) maapLogFn(0, "VERBOSE", MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, FMT, __VA_ARGS__); } while(0)
#define MAAP_LOG_DEV(LEVEL, MSG)       do { if (LEVEL <= MAAP_LOG_LEVEL)                  maapLogFn(0, "DEV",     MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_ERROR(MSG)            do { if (MAAP_LOG_LEVEL_ERROR <= MAAP_LOG_LEVEL)   maapLogFn(0, "ERROR",   MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_WARNING(MSG)          do { if (MAAP_LOG_LEVEL_WARNING <= MAAP_LOG_LEVEL) maapLogFn(0, "WARNING", MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_INFO(MSG)             do { if (MAAP_LOG_LEVEL_INFO <= MAAP_LOG_LEVEL)    maapLogFn(0, "INFO",    MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_STATUS(MSG)           do { if (MAAP_LOG_LEVEL_STATUS <= MAAP_LOG_LEVEL)  maapLogFn(0, "STATUS",  MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_DEBUG(MSG)            do { if (MAAP_LOG_LEVEL_DEBUG <= MAAP_LOG_LEVEL)   maapLogFn(0, "DEBUG",   MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOG_VERBOSE(MSG)          do { if (MAAP_LOG_LEVEL_VERBOSE <= MAAP_LOG_LEVEL) maapLogFn(0, "VERBOSE", MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__, "%s", MSG); } while(0)
#define MAAP_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_ERROR, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_WARNING, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_INFO, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_STATUS, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_DEBUG, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)	maapLogRT(MAAP_LOG_LEVEL_VERBOSE, BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)   do { if (LEVEL <= MAAP_LOG_LEVEL) maapLogBuffer(0, DATA, DATALEN, LINELINE, MAAP_LOG_COMPANY, MAAP_LOG_COMPONENT, __FILE__, __LINE__); } while(0)
#else
#define MAAP_LOGF_DEV(LEVEL, FMT, ...)
#define MAAP_LOGF_ERROR(FMT, ...)
#define MAAP_LOGF_WARNING(FMT, ...)
#define MAAP_LOGF_INFO(FMT, ...)
#define MAAP_LOGF_STATUS(FMT, ...)
#define MAAP_LOGF_DEBUG(FMT, ...)
#define MAAP_LOGF_VERBOSE(FMT, ...)
#define MAAP_LOG_DEV(LEVEL, FMT, ...)
#define MAAP_LOG_ERROR(MSG)
#define MAAP_LOG_WARNING(MSG)
#define MAAP_LOG_INFO(MSG)
#define MAAP_LOG_STATUS(MSG)
#define MAAP_LOG_DEBUG(MSG)
#define MAAP_LOG_VERBOSE(MSG)
#define MAAP_LOGRT_ERROR(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_WARNING(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_INFO(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_STATUS(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_DEBUG(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOGRT_VERBOSE(BEGIN, ITEM, END, FMT, TYPE, VAL)
#define MAAP_LOG_BUFFER(LEVEL, DATA, DATALEN, LINELINE)
#endif	// MAAP_LOG_ON

// Get a queued log message. Intended to be used with the MAAP_LOG_PULL_MODE option.
// Message will not be null terminated.
uint32_t maapLogGetMsg(uint8_t *pBuf, uint32_t bufSize);

#endif // MAAP_LOG_H
