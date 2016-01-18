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
* MODULE SUMMARY : Mpeg2 TS File interface module.
*                  Computation of TS packet duration copied 
*                  from Live555 application (www.live555.com).
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"MPEG2TS Interface"
#include "openavb_log_pub.h" 

#define PCR_PERIOD_VARIATION_RATIO 0.5
#define TIME_ADJUSTMENT_FACTOR 0.8
#define MAX_PLAYOUT_BUFFER_DURATION 0.1 // (seconds)
#define NEW_DURATION_WEIGHT 0.5
#define MAX_TABLE_PIDS 100
#define F27_MHZ 27000000.0
#define F90_KHZ 90000.0

struct PIDStatus {
  double firstClock, lastClock, firstRealTime, lastRealTime;
  unsigned long long lastPacketNum;
  int used;
  int pid;
};

typedef struct {
	/////////////
	// Config data
	/////////////
	// intf_nv_file_name: The fully qualified file name used both the talker and listener.
	// NULL means use stdin/stdout
	char *pFileName;		

	// intf_nv_repeat: Continually repeat the file stream when running
	bool repeat;

	// Delay repeating the video until this many seconds have passed
	int repeatSeconds;

	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	/////////////
	// Variable data
	/////////////
	FILE *pFile;

	// Talker variables for tracking rewind
	struct timespec startTime;
	int nRepeatCount;
	int nBuffersSent;

	// Talker variables for tracking bitrate
	unsigned int maxBitrate;
	int    enableBitrateTracking;
	double nextTransmitTime;
	double fTSPacketCount;
	double fTSPCRCount;
	double fTSPacketDurationEstimate;
	struct PIDStatus *fPIDStatusTable;

} pvt_data_t;

double openavbIntfMpeg2tsFileComputeDuration(pvt_data_t* pPvtData, unsigned char* pkts, unsigned int length);

