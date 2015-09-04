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
* MODULE SUMMARY : wav File interface module. Talker only.
* 
* This interface module is narrowly focused to read a common wav file format
* and send the data samples to mapping modules.
* 
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "openavb_types_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_uncmp_audio_pub.h"
#include "openavb_map_aaf_audio_pub.h"
#include "openavb_intf_pub.h"

#define	AVB_LOG_COMPONENT	"Wav File Interface"
#include "openavb_log_pub.h" 

typedef struct {
    // RIFF Chunk descriptor
    U8 chunkID[4];			// "RIFF"
	U32 chunkSize;
	U8 format[4];			// "WAVE"

	// FMT Sub Chuck
	U8 subChunk1ID[4];		// "fmt "
	U32 subChunk1Size;
	U16 audioFormat;		// 1 = PCM
	U16 numberChannels;
	U32 sampleRate;
	U32 byteRate;
	U16 blockAlign;
	U16 bitsPerSample;

	// Data Sub Chunk
	U8 subChunk2ID[4];		// "data"
	U32 subChunk2Size;

} wav_file_header_t;


typedef struct {
	/////////////
	// Config data
	/////////////
	// intf_nv_file_name: The fully qualified file name used both the talker and listener.
	char *pFileName;

	/////////////
	// Variable data
	/////////////
	FILE *pFile;

	// ALSA read/write interval
	U32 intervalCounter;

	// map_nv_audio_rate
	avb_audio_rate_t audioRate;

	// map_nv_audio_bit_depth
	avb_audio_bit_depth_t audioBitDepth;

	// map_nv_channels
	avb_audio_channels_t audioChannels;

	// map_nv_audio_endian
	avb_audio_endian_t audioEndian;

	// intf_nv_number_of_data_bytes
	U32 numberOfDataBytes;

} pvt_data_t;

// fread that (mostly) ignores return value - to silence compiler warnings
static inline void ifread(void *ptr, size_t size, size_t num, FILE *stream)
{
	if (fread(ptr, size, num, stream) != num) {
		AVB_LOG_DEBUG("Error reading file");
	}
}

static inline void ifwrite(void *ptr, size_t size, size_t num, FILE *stream)
{
    if (fwrite(ptr, size, num, stream) != num) {
        AVB_LOG_DEBUG("Error writting to file");
    }
}

