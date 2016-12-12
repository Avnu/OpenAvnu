/*************************************************************************************************************
Copyright (c) 2016, Harman International Industries, Incorporated
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

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

#include "platform.h"

#define MAAP_LOG_COMPONENT "Log"
#include "maap_log.h"

static char s_lastLogTag[50];
static char s_lastLogMessage[LOG_MSG_LEN];

const char * Logging_getLastTag(void)
{
	return s_lastLogTag;
}

const char * Logging_getLastMessage(void)
{
	return s_lastLogMessage;
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
