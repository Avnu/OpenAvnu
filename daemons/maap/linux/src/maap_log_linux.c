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
#include <inttypes.h>

#include "platform.h"
#include "maap_log_queue.h"
#include "maap_helper_linux.h"

#define MAAP_LOG_COMPONENT "Log"
#include "maap_log.h"

typedef struct {
	uint8_t msg[LOG_QUEUE_MSG_SIZE];
  	int bRT;						// TRUE = Details are in RT queue
} log_queue_item_t;

typedef struct {
	char *pFormat;
	log_rt_datatype_t dataType;
	union {
		struct timespec nowTS;
		uint16_t unsignedShortVar;
		int16_t signedShortVar;
		uint32_t unsignedLongVar;
		int32_t signedLongVar;
		uint64_t unsignedLongLongVar;
		int64_t signedLongLongVar;
		float floatVar;
	} data;
	int bEnd;
} log_rt_queue_item_t;

static maap_log_queue_t logQueue;
static maap_log_queue_t logRTQueue;

static char msg[LOG_MSG_LEN] = "";
static char time_msg[LOG_TIME_LEN] = "";
static char timestamp_msg[LOG_TIMESTAMP_LEN] = "";
static char file_msg[LOG_FILE_LEN] = "";
static char proc_msg[LOG_PROC_LEN] = "";
static char thread_msg[LOG_THREAD_LEN] = "";
static char full_msg[LOG_FULL_MSG_LEN] = "";

static char rt_msg[LOG_RT_MSG_LEN] = "";

static int loggingThreadRunning = FALSE;
extern void *loggingThreadFn(void *pv);
THREAD_TYPE(loggingThread);
THREAD_DEFINITON(loggingThread);

#define THREAD_STACK_SIZE 									65536
#define loggingThread_THREAD_STK_SIZE    					THREAD_STACK_SIZE

static MUTEX_HANDLE_ALT(gLogMutex);
#define LOG_LOCK() MUTEX_LOCK_ALT(gLogMutex)
#define LOG_UNLOCK() MUTEX_UNLOCK_ALT(gLogMutex)

void maapLogRTRender(log_queue_item_t *pLogItem)
{
	if (logRTQueue) {
	  	pLogItem->msg[0] = 0x00;
		int bMore = TRUE;
		while (bMore) {
			maap_log_queue_elem_t elem = maapLogQueueTailLock(logRTQueue);
			if (elem) {
				log_rt_queue_item_t *pLogRTItem = (log_rt_queue_item_t *)maapLogQueueData(elem);

				switch (pLogRTItem->dataType) {
					case LOG_RT_DATATYPE_CONST_STR:
						strcat((char *)pLogItem->msg, pLogRTItem->pFormat);
						break;
					case LOG_RT_DATATYPE_NOW_TS:
						sprintf(rt_msg, "[%lu:%09lu] ", pLogRTItem->data.nowTS.tv_sec, pLogRTItem->data.nowTS.tv_nsec);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_U16:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.unsignedShortVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_S16:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.signedShortVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_U32:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.unsignedLongVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_S32:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.signedLongVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_U64:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.unsignedLongLongVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_S64:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.signedLongLongVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					case LOG_RT_DATATYPE_FLOAT:
						sprintf(rt_msg, pLogRTItem->pFormat, pLogRTItem->data.floatVar);
						strcat((char *)pLogItem->msg, rt_msg);
						break;
					default:
						break;
				}

				if (pLogRTItem->bEnd) {
					if (MAAP_LOG_EXTRA_NEWLINE)
						strcat((char *)pLogItem->msg, "\n");
					bMore = FALSE;
				}
				maapLogQueueTailPull(logRTQueue);
			}
		}
	}
}

uint32_t maapLogGetMsg(uint8_t *pBuf, uint32_t bufSize)
{
	uint32_t dataLen = 0;
	if (logQueue) {
		maap_log_queue_elem_t elem = maapLogQueueTailLock(logQueue);
		if (elem) {
			log_queue_item_t *pLogItem = (log_queue_item_t *)maapLogQueueData(elem);

			if (pLogItem->bRT)
				maapLogRTRender(pLogItem);

			dataLen = strlen((const char *)pLogItem->msg);
			if (dataLen <= bufSize)
				memcpy(pBuf, (uint8_t *)pLogItem->msg, dataLen);
			else
			  	memcpy(pBuf, (uint8_t *)pLogItem->msg, bufSize);
			maapLogQueueTailPull(logQueue);
			return dataLen;
		}
	}
	return dataLen;
}