static void x_parseWaveFile(media_q_t *pMediaQ)
{
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (!pPvtData->pFileName) {
			AVB_LOG_ERROR("Input file name not found.");
			return;
		}

		pPvtData->pFile = fopen(pPvtData->pFileName, "rb");
		if (!pPvtData->pFile) {
			AVB_LOGF_ERROR("Unable to open input file: %s", pPvtData->pFileName);
			return;
		}

		// Check if wav file format is valid and of a supported type.
		wav_file_header_t wavFileHeader;

		// RIFF Chunk
		ifread(wavFileHeader.chunkID, sizeof(wavFileHeader.chunkID), 1, pPvtData->pFile);
		ifread(&wavFileHeader.chunkSize, sizeof(wavFileHeader.chunkSize), 1, pPvtData->pFile);
		ifread(wavFileHeader.format, sizeof(wavFileHeader.format), 1, pPvtData->pFile);

		// FMT sub Chunk
		ifread(wavFileHeader.subChunk1ID, sizeof(wavFileHeader.subChunk1ID), 1, pPvtData->pFile);
		ifread(&wavFileHeader.subChunk1Size, sizeof(wavFileHeader.subChunk1Size), 1, pPvtData->pFile);
		ifread(&wavFileHeader.audioFormat, sizeof(wavFileHeader.audioFormat), 1, pPvtData->pFile);
		ifread(&wavFileHeader.numberChannels, sizeof(wavFileHeader.numberChannels), 1, pPvtData->pFile);
		ifread(&wavFileHeader.sampleRate, sizeof(wavFileHeader.sampleRate), 1, pPvtData->pFile);
		ifread(&wavFileHeader.byteRate, sizeof(wavFileHeader.byteRate), 1, pPvtData->pFile);
		ifread(&wavFileHeader.blockAlign, sizeof(wavFileHeader.blockAlign), 1, pPvtData->pFile);
		ifread(&wavFileHeader.bitsPerSample, sizeof(wavFileHeader.bitsPerSample), 1, pPvtData->pFile);

		// Data sub Chunk
		ifread(wavFileHeader.subChunk2ID, sizeof(wavFileHeader.subChunk2ID), 1, pPvtData->pFile);
		ifread(&wavFileHeader.subChunk2Size, sizeof(wavFileHeader.subChunk2Size), 1, pPvtData->pFile);

		AVB_LOGF_INFO("Number of data bytes:%d", wavFileHeader.subChunk2Size);

		// Make sure wav file format is supported
		if (memcmp(wavFileHeader.chunkID, "RIFF", 4) != 0) {
			AVB_LOGF_ERROR("%s does not appear to be a supported wav file.", pPvtData->pFileName);
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
			return;
		}

		if (memcmp(wavFileHeader.format, "WAVE", 4) != 0) {
			AVB_LOGF_ERROR("%s does not appear to be a supported wav file.", pPvtData->pFileName);
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
			return;
		}

		if (memcmp(wavFileHeader.subChunk1ID, "fmt ", 4) != 0) {
			AVB_LOGF_ERROR("%s does not appear to be a supported wav file.", pPvtData->pFileName);
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
			return;
		}

		if (memcmp(wavFileHeader.subChunk2ID, "data", 4) != 0) {
			AVB_LOGF_ERROR("%s does not appear to be a supported wav file.", pPvtData->pFileName);
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
			return;
		}

		if (wavFileHeader.audioFormat != 1) {
			AVB_LOGF_ERROR("%s does not appear to be a supported wav file.", pPvtData->pFileName);
			fclose(pPvtData->pFile);
			pPvtData->pFile = NULL;
			return;
		}

		// Give the audio parameters to the mapping module.
		if (pMediaQ->pMediaQDataFormat) {
			if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
				|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
				media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo;
				pPubMapUncmpAudioInfo = (media_q_pub_map_uncmp_audio_info_t *)pMediaQ->pPubMapInfo;
				pPubMapUncmpAudioInfo->audioRate = wavFileHeader.sampleRate;
				pPubMapUncmpAudioInfo->audioBitDepth = wavFileHeader.bitsPerSample;
				pPubMapUncmpAudioInfo->audioChannels = wavFileHeader.numberChannels;
				pPubMapUncmpAudioInfo->audioEndian = pPvtData->audioEndian;

				AVB_LOGF_INFO("Wav file - Rate:%d Bits:%d Channels:%d", pPubMapUncmpAudioInfo->audioRate, pPubMapUncmpAudioInfo->audioBitDepth, pPubMapUncmpAudioInfo->audioChannels);
			}
			else {
				AVB_LOG_ERROR("MediaQ mapping format not supported in this interface module.");
			}
		}
		else {
			AVB_LOG_ERROR("MediaQ mapping format not defined.");
		}
	}
}

static void passParamToMapModule(media_q_t *pMediaQ)
{
    if (pMediaQ) {
        media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = (media_q_pub_map_uncmp_audio_info_t *)pMediaQ->pPubMapInfo;
        pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

        if (!pPubMapUncmpAudioInfo) {
            AVB_LOG_ERROR("Public interface module data not allocated.");
            return;
        }

        if (!pPvtData) {
            AVB_LOG_ERROR("Private interface module data not allocated.");
            return;
        }

       if (pMediaQ->pMediaQDataFormat) {
            if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
                || strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
                pPubMapUncmpAudioInfo->audioRate = pPvtData->audioRate;
                pPubMapUncmpAudioInfo->audioBitDepth = pPvtData->audioBitDepth;
                pPubMapUncmpAudioInfo->audioChannels = pPvtData->audioChannels;
                pPubMapUncmpAudioInfo->audioEndian = pPvtData->audioEndian;
            }
        }
    }
}

// CORE_TODO : this version of convertEndianess is in the commit process but didn't appear to work as expected. 
// As part of a separate merge the function below this one was pulled in which does work as expected.
#if 0
// little <-> big endian conversion: copy bytes of each
// sample in reverse order back into the buffer
static void convertEndianness(uint8_t *pData, U32 dataLen, U32 sampleSize)
{
	U32 sampleByteIndex = sampleSize;
	U32 itemIndex, readIndex = 0;
	U8 itemData[dataLen];
	for (itemIndex = 0; itemIndex < dataLen; itemIndex++) {
		sampleByteIndex--;
		itemData[itemIndex] = *(pData + readIndex + sampleByteIndex);
		if (sampleByteIndex == 0) {
			sampleByteIndex = sampleSize;
			readIndex = itemIndex + 1;
		}
	}
	memcpy(pData, itemData, dataLen);
}
#endif


