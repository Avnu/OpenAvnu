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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if defined(_WIN32) && (_MSC_VER < 1800)
/* Visual Studio 2012 and earlier */
typedef unsigned __int8 uint8_t;
typedef unsigned __int32 uint32_t;
#else
#include <inttypes.h>
#endif

#include <assert.h>

#include "platform.h"

#define MAAP_LOG_COMPONENT "Log"
#include "maap_log.h"

static char s_lastLogTag[LOG_MSG_LEN];
static char s_lastLogMessage[LOG_MSG_LEN];

const char * Logging_getLastTag(void)
{
	static char returnLogTag[LOG_MSG_LEN];

	/* Copy the current value. */
	strncpy(returnLogTag, s_lastLogTag, sizeof(returnLogTag));
	returnLogTag[sizeof(returnLogTag) - 1] = '\0';

	/* Clear the current value, so it isn't returned again. */
	s_lastLogTag[0] = '\0';

	return returnLogTag;
}

const char * Logging_getLastMessage(void)
{
	static char returnLogMessage[LOG_MSG_LEN];

	/* Copy the current value. */
	strncpy(returnLogMessage, s_lastLogMessage, sizeof(returnLogMessage));
	returnLogMessage[sizeof(returnLogMessage) - 1] = '\0';

	/* Clear the current value, so it isn't returned again. */
	s_lastLogMessage[0] = '\0';

	return returnLogMessage;
}


void maapLogInit(void)
{
}

void maapLogExit()
{
}

void maapLogFn(
	int level,
	const char *tag,
	const char *company,
	const char *component,
	const char *path,
	int line,
	const char *fmt,
	...)
{
	va_list args;
	va_start(args, fmt);

	if (strcmp(tag, "DEBUG") == 0 || strcmp(tag, "VERBOSE") == 0) { return; }

	/* Save the supplied tag for use later. */
	strncpy(s_lastLogTag, tag, sizeof(s_lastLogTag));
	s_lastLogTag[sizeof(s_lastLogTag) - 1] = '\0';

	/* Save the supplied message for use later. */
	assert(sizeof(s_lastLogMessage) >= 1000);
	vsprintf(s_lastLogMessage, fmt, args);

#if 0
	/* Print the highlights of the log data. */
	printf("%s:  %s\n", s_lastLogTag, s_lastLogMessage);
#endif

	va_end(args);
}

void maapLogBuffer(
	int level,
	const uint8_t *pData,
	int dataLen,
	int lineLen,
	const char *company,
	const char *component,
	const char *path,
	int line)
{
	/* Don't do anything with this. */
}
