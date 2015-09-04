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
* MODULE SUMMARY : Tone generator interface module. Talker only.
* 
* - This interface module generates and audio tone for use with -6 and AAF mappings
* - Requires an OSAL sin implementation of reasonable performance. 
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "openavb_platform_pub.h"
#include "openavb_osal_pub.h"
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_uncmp_audio_pub.h"
#include "openavb_map_aaf_audio_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"Tone Gen Interface"
#include "openavb_log_pub.h" 

#define PI 3.14159265358979f

typedef struct {
	/////////////
	// Config data
	/////////////
	// intf_nv_tone_hz: The tone hz to generate
	U32 toneHz;
	
	// intf_nv_on_off_interval_msec: Interval for turning tone on and off. A value of zero will keep the tone on.
	U32 onOffIntervalMSec;

	// Simple melody
	char *pMelodyString;

	// intf_nv_audio_rate
	avb_audio_rate_t audioRate;

	// intf_nv_audio_type
	avb_audio_bit_depth_t audioType;

	// intf_nv_audio_bit_depth
	avb_audio_bit_depth_t audioBitDepth;

	// intf_nv_audio_endian
	avb_audio_bit_depth_t audioEndian;

	// intf_nv_channels
	avb_audio_channels_t audioChannels;

	/////////////
	// Variable data
	/////////////

	// Packing interval
	U32 intervalCounter;

	// Keeps track of if the tone is currently on or off	
	U32 freq;
	
	// Keeps track of how long before toggling the tone on / off
	U32 freqCountdown;

	// Ratio precalc
	float ratio;

	// Index to into the melody string
	U32 melodyIdx;
	
	// Length of the melody string
	U32 melodyLen;

} pvt_data_t;

#define MSEC_PER_COUNT 250
static void xGetMelodyToneAndDuration(char note, char count, U32 *freq, U32 *sampleMSec)
{
	switch (note) {
		case 'A':
			*freq = 220;
			break;
		case 'B':
			*freq = 246;
			break;
		case 'C':
			*freq = 261;
			break;
		case 'D':
			*freq = 293;
			break;
		case 'E':
			*freq = 329;
			break;
		case 'F':
			*freq = 349;
			break;
		case 'G':
			*freq = 391;
			break;
		case 'a':
			*freq = 440;
			break;
		case 'b':
			*freq = 493;
			break;
		case 'c':
			*freq = 523;
			break;
		case 'd':
			*freq = 587;
			break;
		case 'e':
			*freq = 659;
			break;
		case 'f':
			*freq = 698;
			break;
		case 'g':
			*freq = 783;
			break;
		case '-':
		default:
			*freq = 0;			
			break;
	}
	
	switch (count) {
		case '1':
			*sampleMSec = MSEC_PER_COUNT * 1;
			break;
		case '2':
			*sampleMSec = MSEC_PER_COUNT * 2;
			break;
		case '3':
			*sampleMSec = MSEC_PER_COUNT * 3;
			break;
		case '4':
			*sampleMSec = MSEC_PER_COUNT * 4;
			break;
		default:
			*sampleMSec = MSEC_PER_COUNT * 4;
			break;
	}
}