void *loggingThreadFn(void *pv)
{
	while (loggingThreadRunning) {
		SLEEP_MSEC(LOG_QUEUE_SLEEP_MSEC);

		int more = TRUE;

		while (more) {
			more = FALSE;
			maap_log_queue_elem_t elem = maapLogQueueTailLock(logQueue);
			if (elem) {
				log_queue_item_t *pLogItem = (log_queue_item_t *)maapLogQueueData(elem);

				if (pLogItem->bRT)
					maapLogRTRender(pLogItem);

				fputs((const char *)pLogItem->msg, MAAP_LOG_OUTPUT_FD);
				maapLogQueueTailPull(logQueue);
				more = TRUE;
			}
		}
	}

	return NULL;
}

void maapLogInit(void)
{
	MUTEX_CREATE_ALT(gLogMutex);

	logQueue = maapLogQueueNewQueue(sizeof(log_queue_item_t), LOG_QUEUE_MSG_CNT);
	if (!logQueue) {
		printf("Failed to initialize logging facility\n");
	}

	logRTQueue = maapLogQueueNewQueue(sizeof(log_rt_queue_item_t), LOG_RT_QUEUE_CNT);
	if (!logRTQueue) {
		printf("Failed to initialize logging RT facility\n");
	}

	// Start the logging task
	if (MAAP_LOG_FROM_THREAD) {
		int errResult;
		loggingThreadRunning = TRUE;
		THREAD_CREATE(loggingThread, loggingThread, NULL, loggingThreadFn, NULL);
		THREAD_CHECK_ERROR(loggingThread, "Thread / task creation failed", errResult);
		if (errResult);		// Already reported
	}
}