#define SWAPU16(x)	(((x) >> 8) | ((x) << 8))
#define SWAPU32(x)	(((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))
static void convertEndianness(uint8_t *pData, U32 dataLen, U32 sampleSize)
{
	if (sampleSize == 2) {
		int i1;
		int cnt = dataLen >> 1;	// 2 bytes at a time
		U16 *pData16 = (U16 *)pData;
		for (i1 = 0; i1 < cnt; i1++) {
			*pData16 = SWAPU16(*pData16);
			pData16++;
		}
	}
	else if (sampleSize == 4) {
		int i1;
		int cnt = dataLen >> 1;	// 2 bytes at a time
		U32 *pData32 = (U32 *)pData;
		for (i1 = 0; i1 < cnt; i1++) {
			*pData32 = SWAPU32(*pData32);
			pData32++;
		}
	}
}

// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfWavFileCfgCB(media_q_t *pMediaQ, const char *name, const char *value) 
{
        AVB_TRACE_ENTRY(AVB_TRACE_INTF);

    if (pMediaQ) {
        char *pEnd;
        U32 val;
        pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
        if (!pPvtData) {
            AVB_LOG_ERROR("Private interface module data not allocated.");
            return;
        }

        if (strcmp(name, "intf_nv_file_name") == 0) {
            if (pPvtData->pFileName) {
                free(pPvtData->pFileName);
            }
            pPvtData->pFileName = strdup(value);
            x_parseWaveFile(pMediaQ);
        }
        else if (strcmp(name, "intf_nv_file_name_rx") == 0) {
            if (pPvtData->pFileName) {
                free(pPvtData->pFileName);
            }
            pPvtData->pFileName = strdup(value);
            passParamToMapModule(pMediaQ);
        }
        else if (strcmp(name, "intf_nv_audio_rate") == 0) {
            val = strtol(value, &pEnd, 10);
            if (val >= AVB_AUDIO_RATE_8KHZ && val <= AVB_AUDIO_RATE_192KHZ) {
                pPvtData->audioRate = val;
            }
            else {
                AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
                pPvtData->audioRate = AVB_AUDIO_RATE_44_1KHZ;
            }
        }
        else if (strcmp(name, "intf_nv_audio_bit_depth") == 0) {
            val = strtol(value, &pEnd, 10);
            if (val >= AVB_AUDIO_BIT_DEPTH_1BIT && val <= AVB_AUDIO_BIT_DEPTH_64BIT) {
                pPvtData->audioBitDepth = val;
            }
            else {
                AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_bits.");
                pPvtData->audioBitDepth = AVB_AUDIO_BIT_DEPTH_24BIT;
            }
        }
        else if (strcmp(name, "intf_nv_audio_channels") == 0) {
            val = strtol(value, &pEnd, 10);
            if (val >= AVB_AUDIO_CHANNELS_1 && val <= AVB_AUDIO_CHANNELS_8) {
                pPvtData->audioChannels = val;
            }
            else {
                AVB_LOG_ERROR("Invalid audio channels configured for intf_nv_audio_channels.");
                pPvtData->audioChannels = AVB_AUDIO_CHANNELS_2;
            }
        }
        else if (strcmp(name, "intf_nv_number_of_data_bytes") == 0) {
            val = strtol(value, &pEnd, 10);
            if (val > 0) {
                pPvtData->numberOfDataBytes = val;
            }
            else {
                AVB_LOG_ERROR("Invalid number of data bytes for intf_nv_number_of_data_bytes.");
            }
        }
       else if (strcmp(name, "intf_nv_audio_endian") == 0) {
            if (strncasecmp(value, "big", 3) == 0) {
                pPvtData->audioEndian = AVB_AUDIO_ENDIAN_BIG;
                AVB_LOG_INFO("Forced audio samples endian conversion: little <-> big");
            }
        }
    }
    AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfWavFileGenInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
			// WAV files are little-endian, AAF (SAF) need big-endian
			pPvtData->audioEndian = AVB_AUDIO_ENDIAN_BIG;
		}
		else {
			pPvtData->audioEndian = AVB_AUDIO_ENDIAN_LITTLE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfWavFileTxInitCB(media_q_t *pMediaQ) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		// U8 b;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

		if (!pPvtData->pFile) {
			pPvtData->pFile = fopen(pPvtData->pFileName, "rb");
			if (!pPvtData->pFile) {
				AVB_LOGF_ERROR("Unable to open input file: %s", pPvtData->pFileName);
				return;
			}
		}

		if (pPvtData->pFile) {
			// Seek to start of data for our only supported wav file format.
			fseek(pPvtData->pFile, 44, 0);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval. Commonly this will be
// 4000 or 8000 times  per second.
bool openavbIntfWavFileTxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		media_q_item_t *pMediaQItem = NULL;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return FALSE;
		}

		//put current wall time into tail item used by AAF maping module
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

			if (pPvtData->pFile) {

				U32 bytesRead = fread(pMediaQItem->pPubData, 1, pPubMapUncmpAudioInfo->itemSize, pPvtData->pFile);

				if (bytesRead < pPubMapUncmpAudioInfo->itemSize) {
					// Pad reminder of item with anything we didn't read because of end of file.
					memset(pMediaQItem->pPubData + bytesRead, 0x00, pPubMapUncmpAudioInfo->itemSize - bytesRead);

					// Repeat wav file. Seek to start of data for our only supported wav file format.
					fseek(pPvtData->pFile, 44, 0);
				}
				pMediaQItem->dataLen = pPubMapUncmpAudioInfo->itemSize;

				if (pPvtData->audioEndian == AVB_AUDIO_ENDIAN_BIG) {
					convertEndianness((uint8_t *)pMediaQItem->pPubData, pMediaQItem->dataLen, pPubMapUncmpAudioInfo->itemSampleSizeBytes);
				}

				openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
				openavbMediaQHeadPush(pMediaQ);

				AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
				return TRUE;
			}
			else {
				openavbMediaQHeadUnlock(pMediaQ);
				AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
				return FALSE;	// No File handle
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
void openavbIntfWavFileRxInitCB(media_q_t *pMediaQ) 
{
    AVB_TRACE_ENTRY(AVB_TRACE_INTF);

    if (pMediaQ) {
        pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
        struct stat buf;
        if (!pPvtData) {
            AVB_LOG_ERROR("Private interface module data not allocated.");
            return;
        }

        if (!pPvtData->pFileName) {
            AVB_LOG_ERROR("Output wav file not provided in ini");
            AVB_TRACE_EXIT(AVB_TRACE_INTF);
        }
        else if (stat(pPvtData->pFileName, &buf) == 0) {
            AVB_LOGF_ERROR("Will not open output file: %s  file exists", pPvtData->pFileName);
            AVB_TRACE_EXIT(AVB_TRACE_INTF);
            return;
        }
        else {
                AVB_LOGF_INFO("Creating output wav file: %s", pPvtData->pFileName);
                pPvtData->pFile = fopen(pPvtData->pFileName, "wb");
                if (!pPvtData->pFile) {
                    AVB_LOGF_ERROR("Unable to open output wav file: %s", pPvtData->pFileName);
                    AVB_TRACE_EXIT(AVB_TRACE_INTF);
                    return;
                }
                wav_file_header_t wavFileHeader;
                memcpy(wavFileHeader.chunkID, "RIFF", sizeof(wavFileHeader.chunkID));
                memcpy(wavFileHeader.format, "WAVE", sizeof(wavFileHeader.format));
                memcpy(wavFileHeader.subChunk1ID, "fmt ", sizeof(wavFileHeader.subChunk1ID));
                wavFileHeader.subChunk1Size = 0x10;
                wavFileHeader.audioFormat = 0x01;
                wavFileHeader.numberChannels = pPvtData->audioChannels;
                wavFileHeader.sampleRate = pPvtData->audioRate;
                wavFileHeader.bitsPerSample = pPvtData->audioBitDepth;
                wavFileHeader.byteRate = wavFileHeader.sampleRate * wavFileHeader.numberChannels * wavFileHeader.bitsPerSample/8;
                wavFileHeader.blockAlign = wavFileHeader.numberChannels * wavFileHeader.bitsPerSample/8;
                memcpy(wavFileHeader.subChunk2ID, "data", sizeof(wavFileHeader.subChunk2ID));
                wavFileHeader.subChunk2Size = pPvtData->numberOfDataBytes;
                wavFileHeader.chunkSize = 4 + (8 + wavFileHeader.subChunk1Size) + (8 + wavFileHeader.subChunk2Size);

                ifwrite(&wavFileHeader.chunkID, sizeof(wavFileHeader.chunkID), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.chunkSize, sizeof(wavFileHeader.chunkSize), 1, pPvtData->pFile);
                ifwrite(wavFileHeader.format, sizeof(wavFileHeader.format), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.subChunk1ID, sizeof(wavFileHeader.subChunk1ID), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.subChunk1Size, sizeof(wavFileHeader.subChunk1Size), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.audioFormat, sizeof(wavFileHeader.audioFormat), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.numberChannels, sizeof(wavFileHeader.numberChannels), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.sampleRate, sizeof(wavFileHeader.sampleRate), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.byteRate, sizeof(wavFileHeader.byteRate), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.blockAlign, sizeof(wavFileHeader.blockAlign), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.bitsPerSample, sizeof(wavFileHeader.bitsPerSample), 1, pPvtData->pFile);
                ifwrite(wavFileHeader.subChunk2ID, sizeof(wavFileHeader.subChunk2ID), 1, pPvtData->pFile);
                ifwrite(&wavFileHeader.subChunk2Size, sizeof(wavFileHeader.subChunk2Size), 1, pPvtData->pFile);
            }
        }
    AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfWavFileRxCB(media_q_t *pMediaQ) 
{
    AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);

    if (pMediaQ) {
        pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;

        if (!pPvtData) {
            AVB_LOG_ERROR("Private interface module data not allocated.");
            return FALSE;
        }

        bool moreData = TRUE;
        size_t written;
        static U32 numOfStoredDataBytes = 0;       //total number of data bytes stored in file so far
        static bool fileReady = FALSE;             //set when writing to file is finished
        bool expectedNumberOfDataReceived = FALSE; //set when expected number of data bytes has been received

        while (moreData) {
            media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, TRUE);
            if ((pMediaQItem) && (fileReady == FALSE)) {
                if (pPvtData->pFile && pMediaQItem->dataLen > 0) {
                    if (expectedNumberOfDataReceived == FALSE) {
                        if ((numOfStoredDataBytes + pMediaQItem->dataLen ) > pPvtData->numberOfDataBytes) {
                            pMediaQItem->dataLen = pPvtData->numberOfDataBytes - numOfStoredDataBytes;
                            expectedNumberOfDataReceived = TRUE;
                        }
                    }
					if (pPvtData->audioEndian == AVB_AUDIO_ENDIAN_BIG) {
                        convertEndianness((uint8_t *)pMediaQItem->pPubData, pMediaQItem->dataLen, pPubMapUncmpAudioInfo->itemSampleSizeBytes);
                    }
                    written = fwrite(pMediaQItem->pPubData, 1, pMediaQItem->dataLen, pPvtData->pFile);
                    if (written != pMediaQItem->dataLen) {
                        int e = ferror(pPvtData->pFile);
                        AVB_LOGF_ERROR("Error writing file: %s, %s", pPvtData->pFileName, strerror(e));
                        fclose(pPvtData->pFile);
                        pPvtData->pFile = NULL;
                    }
                    else {
                        pMediaQItem->dataLen = 0;
                        numOfStoredDataBytes += written;
                        if (expectedNumberOfDataReceived == TRUE) {
                            fileReady = TRUE;
                            AVB_LOG_INFO("Wav file ready.");
                        }
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
void openavbIntfWavFileEndCB(media_q_t *pMediaQ) 
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

void openavbIntfWavFileGenEndCB(media_q_t *pMediaQ) 
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
extern DLL_EXPORT bool openavbIntfWavFileInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;

		pIntfCB->intf_cfg_cb = openavbIntfWavFileCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfWavFileGenInitCB;
		pIntfCB->intf_tx_init_cb = openavbIntfWavFileTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfWavFileTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfWavFileRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfWavFileRxCB;
		pIntfCB->intf_end_cb = openavbIntfWavFileEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfWavFileGenEndCB;
		pPvtData->audioEndian = AVB_AUDIO_ENDIAN_LITTLE;		//wave file default

		pPvtData->intervalCounter = 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
