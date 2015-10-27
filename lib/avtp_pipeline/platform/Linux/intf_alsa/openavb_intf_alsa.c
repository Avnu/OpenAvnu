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
* MODULE SUMMARY : ALSA interface module.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "openavb_types_pub.h"
#include "openavb_audio_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_uncmp_audio_pub.h"
#include "openavb_map_aaf_audio_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"ALSA Interface"
#include "openavb_log_pub.h"

// The asoundlib.h header needs to appear after openavb_trace_pub.h otherwise an incompatibtily version of time.h gets pulled in.
#include <alsa/asoundlib.h>

#define PCM_DEVICE_NAME_DEFAULT	"default"
#define PCM_ACCESS_TYPE			SND_PCM_ACCESS_RW_INTERLEAVED

typedef struct {
	/////////////
	// Config data
	/////////////
	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	// ALSA Device name
	char *pDeviceName;

	// map_nv_audio_rate
	avb_audio_rate_t audioRate;

	// map_nv_audio_type
	avb_audio_type_t audioType;

	// map_nv_audio_bit_depth
	avb_audio_bit_depth_t audioBitDepth;

	// map_nv_audio_endian
	avb_audio_endian_t audioEndian;

	// map_nv_channels
	avb_audio_channels_t audioChannels;

	// map_nv_allow_resampling
	bool allowResampling;

	U32 startThresholdPeriods;

	U32 periodTimeUsec;

	/////////////
	// Variable data
	/////////////
	// Handle for the PCM device
	snd_pcm_t *pcmHandle;

	// ALSA stream
	snd_pcm_stream_t pcmStream;

	// ALSA read/write interval
	U32 intervalCounter;
} pvt_data_t;