int pidTableFindOrCreatePid(struct PIDStatus *table, const unsigned int pid)
{
	int idx = -1;
	int i = 0;
	for (i = 0; i < MAX_TABLE_PIDS; ++i)
	{
		if (pid == table[i].pid)
		{
			idx = i;
			break;
		}
	}
	if (-1 == idx) /* create */
	{
		/* find free slot */
		for (i = 0; i < MAX_TABLE_PIDS; ++i)
		{
			if (0 == table[i].used)
			{
				table[i].pid = pid;
				idx = i;
				break;
			}
		}
	}

	return idx;
}

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfMpeg2tsFileCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		char *pEnd;
		unsigned long tmp;
		bool nameOK = TRUE, valueOK = FALSE;

		if (strcmp(name, "intf_nv_file_name") == 0) {
			if (pPvtData->pFileName)
				free(pPvtData->pFileName);
			pPvtData->pFileName = strdup(value);
			valueOK = TRUE;
		}

		else if (strcmp(name, "intf_nv_repeat") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && pEnd != value && (tmp == 0 || tmp == 1)) {
				pPvtData->repeat = (tmp == 1);
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "intf_nv_repeat_seconds") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && pEnd != value) {
				pPvtData->repeatSeconds = tmp;
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && pEnd != value && (tmp == 0 || tmp == 1)) {
				pPvtData->ignoreTimestamp = (tmp == 1);
				valueOK = TRUE;
			}
		}
		else if (strcmp(name, "intf_nv_enable_proper_bitrate_streaming") == 0) {
			tmp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && pEnd != value && (tmp == 0 || tmp == 1)) {
				pPvtData->enableBitrateTracking = (tmp == 1);
				valueOK = TRUE;
			}
		}
		else {
			AVB_LOGF_WARNING("Unknown configuration item: %s", name);
			nameOK = FALSE;
		}

		if (nameOK && !valueOK) {
			AVB_LOGF_WARNING("Bad value for configuration item: %s = %s", name, value);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

#define MPEGTS_SYNC_BYTE (0x47)
static void sync_scan(FILE* input)
{
	unsigned char byte = 0;
	while (1 == fread(&byte, 1, 1, input)) {
		if (MPEGTS_SYNC_BYTE == byte) {
			fseek(input, -1, SEEK_CUR);
			break;
		}
	}
}

#define TS_PACKETS 1
static unsigned int openavbComputeFileBitrate(char *fileName, media_q_t *pMediaQ)
{
	double max_bitrate = 0;
	FILE *input = fopen(fileName, "rb");
	if (input != NULL) {
		unsigned char* packets = (unsigned char *) malloc(188*TS_PACKETS);
		double fTSPCRCount = 0;
		struct PIDStatus *fPIDStatusTable = (struct PIDStatus*) calloc(MAX_TABLE_PIDS, sizeof(struct PIDStatus));
		double fTSPacketCount = 0;
		int i = 0;
		for(i = 0; i < MAX_TABLE_PIDS; ++i)
		{
			fPIDStatusTable[i].pid = -1;
			fPIDStatusTable[i].used = 0;
		}
		sync_scan(input);
		while((TS_PACKETS * 188) == fread((void *)packets, 1, 188*TS_PACKETS, input))
		{
			unsigned char* pkt;
			for (pkt = packets; pkt < &(packets[TS_PACKETS*188]); pkt += 188)
			{
				fTSPacketCount++;

				unsigned char const adaptation_field_control = (pkt[3]&0x30)>>4;
				if (adaptation_field_control != 2 && adaptation_field_control != 3)  continue;
				// there's no adaptation_field

				unsigned char const adaptation_field_length = pkt[4];
				if (adaptation_field_length == 0) continue;

				unsigned char const pcrFlag = pkt[5]&0x10;
				if (pcrFlag == 0) continue; // no PCR

				unsigned char const discontinuity_indicator = pkt[5]&0x80;
				// There's a PCR.  Get it.
				++fTSPCRCount;
				unsigned int pcrBaseHigh = (pkt[6]<<24)|(pkt[7]<<16)|(pkt[8]<<8)|pkt[9];
				double fClock = pcrBaseHigh/(F90_KHZ/2);
				if ((pkt[10]&0x80) != 0) fClock += 1/F90_KHZ; // add in low-bit (if set)
				unsigned short pcrExt = ((pkt[10]&0x01)<<8) | pkt[11];
				fClock += pcrExt/F27_MHZ;

				unsigned pid = ((pkt[1]&0x1F)<<8) | pkt[2];
				int idx = pidTableFindOrCreatePid(fPIDStatusTable, pid);
				if (!fPIDStatusTable[idx].used) {
					// We're seeing this PID's PCR for the first time:
					fPIDStatusTable[idx].used = 1;
					fPIDStatusTable[idx].firstClock = fClock;
					fPIDStatusTable[idx].lastClock = fClock;
					fPIDStatusTable[idx].lastPacketNum = fTSPacketCount;
				}
				else {
					if (discontinuity_indicator == 0) {
						double duration = fClock - fPIDStatusTable[idx].lastClock;
						if (duration > 0) {
							double data = (fTSPacketCount - fPIDStatusTable[idx].lastPacketNum) * 188 * 8;
							double bitrate = data / duration;
							if (bitrate > max_bitrate)
								max_bitrate = bitrate;
						}
						fPIDStatusTable[idx].lastClock = fClock;
						if (duration > 0)
							fPIDStatusTable[idx].lastPacketNum = fTSPacketCount;
					}
					else {
						fPIDStatusTable[idx].firstClock = fClock;
						fPIDStatusTable[idx].lastPacketNum = fTSPacketCount;
					}
				}
			}
		}
		fclose(input);
		free(fPIDStatusTable);
		free(packets);
	}

	return (unsigned int)max_bitrate;
}


void openavbIntfMpeg2tsFileGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

unsigned int openavbIntMpeg2tsGetSrcBitrate(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (((pvt_data_t *)pMediaQ->pPvtIntfInfo)->enableBitrateTracking)
		((pvt_data_t *)pMediaQ->pPvtIntfInfo)->maxBitrate = openavbComputeFileBitrate(((pvt_data_t*)pMediaQ->pPvtIntfInfo)->pFileName, pMediaQ);
	else
		((pvt_data_t *)pMediaQ->pPvtIntfInfo)->maxBitrate = 0;
	AVB_TRACE_EXIT(AVB_TRACE_INTF);

	return ((pvt_data_t *)pMediaQ->pPvtIntfInfo)->maxBitrate;
}
// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfMpeg2tsFileTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		pPvtData->nRepeatCount = 0;
		pPvtData->nBuffersSent = 0;

		if (!pPvtData->pFileName) {
			AVB_LOG_INFO("using stdin");
			pPvtData->pFileName = strdup("stdin");
			pPvtData->pFile = stdin;
		}
		else {
			pPvtData->pFile = fopen(pPvtData->pFileName, "rb");
			if (!pPvtData->pFile) {
				AVB_LOGF_ERROR("Unable to open input file: %s", pPvtData->pFileName);
				AVB_TRACE_EXIT(AVB_TRACE_INTF);
				return;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfMpeg2tsFileTxCB(media_q_t *pMediaQ)
{
	media_q_item_t *pMediaQItem = NULL;
	bool retval = FALSE;

	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		if (!pPvtData->pFile) {
			// input already closed
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;
		}

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		double nowSeconds = 0;
		if (pPvtData->enableBitrateTracking) {
			nowSeconds = (double)now.tv_sec + (double)now.tv_nsec / NANOSECONDS_PER_SECOND;

			if (nowSeconds < pPvtData->nextTransmitTime)
			{
				return FALSE;
			}
		}

		// handle end-of-file
		if (feof(pPvtData->pFile)) {
			if (pPvtData->pFileName && pPvtData->repeat) {
				if (pPvtData->nRepeatCount < 2)
					; // No delay for first few rewinds - want to buffer some data for restarts
				else if (pPvtData->repeatSeconds > (now.tv_sec - pPvtData->startTime.tv_sec)
						 || (pPvtData->repeatSeconds == (now.tv_sec - pPvtData->startTime.tv_sec)
							 && (now.tv_nsec >= pPvtData->startTime.tv_nsec))) {
					// don't rewind, yet
					AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
					return FALSE;
				}

				AVB_LOGF_INFO("EOF, rewinding input file: %s", pPvtData->pFileName);
				fseek(pPvtData->pFile, 0, 0);

				pPvtData->nRepeatCount++;
				pPvtData->nBuffersSent = 0;

				if (pPvtData->enableBitrateTracking) {
					// clear PCR infos here, PID hashtable, packet duration estimate
					pPvtData->fTSPacketDurationEstimate = 0;
					pPvtData->fTSPacketCount = 0;
					pPvtData->fTSPCRCount = 0;
					{
						int i = 0;
						for (i = 0; i < MAX_TABLE_PIDS; ++i)
						{
							pPvtData->fPIDStatusTable[i].used = 0;
							pPvtData->fPIDStatusTable[i].pid = -1;
						}
					}
				}
			}
			else {
				AVB_LOGF_INFO("EOF, closing input file: %s", pPvtData->pFileName);
				fclose(pPvtData->pFile);
				pPvtData->pFile = NULL;
				AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
				return FALSE;
			}
		}

		pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (!pMediaQItem) {
			IF_LOG_INTERVAL(1000) AVB_LOG_ERROR("Media queue full");
			AVB_TRACE_EXIT(AVB_TRACE_MAP_DETAIL);
			return FALSE;	// Media queue full
		}
 
		size_t result = fread(pMediaQItem->pPubData, 1, pMediaQItem->itemSize, pPvtData->pFile);
		if (result == 0) {
			int e = ferror(pPvtData->pFile);
			if (e != 0) {
				AVB_LOGF_ERROR("Error reading file: %s, %s", pPvtData->pFileName, strerror(e));
				fclose(pPvtData->pFile);
				pPvtData->pFile = NULL;
			}
			pMediaQItem->dataLen = 0;
			openavbMediaQHeadUnlock(pMediaQ);
		}
		else {
			pMediaQItem->dataLen = result;
			if (pPvtData->enableBitrateTracking) {
				pPvtData->nextTransmitTime = nowSeconds + openavbIntfMpeg2tsFileComputeDuration(pPvtData,(unsigned char*) pMediaQItem->pPubData, pMediaQItem->dataLen);
			}
			openavbMediaQHeadPush(pMediaQ);
			retval = TRUE;

			if (pPvtData->nBuffersSent++ == 0) {
				pPvtData->startTime = now;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);	
	return retval;
}

double openavbIntfMpeg2tsFileComputeDuration(pvt_data_t* pPvtData, unsigned char* pkts, unsigned int length)
{
	int offset = 0;
	unsigned char *pkt = NULL;

	while(pkts[offset] != 0x47)
		++offset;

	for (pkt = &pkts[offset]; pkt <= &(pkts[length-188]); pkt += 188)
	{
		struct timespec tvNow;
		clock_gettime(CLOCK_MONOTONIC, &tvNow);


		double timeNow = tvNow.tv_sec + tvNow.tv_nsec/NANOSECONDS_PER_SECOND;

		pPvtData->fTSPacketCount++;

		unsigned char const adaptation_field_control = (pkt[3]&0x30)>>4;
		if (adaptation_field_control != 2 && adaptation_field_control != 3)  continue;
		// there's no adaptation_field

		unsigned char const adaptation_field_length = pkt[4];
		if (adaptation_field_length == 0) continue;

		unsigned char const discontinuity_indicator = pkt[5]&0x80;
		unsigned char const pcrFlag = pkt[5]&0x10;
		if (pcrFlag == 0) continue; // no PCR

		// There's a PCR.  Get it, and the PID:
		++(pPvtData->fTSPCRCount);
		unsigned int pcrBaseHigh = (pkt[6]<<24)|(pkt[7]<<16)|(pkt[8]<<8)|pkt[9];
		double fClock = pcrBaseHigh/(F90_KHZ/2);
		if ((pkt[10]&0x80) != 0) fClock += 1/F90_KHZ; // add in low-bit (if set)
		unsigned short pcrExt = ((pkt[10]&0x01)<<8) | pkt[11];
		fClock += pcrExt/F27_MHZ;

		unsigned pid = ((pkt[1]&0x1F)<<8) | pkt[2];
		int idx = pidTableFindOrCreatePid(pPvtData->fPIDStatusTable, pid);
		if (!pPvtData->fPIDStatusTable[idx].used) {
			// We're seeing this PID's PCR for the first time:
			pPvtData->fPIDStatusTable[idx].used = 1;
			pPvtData->fPIDStatusTable[idx].firstClock = fClock;
			pPvtData->fPIDStatusTable[idx].lastClock = fClock;
			pPvtData->fPIDStatusTable[idx].firstRealTime = timeNow;
			pPvtData->fPIDStatusTable[idx].lastRealTime = timeNow;
			pPvtData->fPIDStatusTable[idx].lastPacketNum = 0;
			AVB_LOGF_VERBOSE("PID 0x%x, FIRST PCR 0x%08x+%d:%03x == %f @ %f, pkt #%lf\n", pid, pcrBaseHigh, pkt[10]>>7, pcrExt, fClock, timeNow, pPvtData->fTSPacketCount);
		} else {
			// We've seen this PID's PCR before; update our per-packet duration estimate:
			double packetsSinceLast = (pPvtData->fTSPacketCount -pPvtData->fPIDStatusTable[idx].lastPacketNum);
			// it's "int64_t" because some compilers can't convert "u_int64_t" -> "double"
			double durationPerPacket = (fClock - pPvtData->fPIDStatusTable[idx].lastClock)/packetsSinceLast;
			// Hack (suggested by "Romain"): Don't update our estimate if this PCR appeared unusually quickly.
			// (This can produce more accurate estimates for wildly VBR streams.)
			double meanPCRPeriod = 0.0;
			if (pPvtData->fTSPCRCount > 0) {
				double tsPacketCount = (double)(long long) pPvtData->fTSPacketCount;
				double tsPCRCount = (double)(long long)pPvtData->fTSPCRCount;
				meanPCRPeriod = tsPacketCount/tsPCRCount;
				if (packetsSinceLast < meanPCRPeriod*PCR_PERIOD_VARIATION_RATIO) continue ;
			}

			if (pPvtData->fTSPacketDurationEstimate == 0.0) { // we've just started
				pPvtData->fTSPacketDurationEstimate = durationPerPacket;
			} else if (discontinuity_indicator == 0 && durationPerPacket >= 0.0) {
				pPvtData->fTSPacketDurationEstimate= durationPerPacket*NEW_DURATION_WEIGHT + pPvtData->fTSPacketDurationEstimate*(1-NEW_DURATION_WEIGHT);

				// Also adjust the duration estimate to try to ensure that the transmission
				// rate matches the playout rate:

				double transmitDuration = timeNow - pPvtData->fPIDStatusTable[idx].firstRealTime;
				double playoutDuration = fClock - pPvtData->fPIDStatusTable[idx].firstClock;
				if (transmitDuration > playoutDuration) {
					pPvtData->fTSPacketDurationEstimate *= TIME_ADJUSTMENT_FACTOR; // reduce estimate
				} else if (transmitDuration + MAX_PLAYOUT_BUFFER_DURATION < playoutDuration) {
					pPvtData->fTSPacketDurationEstimate /= TIME_ADJUSTMENT_FACTOR; // increase estimate
				}
			} else {
				// the PCR has a discontinuity from its previous value; don't use it now,
				// but reset our PCR and real-time values to compensate:
				pPvtData->fPIDStatusTable[idx].firstClock = fClock;
				pPvtData->fPIDStatusTable[idx].firstRealTime = timeNow;
			}
			AVB_LOGF_VERBOSE("PID 0x%x, PCKT_CNT %lf PCR 0x%08x+%d:%03x == %f @ %f (diffs %f @ %f), pkt #%lf, discon %d => this duration %f, new estimate %f, mean PCR period=%f\n",
					pid, pPvtData->fTSPacketCount, pcrBaseHigh, pkt[10]>>7, pcrExt, fClock, timeNow, fClock - pPvtData->fPIDStatusTable[idx].firstClock, timeNow - pPvtData->fPIDStatusTable[idx].firstRealTime, pPvtData->fTSPacketCount, discontinuity_indicator != 0, durationPerPacket, pPvtData->fTSPacketDurationEstimate, meanPCRPeriod );
		}
		pPvtData->fPIDStatusTable[idx].lastClock = fClock;
		pPvtData->fPIDStatusTable[idx].lastRealTime = timeNow;
		pPvtData->fPIDStatusTable[idx].lastPacketNum = pPvtData->fTSPacketCount;
	}

	return pPvtData->fTSPacketDurationEstimate * ((length - offset) / 188);
}

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfMpeg2tsFileRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (!pPvtData->pFileName) {
			AVB_LOG_INFO("Using stdout");
			pPvtData->pFileName = strdup("stdout");
			pPvtData->pFile = stdout;
		}
		else {
			pPvtData->pFile = fopen(pPvtData->pFileName, "wb");
			if (!pPvtData->pFile) {
				AVB_LOGF_ERROR("Unable to open output file: %s", pPvtData->pFileName);
				AVB_TRACE_EXIT(AVB_TRACE_INTF);
				return;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfMpeg2tsFileRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		bool moreData = TRUE;
		size_t written;

		while (moreData) {
			media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
			if (pMediaQItem) {
				while (pPvtData->pFile && pMediaQItem->dataLen > 0) {
					written = fwrite(pMediaQItem->pPubData, 1, pMediaQItem->dataLen, pPvtData->pFile);
					if (written == 0) {
						int e = ferror(pPvtData->pFile);
						AVB_LOGF_ERROR("Error writing file: %s, %s", pPvtData->pFileName, strerror(e));
						fclose(pPvtData->pFile);
						pPvtData->pFile = NULL;
					}
					else {
						pMediaQItem->dataLen -= written;
					}
				}
				openavbMediaQTailPull(pMediaQ);
			}
			else {
				moreData = FALSE;
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}

// This callback will be called when the interface needs to be closed. All shutdown should 
// occur in this function.
void openavbIntfMpeg2tsFileEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (pPvtData->pFile) {
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfMpeg2tsFileGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (pPvtData->pFileName) {
			free(pPvtData->pFileName);
			pPvtData->pFileName = NULL;
		}
	}
	
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfMpeg2tsFileInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		int i = 0;
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfMpeg2tsFileCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfMpeg2tsFileGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfMpeg2tsFileTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfMpeg2tsFileTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfMpeg2tsFileRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfMpeg2tsFileRxCB;
		pIntfCB->intf_end_cb = openavbIntfMpeg2tsFileEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfMpeg2tsFileGenEndCB;
		pIntfCB->intf_get_src_bitrate_cb = openavbIntMpeg2tsGetSrcBitrate;

		pPvtData->ignoreTimestamp = FALSE;

		pPvtData->fPIDStatusTable = (struct PIDStatus*) calloc(MAX_TABLE_PIDS, sizeof(struct PIDStatus));

		if (NULL == pPvtData->fPIDStatusTable) {
			AVB_LOG_ERROR("Unable to allocate memory for PID status table.");
			return FALSE;
		}

		for (i = 0; i < MAX_TABLE_PIDS; ++i)
		{
			pPvtData->fPIDStatusTable[i].pid = -1;
		}
		pPvtData->enableBitrateTracking = 1;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