static bool xSupportedMappingFormat(media_q_t *pMediaQ)
{
	if (pMediaQ) {
		if (pMediaQ->pMediaQDataFormat) {
			if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0 || strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfToneGenCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	
	char *pEnd;
	U32 val;

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo;
		pPubMapUncmpAudioInfo = (media_q_pub_map_uncmp_audio_info_t *)pMediaQ->pPubMapInfo;
		if (!pPubMapUncmpAudioInfo) {
			AVB_LOG_ERROR("Public map data for audio info not allocated.");
			return;
		}

		if (strcmp(name, "intf_nv_tone_hz") == 0) {
			pPvtData->toneHz = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_on_off_interval_msec") == 0) {
			pPvtData->onOffIntervalMSec = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_melody_string") == 0) {
			if (pPvtData->pMelodyString)
				free(pPvtData->pMelodyString);
			pPvtData->pMelodyString = strdup(value);
			if (pPvtData->pMelodyString) {
				pPvtData->melodyLen = strlen(pPvtData->pMelodyString);
			}
		}

		else if (strcmp(name, "intf_nv_audio_rate") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_RATE_8KHZ && val <= AVB_AUDIO_RATE_192KHZ) {
				pPvtData->audioRate = (avb_audio_rate_t)val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
				pPvtData->audioRate = AVB_AUDIO_RATE_44_1KHZ;
			}

			// Give the audio parameters to the mapping module.
			if (xSupportedMappingFormat(pMediaQ)) {
				pPubMapUncmpAudioInfo->audioRate = pPvtData->audioRate;
			}
		}

		else if (strcmp(name, "intf_nv_audio_bit_depth") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_BIT_DEPTH_1BIT && val <= AVB_AUDIO_BIT_DEPTH_64BIT) {
				pPvtData->audioBitDepth = (avb_audio_bit_depth_t)val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_bits.");
				pPvtData->audioBitDepth = AVB_AUDIO_BIT_DEPTH_24BIT;
			}

			// Give the audio parameters to the mapping module.
			if (xSupportedMappingFormat(pMediaQ)) {
				pPubMapUncmpAudioInfo->audioBitDepth = pPvtData->audioBitDepth;
			}
		}

		else if (strcmp(name, "intf_nv_audio_type") == 0) {
			if (strncasecmp(value, "float", 5) == 0)
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_TYPE_FLOAT;
			else if (strncasecmp(value, "sign", 4) == 0 || strncasecmp(value, "int", 4) == 0)
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_TYPE_INT;
			else if (strncasecmp(value, "unsign", 6) == 0 || strncasecmp(value, "uint", 4) == 0)
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_TYPE_UINT;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_type.");
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_TYPE_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (xSupportedMappingFormat(pMediaQ)) {
				pPubMapUncmpAudioInfo->audioType = (avb_audio_type_t)pPvtData->audioType;
			}
		}

		else if (strcmp(name, "intf_nv_audio_endian") == 0) {
			if (strncasecmp(value, "big", 3) == 0)
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_ENDIAN_BIG;
			else if (strncasecmp(value, "little", 6) == 0)
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_ENDIAN_LITTLE;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_endian.");
				pPvtData->audioType = (avb_audio_bit_depth_t)AVB_AUDIO_ENDIAN_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (xSupportedMappingFormat(pMediaQ)) {
				pPubMapUncmpAudioInfo->audioEndian = (avb_audio_endian_t)pPvtData->audioEndian;
			}
		}

		else if (strcmp(name, "intf_nv_audio_channels") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_CHANNELS_1 && val <= AVB_AUDIO_CHANNELS_8) {
				pPvtData->audioChannels = (avb_audio_channels_t)val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio channels configured for intf_nv_audio_channels.");
				pPvtData->audioChannels = (avb_audio_channels_t)AVB_AUDIO_CHANNELS_2;
			}

			// Give the audio parameters to the mapping module.
			if (xSupportedMappingFormat(pMediaQ)) {
				pPubMapUncmpAudioInfo->audioChannels = pPvtData->audioChannels;
			}
		}
	
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfToneGenGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfToneGenTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		
		// U8 b;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo;
		pPubMapUncmpAudioInfo = (media_q_pub_map_uncmp_audio_info_t *)pMediaQ->pPubMapInfo;
		if (!pPubMapUncmpAudioInfo) {
			AVB_LOG_ERROR("Public map data for audio info not allocated.");
			return;
		}
		
		// Will get toggle on at the first tx cb
		if (pPvtData->onOffIntervalMSec > 0) {
			pPvtData->freq = pPvtData->toneHz;
			pPvtData->freqCountdown = 0;
		}
		else {
			pPvtData->freq = 0;
			pPvtData->freqCountdown = 0;
		}
		
		pPvtData->melodyIdx = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfToneGenTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		if (pPvtData->intervalCounter++ % pPubMapUncmpAudioInfo->packingFactor != 0)
			return TRUE;

		media_q_item_t *pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			if (pMediaQItem->itemSize < pPubMapUncmpAudioInfo->itemSize) {
				AVB_LOG_ERROR("Media queue item not large enough for samples");
			}

			// Tone on
			static U32 runningFrameCnt = 0;
			U32 frameCnt;
			U32 channelCnt;
			U32 idx = 0;
			for (frameCnt = 0; frameCnt < pPubMapUncmpAudioInfo->framesPerItem; frameCnt++) {

				// Check for tone on / off toggle
				if (!pPvtData->freqCountdown) {
					if (pPvtData->pMelodyString) {
						// Melody logic
						U32 intervalMSec;
						xGetMelodyToneAndDuration(
							pPvtData->pMelodyString[pPvtData->melodyIdx],
							pPvtData->pMelodyString[pPvtData->melodyIdx + 1],
							&pPvtData->freq, &intervalMSec);
							pPvtData->melodyIdx += 2;
							
							pPvtData->freqCountdown = (pPubMapUncmpAudioInfo->audioRate / 1000) * intervalMSec;
							if (pPvtData->melodyIdx >= pPvtData->melodyLen)
								pPvtData->melodyIdx = 0;
					}
					else {
						// Fixed tone
						if (pPvtData->onOffIntervalMSec > 0) {
							if (pPvtData->freq == 0)
								pPvtData->freq = pPvtData->toneHz;
							else
								pPvtData->freq = 0;
							pPvtData->freqCountdown = (pPubMapUncmpAudioInfo->audioRate / 1000) * pPvtData->onOffIntervalMSec;
						}
						else {
							pPvtData->freqCountdown = pPubMapUncmpAudioInfo->audioRate;		// Just run steady for 1 sec
						}
					}
					pPvtData->ratio = pPvtData->freq / (float)pPubMapUncmpAudioInfo->audioRate;
				}
				pPvtData->freqCountdown--;


				float value = SIN(2 * PI * (runningFrameCnt++ % pPubMapUncmpAudioInfo->audioRate) * pPvtData->ratio);

				if (pPubMapUncmpAudioInfo->itemSampleSizeBytes == 2) {
					// 16 bit audio
					S16 sample = (S16)(value * 15000);
					for (channelCnt = 0; channelCnt < pPubMapUncmpAudioInfo->audioChannels; channelCnt++) {
						unsigned char c;
						U8 *pData = pMediaQItem->pPubData;
						c = (unsigned)sample % 256;
						pData[idx++] = c;
						c = (unsigned)sample / 256 % 256;
						pData[idx++] = c;
					}
				}
				else {
					// CORE_TODO
					AVB_LOG_ERROR("Audio sample size format not implemented yet for tone generator interface module");
				}
			}
			
			pMediaQItem->dataLen = pPubMapUncmpAudioInfo->itemSize;

			openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
			openavbMediaQHeadPush(pMediaQ);

			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return TRUE;
		}
		else {
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return FALSE;	// Media queue full
		}
	}
	
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfToneGenRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfToneGenRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return FALSE;
}

// This callback will be called when the interface needs to be closed. All shutdown should 
// occur in this function.
void openavbIntfToneGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfToneGenGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfToneGenInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfToneGenCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfToneGenGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfToneGenTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfToneGenTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfToneGenRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfToneGenRxCB;
		pIntfCB->intf_end_cb = openavbIntfToneGenEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfToneGenGenEndCB;

		pPvtData->intervalCounter = 0;
		pPvtData->melodyIdx = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