static snd_pcm_format_t x_AVBAudioFormatToAlsaFormat(avb_audio_type_t type,
											  avb_audio_bit_depth_t bitDepth,
											  avb_audio_endian_t endian,
											  char const* pMediaQDataFormat)
{
	bool tight = FALSE;
	if (bitDepth == AVB_AUDIO_BIT_DEPTH_24BIT) {
		if (pMediaQDataFormat != NULL
			&& (strcmp(pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0
			|| strcmp(pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0)) {
			tight = TRUE;
		}
	}

	if (type == AVB_AUDIO_TYPE_FLOAT) {
		switch (bitDepth) {
			case AVB_AUDIO_BIT_DEPTH_32BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_FLOAT_BE;
				else
					return SND_PCM_FORMAT_FLOAT_LE;
			default:
				AVB_LOGF_ERROR("Unsupported audio bit depth for float: %d", bitDepth);
			break;
		}
	}
	else if (type == AVB_AUDIO_TYPE_UINT) {
		switch (bitDepth) {
		case AVB_AUDIO_BIT_DEPTH_8BIT:
			return SND_PCM_FORMAT_U8;
		case AVB_AUDIO_BIT_DEPTH_16BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_U16_BE;
				else
					return SND_PCM_FORMAT_U16_LE;
		case AVB_AUDIO_BIT_DEPTH_20BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_U20_3BE;
				else
					return SND_PCM_FORMAT_U20_3LE;
		case AVB_AUDIO_BIT_DEPTH_24BIT:
		    if (tight) {
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_U24_3BE;
				else
					return SND_PCM_FORMAT_U24_3LE;
			}
			else {
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_U24_BE;
				else
					return SND_PCM_FORMAT_U24_LE;
			}
		case AVB_AUDIO_BIT_DEPTH_32BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_U32_BE;
				else
					return SND_PCM_FORMAT_U32_LE;
			case AVB_AUDIO_BIT_DEPTH_1BIT:
		case AVB_AUDIO_BIT_DEPTH_48BIT:
		case AVB_AUDIO_BIT_DEPTH_64BIT:
		default:
				AVB_LOGF_ERROR("Unsupported integer audio bit depth: %d", bitDepth);
			break;
		}
	}
	else {
		// AVB_AUDIO_TYPE_INT
		// or unspecified (defaults to signed int)
		switch (bitDepth) {
			case AVB_AUDIO_BIT_DEPTH_8BIT:
				// 8bit samples don't worry about endianness,
				// but default to unsigned instead of signed.
				if (type == AVB_AUDIO_TYPE_INT)
					return SND_PCM_FORMAT_S8;
				else // default
					return SND_PCM_FORMAT_U8;
			case AVB_AUDIO_BIT_DEPTH_16BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_S16_BE;
				else
					return SND_PCM_FORMAT_S16_LE;
			case AVB_AUDIO_BIT_DEPTH_20BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_S20_3BE;
				else
					return SND_PCM_FORMAT_S20_3LE;
			case AVB_AUDIO_BIT_DEPTH_24BIT:
				if (tight) {
					if (endian == AVB_AUDIO_ENDIAN_BIG)
						return SND_PCM_FORMAT_S24_3BE;
					else
						return SND_PCM_FORMAT_S24_3LE;
				}
				else {
					if (endian == AVB_AUDIO_ENDIAN_BIG)
						return SND_PCM_FORMAT_S24_BE;
					else
						return SND_PCM_FORMAT_S24_LE;
				}
			case AVB_AUDIO_BIT_DEPTH_32BIT:
				if (endian == AVB_AUDIO_ENDIAN_BIG)
					return SND_PCM_FORMAT_S32_BE;
				else
					return SND_PCM_FORMAT_S32_LE;
			case AVB_AUDIO_BIT_DEPTH_1BIT:
			case AVB_AUDIO_BIT_DEPTH_48BIT:
			case AVB_AUDIO_BIT_DEPTH_64BIT:
			default:
				AVB_LOGF_ERROR("Unsupported audio bit depth: %d", bitDepth);
				break;
		}
	}

	return SND_PCM_FORMAT_UNKNOWN;
}


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfAlsaCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	if (pMediaQ) {
		char *pEnd;
		long tmp;
		U32 val;

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


		if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->ignoreTimestamp = (tmp == 1);
			}
		}

		else if (strcmp(name, "intf_nv_device_name") == 0) {
			if (pPvtData->pDeviceName)
				free(pPvtData->pDeviceName);
			pPvtData->pDeviceName = strdup(value);
		}

		else if (strcmp(name, "intf_nv_audio_rate") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_RATE_8KHZ && val <= AVB_AUDIO_RATE_192KHZ) {
				pPvtData->audioRate = val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
				pPvtData->audioRate = AVB_AUDIO_RATE_44_1KHZ;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioRate = pPvtData->audioRate;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
		}

		else if (strcmp(name, "intf_nv_audio_bit_depth") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_BIT_DEPTH_1BIT && val <= AVB_AUDIO_BIT_DEPTH_64BIT) {
				pPvtData->audioBitDepth = val;
			} 
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_bits.");
				pPvtData->audioBitDepth = AVB_AUDIO_BIT_DEPTH_24BIT;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioBitDepth = pPvtData->audioBitDepth;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
		}

		else if (strcmp(name, "intf_nv_audio_type") == 0) {
			if (strncasecmp(value, "float", 5) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_FLOAT;
			else if (strncasecmp(value, "sign", 4) == 0
					 || strncasecmp(value, "int", 4) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_INT;
			else if (strncasecmp(value, "unsign", 6) == 0
					 || strncasecmp(value, "uint", 4) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_UINT;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_type.");
				pPvtData->audioType = AVB_AUDIO_TYPE_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioType = pPvtData->audioType;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
		}

		else if (strcmp(name, "intf_nv_audio_endian") == 0) {
			if (strncasecmp(value, "big", 3) == 0)
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_BIG;
			else if (strncasecmp(value, "little", 6) == 0)
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_LITTLE;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_endian.");
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioEndian = pPvtData->audioEndian;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
		}

		else if (strcmp(name, "intf_nv_audio_channels") == 0) {
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_CHANNELS_1 && val <= AVB_AUDIO_CHANNELS_8) {
				pPvtData->audioChannels = val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio channels configured for intf_nv_audio_channels.");
				pPvtData->audioChannels = AVB_AUDIO_CHANNELS_2;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioChannels = pPvtData->audioChannels;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}

		}

		if (strcmp(name, "intf_nv_allow_resampling") == 0) {
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->allowResampling = (tmp == 1);
			}
		}

		else if (strcmp(name, "intf_nv_start_threshold_periods") == 0) {
			pPvtData->startThresholdPeriods = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_period_time") == 0) {
			pPvtData->periodTimeUsec = strtol(value, &pEnd, 10);
		}

	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfAlsaGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfAlsaTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		S32 rslt;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		// Holds the hardware parameters
		snd_pcm_hw_params_t *hwParams;

		// Open the pcm device.
		rslt = snd_pcm_open(&pPvtData->pcmHandle, pPvtData->pDeviceName, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_open error(): %s", snd_strerror(rslt));
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Allocate the parameter structure
		rslt = snd_pcm_hw_params_malloc(&hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_malloc error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Initialize the hardware paramneters
		rslt = snd_pcm_hw_params_any(pPvtData->pcmHandle, hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_any() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set if resampling allowed.
		rslt = snd_pcm_hw_params_set_rate_resample(pPvtData->pcmHandle, hwParams, pPvtData->allowResampling);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_rate_resample() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the access type
		rslt = snd_pcm_hw_params_set_access(pPvtData->pcmHandle, hwParams, PCM_ACCESS_TYPE);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_access() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the sample format
		int fmt = x_AVBAudioFormatToAlsaFormat(pPvtData->audioType,
											   pPvtData->audioBitDepth,
											   pPvtData->audioEndian, 
											   pMediaQ->pMediaQDataFormat);
		rslt = snd_pcm_hw_params_set_format(pPvtData->pcmHandle, hwParams, fmt);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_format() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the sample rate
		U32 rate = pPvtData->audioRate;
		rslt = snd_pcm_hw_params_set_rate_near(pPvtData->pcmHandle, hwParams, &rate, 0);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_rate_near() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
		if (rate != pPvtData->audioRate) {
			AVB_LOGF_ERROR("Could not set the exact rate. Requested: %u  Using: %u", pPvtData->audioRate, rate);
		}

		// Set the number of channels
		rslt = snd_pcm_hw_params_set_channels(pPvtData->pcmHandle, hwParams, pPvtData->audioChannels);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_channels() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Give the hardware parameters to ALSA
		rslt = snd_pcm_hw_params(pPvtData->pcmHandle, hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Free the hardware parameters
		snd_pcm_hw_params_free(hwParams);
		hwParams = NULL;

		// Get ready for playback
		rslt = snd_pcm_prepare(pPvtData->pcmHandle);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_prepare() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Dump settings
		snd_output_t* out;
		snd_output_stdio_attach(&out, stderr, 0);
		snd_pcm_dump(pPvtData->pcmHandle, out);
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval. 
bool openavbIntfAlsaTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	S32 rslt;

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		media_q_item_t *pMediaQItem = NULL;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}
		//put current wall time into tail item used by AAF mapping module
		if ((pPubMapUncmpAudioInfo->sparseMode != TS_SPARSE_MODE_UNSPEC)) {
			pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
			if ((pMediaQItem) && (pPvtData->intervalCounter % pPubMapUncmpAudioInfo->sparseMode == 0)) {
				openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
			}
			openavbMediaQTailUnlock(pMediaQ);
			pMediaQItem = NULL;
		}

		if (pPvtData->intervalCounter++ % pPubMapUncmpAudioInfo->packingFactor != 0)
			return TRUE;

		pMediaQItem = openavbMediaQHeadLock(pMediaQ);
		if (pMediaQItem) {
			if (pMediaQItem->itemSize < pPubMapUncmpAudioInfo->itemSize) {
				AVB_LOG_ERROR("Media queue item not large enough for samples");
			}

			rslt = snd_pcm_readi(pPvtData->pcmHandle, pMediaQItem->pPubData + pMediaQItem->dataLen, pPubMapUncmpAudioInfo->framesPerItem - (pMediaQItem->dataLen / pPubMapUncmpAudioInfo->itemFrameSizeBytes));

			if (rslt == -EPIPE) {
				AVB_LOGF_ERROR("snd_pcm_readi() error: %s", snd_strerror(rslt));
				rslt = snd_pcm_recover(pPvtData->pcmHandle, rslt, 0);
				if (rslt < 0) {
					AVB_LOGF_ERROR("snd_pcm_recover: %s", snd_strerror(rslt));
				}
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_INTF);
				return FALSE;
			}
			if (rslt < 0) {
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_INTF);
				return FALSE;
			}

			pMediaQItem->dataLen += rslt * pPubMapUncmpAudioInfo->itemFrameSizeBytes;
			if (pMediaQItem->dataLen != pPubMapUncmpAudioInfo->itemSize) {
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
				return TRUE;
			}
			else {
				openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
				openavbMediaQHeadPush(pMediaQ);

				AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
				return TRUE;
			}
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
void openavbIntfAlsaRxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	S32 rslt;

	// Holds the hardware parameters
	snd_pcm_hw_params_t *hwParams;
	snd_pcm_sw_params_t *swParams;

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		// Open the pcm device.
		rslt = snd_pcm_open(&pPvtData->pcmHandle, pPvtData->pDeviceName, SND_PCM_STREAM_PLAYBACK, 0);

		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_open error(): %s", snd_strerror(rslt));
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Allocate the parameter structure
		rslt = snd_pcm_hw_params_malloc(&hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_malloc error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Initialize the hardware paramneters
		rslt = snd_pcm_hw_params_any(pPvtData->pcmHandle, hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_any() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set if resampling allowed.
		rslt = snd_pcm_hw_params_set_rate_resample(pPvtData->pcmHandle, hwParams, pPvtData->allowResampling);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_rate_resample() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the access type
		rslt = snd_pcm_hw_params_set_access(pPvtData->pcmHandle, hwParams, PCM_ACCESS_TYPE);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_access() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the sample format
		int fmt = x_AVBAudioFormatToAlsaFormat(pPvtData->audioType,
											   pPvtData->audioBitDepth,
											   pPvtData->audioEndian, 
											   pMediaQ->pMediaQDataFormat);
		rslt = snd_pcm_hw_params_set_format(pPvtData->pcmHandle, hwParams, fmt);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_format() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Set the sample rate
		U32 rate = pPvtData->audioRate;
		rslt = snd_pcm_hw_params_set_rate_near(pPvtData->pcmHandle, hwParams, &rate, 0);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_rate_near() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
		if (rate != pPvtData->audioRate) {
			AVB_LOGF_ERROR("Could not set the exact rate. Requested: %u  Using: %u", pPvtData->audioRate, rate);
		}

		// Set the number of channels
		rslt = snd_pcm_hw_params_set_channels(pPvtData->pcmHandle, hwParams, pPvtData->audioChannels);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_channels() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}


		// Time based buffer and period setup
		int dir;

		unsigned int buffer_time = 500000;						// ring buffer length in us
		unsigned int period_time = pPvtData->periodTimeUsec;	// period time in us
		int period_event = 1;									// produce poll event after each period
		snd_pcm_uframes_t buffer_size;
		snd_pcm_uframes_t period_size;
		const unsigned int usec_round = 10000;
		unsigned int max;

		// Hard-coded buffer and period times were failing for 192KHz.
		// Check for maximum buffer time and adjust ours down if necessary
		rslt = snd_pcm_hw_params_get_buffer_time_max(hwParams, &max, &dir);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_get_buffer_time_max() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
		else if (max < buffer_time) {
			buffer_time = (max / usec_round) * usec_round;
		}

		// Check for maximum perioid time and adjust ours down if necessary
		rslt = snd_pcm_hw_params_get_period_time_max(hwParams, &max, &dir);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_get_period_time_max() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
		else if (max < period_time) {
			period_time = (max / usec_round) * usec_round;
		}

		rslt = snd_pcm_hw_params_set_buffer_time_near(pPvtData->pcmHandle, hwParams, &buffer_time, &dir);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_buffer_time_near() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_hw_params_get_buffer_size(hwParams, &buffer_size);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_get_buffer_size() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_hw_params_set_period_time_near(pPvtData->pcmHandle, hwParams, &period_time, &dir);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_set_period_time_near() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
		pPvtData->periodTimeUsec = period_time;

		rslt = snd_pcm_hw_params_get_period_size(hwParams, &period_size, &dir);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params_get_period_size() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Give the hardware parameters to ALSA
		rslt = snd_pcm_hw_params(pPvtData->pcmHandle, hwParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_hw_params() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			snd_pcm_hw_params_free(hwParams);
			hwParams = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Free the hardware parameters
		snd_pcm_hw_params_free(hwParams);
		hwParams = NULL;


		// Set software parameters

		// Allocate the parameter structure
		rslt = snd_pcm_sw_params_malloc(&swParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params_malloc error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_sw_params_current(pPvtData->pcmHandle, swParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params_current error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_sw_params_set_start_threshold(pPvtData->pcmHandle, swParams, period_size * pPvtData->startThresholdPeriods);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params_set_start_threshold error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_sw_params_set_avail_min(pPvtData->pcmHandle, swParams, period_event ? buffer_size : period_size);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params_set_avail_min error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_sw_params_set_period_event(pPvtData->pcmHandle, swParams, 1);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params_set_period_event error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		rslt = snd_pcm_sw_params(pPvtData->pcmHandle, swParams);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_sw_params error(): %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Free the hardware parameters
		snd_pcm_sw_params_free(swParams);
		swParams = NULL;


		// Get ready for playback
		rslt = snd_pcm_prepare(pPvtData->pcmHandle);
		if (rslt < 0) {
			AVB_LOGF_ERROR("snd_pcm_prepare() error: %s", snd_strerror(rslt));
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}

		// Dump settings
		snd_output_t* out;
		snd_output_stdio_attach(&out, stderr, 0);
		snd_pcm_dump(pPvtData->pcmHandle, out);
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfAlsaRxCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		bool moreItems = TRUE;

		while (moreItems) {
			media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
			if (pMediaQItem) {
				if (pMediaQItem->dataLen) {
					S32 rslt;

					rslt = snd_pcm_writei(pPvtData->pcmHandle, pMediaQItem->pPubData, pPubMapUncmpAudioInfo->framesPerItem);
					if (rslt < 0) {
						AVB_LOGF_ERROR("snd_pcm_writei: %s", snd_strerror(rslt));
						rslt = snd_pcm_recover(pPvtData->pcmHandle, rslt, 0);
						if (rslt < 0) {
							AVB_LOGF_ERROR("snd_pcm_recover: %s", snd_strerror(rslt));
						}
						rslt = snd_pcm_writei(pPvtData->pcmHandle, pMediaQItem->pPubData, pPubMapUncmpAudioInfo->framesPerItem);
					}
					if (rslt != pPubMapUncmpAudioInfo->framesPerItem) {
						AVB_LOGF_WARNING("Not all pcm data consumed written:%u  consumed:%u", pMediaQItem->dataLen, rslt * pPubMapUncmpAudioInfo->audioChannels);
					}

					// DEBUG
					// rslt = snd_pcm_avail(pPvtData->pcmHandle);
					// AVB_LOGF_INFO("%d\n", rslt);

				}
				else {
				}
				openavbMediaQTailPull(pMediaQ);
			}
			else {
				moreItems = FALSE;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}

// This callback will be called when the interface needs to be closed. All shutdown should 
// occur in this function.
void openavbIntfAlsaEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (pPvtData->pcmHandle) {
			snd_pcm_close(pPvtData->pcmHandle);
			pPvtData->pcmHandle = NULL;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfAlsaGenEndCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfAlsaInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfAlsaCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfAlsaGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfAlsaTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfAlsaTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfAlsaRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfAlsaRxCB;
		pIntfCB->intf_end_cb = openavbIntfAlsaEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfAlsaGenEndCB;

		pPvtData->ignoreTimestamp = FALSE;
		pPvtData->pDeviceName = strdup(PCM_DEVICE_NAME_DEFAULT);
		pPvtData->allowResampling = TRUE;
		pPvtData->intervalCounter = 0;
		pPvtData->startThresholdPeriods = 2;	// Default to 2 periods of frames as the start threshold
		pPvtData->periodTimeUsec = 100000;

	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
