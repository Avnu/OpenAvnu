/*************************************************************************************************************
Copyright (c) 2012-2016, Harman International Industries, Incorporated
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

#ifndef GPTP_LOG_HPP
#define GPTP_LOG_HPP

/**@file*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef GENIVI_DLT
#include "dlt.h"
#endif

#define GPTP_LOG_CRITICAL_ON		1
#define GPTP_LOG_ERROR_ON			1
#define GPTP_LOG_EXCEPTION_ON		1
#define GPTP_LOG_WARNING_ON			1
#define GPTP_LOG_INFO_ON			1
#define GPTP_LOG_STATUS_ON			1
//#define GPTP_LOG_DEBUG_ON			1
//#define GPTP_LOG_VERBOSE_ON		1

typedef enum {
	GPTP_LOG_LVL_CRITICAL,
	GPTP_LOG_LVL_ERROR,
	GPTP_LOG_LVL_EXCEPTION,
	GPTP_LOG_LVL_WARNING,
	GPTP_LOG_LVL_INFO,
	GPTP_LOG_LVL_STATUS,
	GPTP_LOG_LVL_DEBUG,
	GPTP_LOG_LVL_VERBOSE,
} GPTP_LOG_LEVEL;


void gptplogRegister(void);
void gptplogUnregister(void);
void gptpLog(GPTP_LOG_LEVEL level, const char *tag, const char *path, int line, const char *fmt, ...);


#define GPTP_LOG_REGISTER() gptplogRegister()

#define GPTP_LOG_UNREGISTER() gptplogUnregister()

#ifdef GPTP_LOG_CRITICAL_ON
#define GPTP_LOG_CRITICAL(fmt,...) gptpLog(GPTP_LOG_LVL_CRITICAL, "CRITICAL ", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_CRITICAL(fmt,...)
#endif

#ifdef GPTP_LOG_ERROR_ON
#define GPTP_LOG_ERROR(fmt,...) gptpLog(GPTP_LOG_LVL_ERROR, "ERROR    ", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_ERROR(fmt,...)
#endif

#ifdef GPTP_LOG_EXCEPTION_ON
#define GPTP_LOG_EXCEPTION(fmt,...) gptpLog(GPTP_LOG_LVL_EXCEPTION, "EXCEPTION", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_EXCEPTION(fmt,...)
#endif

#ifdef GPTP_LOG_WARNING_ON
#define GPTP_LOG_WARNING(fmt,...) gptpLog(GPTP_LOG_LVL_WARNING, "WARNING  ", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_WARNING(fmt,...)
#endif

#ifdef GPTP_LOG_INFO_ON
#define GPTP_LOG_INFO(fmt,...) gptpLog(GPTP_LOG_LVL_INFO, "INFO     ", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_INFO(fmt,...)
#endif

#ifdef GPTP_LOG_STATUS_ON
#define GPTP_LOG_STATUS(fmt,...) gptpLog(GPTP_LOG_LVL_STATUS, "STATUS   ", NULL, 0, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_STATUS(fmt,...)
#endif

#ifdef GPTP_LOG_DEBUG_ON
#define GPTP_LOG_DEBUG(fmt,...) gptpLog(GPTP_LOG_LVL_DEBUG, "DEBUG    ", __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_DEBUG(fmt,...)
#endif

#ifdef GPTP_LOG_VERBOSE_ON
#define GPTP_LOG_VERBOSE(fmt,...) gptpLog(GPTP_LOG_LVL_VERBOSE, "VERBOSE  ", __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#else
#define GPTP_LOG_VERBOSE(fmt,...)
#endif

#endif