void maapLogExit()
{
	if (MAAP_LOG_FROM_THREAD) {
		loggingThreadRunning = FALSE;
		THREAD_JOIN(loggingThread, NULL);
	}
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
	if (level <= MAAP_LOG_LEVEL) {
		va_list args;
		va_start(args, fmt);

		LOG_LOCK();

		vsprintf(msg, fmt, args);

		if (MAAP_LOG_FILE_INFO && path) {
			char* file = strrchr(path, '/');
			if (!file)
				file = strrchr(path, '\\');
			if (file)
				file += 1;
			else
				file = (char*)path;
			sprintf(file_msg, " %s:%d", file, line);
		}
		if (MAAP_LOG_PROC_INFO) {
			sprintf(proc_msg, " P:%5.5d", GET_PID());
		}
		if (MAAP_LOG_THREAD_INFO) {
			sprintf(thread_msg, " T:%lu", THREAD_SELF());
		}
		if (MAAP_LOG_TIME_INFO) {
			time_t tNow = time(NULL);
			struct tm tmNow;
			localtime_r(&tNow, &tmNow);

			sprintf(time_msg, "%2.2d:%2.2d:%2.2d", tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
		}
		if (MAAP_LOG_TIMESTAMP_INFO) {
			struct timespec nowTS;
			clock_gettime(CLOCK_REALTIME, &nowTS);

			sprintf(timestamp_msg, "%lu:%09lu", nowTS.tv_sec, nowTS.tv_nsec);
		}

		// using sprintf and puts allows using static buffers rather than heap.
		if (MAAP_LOG_EXTRA_NEWLINE)
			/* int32_t full_msg_len = */ sprintf(full_msg, "[%s%s%s%s %s %s%s] %s: %s\n", time_msg, timestamp_msg, proc_msg, thread_msg, company, component, file_msg, tag, msg);
		else
			/* int32_t full_msg_len = */ sprintf(full_msg, "[%s%s%s%s %s %s%s] %s: %s", time_msg, timestamp_msg, proc_msg, thread_msg, company, component, file_msg, tag, msg);

		if (!MAAP_LOG_FROM_THREAD && !MAAP_LOG_PULL_MODE) {
			fputs(full_msg, MAAP_LOG_OUTPUT_FD);
		}
		else {
			if (logQueue) {
				maap_log_queue_elem_t elem = maapLogQueueHeadLock(logQueue);
				if (elem) {
					log_queue_item_t *pLogItem = (log_queue_item_t *)maapLogQueueData(elem);
					pLogItem->bRT = FALSE;
					strncpy((char *)pLogItem->msg, full_msg, LOG_QUEUE_MSG_LEN);
					maapLogQueueHeadPush(logQueue);
				}
			}
		}

		va_end(args);

		LOG_UNLOCK();
	}
}

void maapLogRT(int level, int bBegin, int bItem, int bEnd, char *pFormat, log_rt_datatype_t dataType, void *pVar)
{
	if (level <= MAAP_LOG_LEVEL) {
		if (logRTQueue) {
			if (bBegin) {
				LOG_LOCK();

				maap_log_queue_elem_t elem = maapLogQueueHeadLock(logRTQueue);
				if (elem) {
					log_rt_queue_item_t *pLogRTItem = (log_rt_queue_item_t *)maapLogQueueData(elem);
					pLogRTItem->bEnd = FALSE;
					pLogRTItem->pFormat = NULL;
					pLogRTItem->dataType = LOG_RT_DATATYPE_NOW_TS;
					clock_gettime(CLOCK_REALTIME, &pLogRTItem->data.nowTS);
					maapLogQueueHeadPush(logRTQueue);
				}
			}

			if (bItem) {
				maap_log_queue_elem_t elem = maapLogQueueHeadLock(logRTQueue);
				if (elem) {
					log_rt_queue_item_t *pLogRTItem = (log_rt_queue_item_t *)maapLogQueueData(elem);
					if (bEnd)
						pLogRTItem->bEnd = TRUE;
					else
						pLogRTItem->bEnd = FALSE;
					pLogRTItem->pFormat = pFormat;
					pLogRTItem->dataType = dataType;

					switch (pLogRTItem->dataType) {
						case LOG_RT_DATATYPE_CONST_STR:
					  		break;
						case LOG_RT_DATATYPE_U16:
							pLogRTItem->data.unsignedLongVar = *(uint16_t *)pVar;
							break;
						case LOG_RT_DATATYPE_S16:
							pLogRTItem->data.signedLongVar = *(int16_t *)pVar;
							break;
						case LOG_RT_DATATYPE_U32:
							pLogRTItem->data.unsignedLongVar = *(uint32_t *)pVar;
							break;
						case LOG_RT_DATATYPE_S32:
							pLogRTItem->data.signedLongVar = *(int32_t *)pVar;
							break;
						case LOG_RT_DATATYPE_U64:
							pLogRTItem->data.unsignedLongLongVar = *(uint64_t *)pVar;
							break;
						case LOG_RT_DATATYPE_S64:
							pLogRTItem->data.signedLongLongVar = *(int64_t *)pVar;
							break;
						case LOG_RT_DATATYPE_FLOAT:
							pLogRTItem->data.floatVar = *(float *)pVar;
							break;
						default:
							break;
					}
					maapLogQueueHeadPush(logRTQueue);
				}
			}

			if (!bItem && bEnd) {
				maap_log_queue_elem_t elem = maapLogQueueHeadLock(logRTQueue);
				if (elem) {
					log_rt_queue_item_t *pLogRTItem = (log_rt_queue_item_t *)maapLogQueueData(elem);
					pLogRTItem->bEnd = TRUE;
					pLogRTItem->pFormat = NULL;
					pLogRTItem->dataType = LOG_RT_DATATYPE_NONE;
					maapLogQueueHeadPush(logRTQueue);
				}
			}

			if (bEnd) {
				if (logQueue) {
					maap_log_queue_elem_t elem = maapLogQueueHeadLock(logQueue);
					if (elem) {
						log_queue_item_t *pLogItem = (log_queue_item_t *)maapLogQueueData(elem);
						pLogItem->bRT = TRUE;
						if (MAAP_LOG_FROM_THREAD) {
							maapLogQueueHeadPush(logQueue);
						} else {
							maapLogRTRender(pLogItem);
							fputs((const char *)pLogItem->msg, MAAP_LOG_OUTPUT_FD);
							maapLogQueueHeadUnlock(logQueue);
						}
					}
				}

				LOG_UNLOCK();
			}
		}
	}
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
	char szDataLine[ 400 ];
	char *pszOut;
	size_t i, j;

	if (level > MAAP_LOG_LEVEL) { return; }

	for (i = 0; i < dataLen; i += lineLen) {
		/* Create the hexadecimal output for the buffer. */
		pszOut = szDataLine;
		*pszOut++ = '\t';
		for (j = i; j < i + lineLen; ++j) {
			if (j < dataLen) {
				sprintf(pszOut, "%02x ", pData[j]);
			} else {
				strcpy(pszOut, "   ");
			}
			pszOut += 3;
		}

		*pszOut++ = ' ';
		*pszOut++ = ' ';

		/* Append the ASCII equivalent of each character. */
		for (j = i; j < dataLen && j < i + lineLen; ++j) {
			if (pData[j] >= 0x20 && pData[j] < 0x7f) {
				*pszOut++ = (char) pData[j];
			} else {
				*pszOut++ = '.';
			}
		}

		/* Display this line of text. */
		*pszOut = '\0';
		maapLogFn(level, "BUFFER", company, component, path, line, "%s", szDataLine);
	}
}
