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
 ******************************************************************
 * MODULE : AEM - AVDECC Stream IO Descriptor
 * MODULE SUMMARY : Implements the AVDECC Stream IO IEEE Std 1722.1-2013 clause 7.2.6
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_stream_io.h"

////////////////////////////////
// Private (internal) functions
////////////////////////////////

openavbRC openavbAemDescriptorStreamIOToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_stream_io_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_STREAM_IO_BASE_LENGTH + (pDescriptor->number_of_formats * OPENAVB_DESCRIPTOR_STREAM_IO_FORMAT_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_stream_io_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->object_name);
	BIT_D2BHTONS(pDst, pSrc->localized_description.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->localized_description.index, 0, 2);
	OCT_D2BHTONS(pDst, pSrc->clock_domain_index);
	OCT_D2BHTONS(pDst, pSrc->stream_flags);
	openavbAemStreamFormatToBuf(&pSrc->current_format, pDst);
	pDst += 8;
	OCT_D2BHTONS(pDst, pSrc->formats_offset);
	OCT_D2BHTONS(pDst, pSrc->number_of_formats);
	OCT_D2BMEMCP(pDst, pSrc->backup_talker_entity_id_0);
	OCT_D2BHTONS(pDst, pSrc->backup_talker_unique_id_0);
	OCT_D2BMEMCP(pDst, pSrc->backup_talker_entity_id_1);
	OCT_D2BHTONS(pDst, pSrc->backup_talker_unique_id_1);
	OCT_D2BMEMCP(pDst, pSrc->backup_talker_entity_id_2);
	OCT_D2BHTONS(pDst, pSrc->backup_talker_unique_id_2);
	OCT_D2BMEMCP(pDst, pSrc->backedup_talker_entity_id);
	OCT_D2BHTONS(pDst, pSrc->backedup_talker_unique_id);
	OCT_D2BHTONS(pDst, pSrc->avb_interface_index);
	OCT_D2BHTONL(pDst, pSrc->buffer_length);

	int i1;
	for (i1 = 0; i1 < pSrc->number_of_formats; i1++) {
		openavbAemStreamFormatToBuf(&pSrc->stream_formats[i1], pDst);
		pDst += 8;
	}

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}


void x_openavbAemStreamFormatFromBuf(openavb_aem_stream_format_t *pDst, U8 *pBuf)
{
	U8 *pSrc = pBuf;

	// Tightly bound to caller not safety checks needed.

	BIT_B2DNTOHB(pDst->v, pSrc, 0x80, 7, 0);
	if (pDst->v == 0) {
		BIT_B2DNTOHB(pDst->subtype, pSrc, 0x7f, 0, 1);
		switch (pDst->subtype) {
			case OPENAVB_AEM_STREAM_FORMAT_61883_IIDC_SUBTYPE:
				{
					BIT_B2DNTOHB(pDst->subtypes.iec_61883_iidc.sf, pSrc, 0x80, 7, 0);
					if (pDst->subtypes.iec_61883_iidc.sf == OPENAVB_AEM_STREAM_FORMAT_SF_IIDC) {
						pSrc += 4;
						OCT_B2DNTOHB(pDst->subtypes.iidc.iidc_format, pSrc);
						OCT_B2DNTOHB(pDst->subtypes.iidc.iidc_mode, pSrc);
						OCT_B2DNTOHB(pDst->subtypes.iidc.iidc_rate, pSrc);
					}
					else {
						BIT_B2DNTOHB(pDst->subtypes.iec_61883.sf, pSrc, 0x80, 7, 0);
						BIT_B2DNTOHB(pDst->subtypes.iec_61883.fmt, pSrc, 0x7e, 1, 1);
						if (pDst->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_4) {
							// Nothingn to do
						}
						else if (pDst->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_6) {
							BIT_B2DNTOHB(pDst->subtypes.iec_61883_6.fdf_evt, pSrc, 0xf8, 3, 0);
							BIT_B2DNTOHB(pDst->subtypes.iec_61883_6.fdf_sfc, pSrc, 0x07, 0, 1);
							OCT_B2DNTOHB(pDst->subtypes.iec_61883_6.dbs, pSrc);
							BIT_B2DNTOHB(pDst->subtypes.iec_61883_6.b, pSrc, 0x80, 7, 0);
							BIT_B2DNTOHB(pDst->subtypes.iec_61883_6.nb, pSrc, 0x40, 6, 1);

							if (pDst->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_32BITS) {
								// Nothing to do
							}
							else if (pDst->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_FLOAT) {
								// Nothing to do
							}
							else if (pDst->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_AM824) {
								OCT_B2DNTOHB(pDst->subtypes.iec_61883_6_am824.label_iec_60958_cnt, pSrc);
								OCT_B2DNTOHB(pDst->subtypes.iec_61883_6_am824.label_mbla_cnt, pSrc);
								BIT_B2DNTOHB(pDst->subtypes.iec_61883_6_am824.label_midi_cnt, pSrc, 0xf0, 4, 0);
								BIT_B2DNTOHB(pDst->subtypes.iec_61883_6_am824.label_smptecnt, pSrc, 0x0f, 0, 1);
							}
						}

						else if (pDst->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_8) {
							pSrc += 3;
							OCT_B2DNTOHB(pDst->subtypes.iec_61883_8.video_mode, pSrc);
							OCT_B2DNTOHB(pDst->subtypes.iec_61883_8.compress_mode, pSrc);
							OCT_B2DNTOHB(pDst->subtypes.iec_61883_8.color_space, pSrc);
						}
					}
				}

				break;
			case OPENAVB_AEM_STREAM_FORMAT_MMA_SUBTYPE:
				// Nothing to do
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_AUDIO_SUBTYPE:
				BIT_B2DNTOHB(pDst->subtypes.avtp_audio.nominal_sample_rate, pSrc, 0x0f, 0, 1);
				OCT_B2DNTOHB(pDst->subtypes.avtp_audio.format, pSrc);
				OCT_B2DNTOHB(pDst->subtypes.avtp_audio.bit_depth, pSrc);
				BIT_B2DNTOHL(pDst->subtypes.avtp_audio.channels_per_frame, pSrc, 0xffc00000, 22, 0);
				BIT_B2DNTOHL(pDst->subtypes.avtp_audio.samples_per_frame, pSrc, 0x003ff000, 12, 4);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_VIDEO_SUBTYPE:
				OCT_B2DNTOHB(pDst->subtypes.avtp_video.format, pSrc);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_CONTROL_SUBTYPE:
				BIT_B2DNTOHB(pDst->subtypes.avtp_control.protocol_type, pSrc, 0xf0, 4, 1);
				OCT_B2DMEMCP(pDst->subtypes.avtp_control.format_id, pSrc);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_VENDOR_SUBTYPE:
				pSrc++;
				OCT_B2DMEMCP(pDst->subtypes.vendor.format_id, pSrc);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_EXPERIMENTAL_SUBTYPE:
				// Nothing to do
				break;
		}
	}
	else {
		pSrc++;
		OCT_B2DMEMCP(pDst->subtypes.vendor_specific.format_id, pSrc);
	}
}


openavbRC openavbAemDescriptorStreamIOFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_stream_io_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_STREAM_IO_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_stream_io_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->object_name, pSrc);
	BIT_B2DNTOHS(pDst->localized_description.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->localized_description.index, pSrc, 0x0007, 0, 2);
	OCT_B2DNTOHS(pDst->clock_domain_index, pSrc);
	OCT_B2DNTOHS(pDst->stream_flags, pSrc);
	x_openavbAemStreamFormatFromBuf(&pDst->current_format, pSrc);
	pSrc += 8;
	OCT_B2DNTOHS(pDst->formats_offset, pSrc);
	OCT_B2DNTOHS(pDst->number_of_formats, pSrc);
	OCT_B2DMEMCP(pDst->backup_talker_entity_id_0, pSrc);
	OCT_B2DNTOHS(pDst->backup_talker_unique_id_0, pSrc);
	OCT_B2DMEMCP(pDst->backup_talker_entity_id_1, pSrc);
	OCT_B2DNTOHS(pDst->backup_talker_unique_id_1, pSrc);
	OCT_B2DMEMCP(pDst->backup_talker_entity_id_2, pSrc);
	OCT_B2DNTOHS(pDst->backup_talker_unique_id_2, pSrc);
	OCT_B2DMEMCP(pDst->backedup_talker_entity_id, pSrc);
	OCT_B2DNTOHS(pDst->backedup_talker_unique_id, pSrc);
	OCT_B2DNTOHS(pDst->avb_interface_index, pSrc);
	OCT_B2DNTOHL(pDst->buffer_length, pSrc);

	if (bufLength < OPENAVB_DESCRIPTOR_STREAM_IO_BASE_LENGTH + (pDescriptor->number_of_formats * OPENAVB_DESCRIPTOR_STREAM_IO_FORMAT_LENGTH)) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	int i1;
	for (i1 = 0; i1 < pDst->number_of_formats; i1++) {
		x_openavbAemStreamFormatFromBuf(&pDst->stream_formats[i1], pSrc);
		pSrc += 8;
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorStreamIOUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_stream_io_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_stream_io_t *openavbAemDescriptorStreamInputNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_stream_io_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(*pDescriptor));

	pDescriptor->descriptorPvtPtr = malloc(sizeof(*pDescriptor->descriptorPvtPtr));
	if (!pDescriptor->descriptorPvtPtr) {
		free(pDescriptor);
		pDescriptor = NULL;
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_stream_io_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorStreamIOToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorStreamIOFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorStreamIOUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_STREAM_INPUT;
	pDescriptor->formats_offset = OPENAVB_DESCRIPTOR_STREAM_IO_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	// Will determine the status of this once the Listener has connected to AVDECC.
	pDescriptor->fast_connect_status = OPENAVB_FAST_CONNECT_STATUS_UNKNOWN;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT openavb_aem_descriptor_stream_io_t *openavbAemDescriptorStreamOutputNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_stream_io_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(*pDescriptor));

	pDescriptor->descriptorPvtPtr = malloc(sizeof(*pDescriptor->descriptorPvtPtr));
	if (!pDescriptor->descriptorPvtPtr) {
		free(pDescriptor);
		pDescriptor = NULL;
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_stream_io_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorStreamIOToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorStreamIOFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorStreamIOUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_STREAM_OUTPUT;
	pDescriptor->formats_offset = OPENAVB_DESCRIPTOR_STREAM_IO_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	// Talkers won't attempt to fast connect.
	pDescriptor->fast_connect_status = OPENAVB_FAST_CONNECT_STATUS_NOT_AVAILABLE;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

/*
 *      Initial and static values for stream input / output descriptors.
 *      ToDo : read actual endpoint configuration.
 */
static void fillInStreamFormat(openavb_aem_descriptor_stream_io_t *pDescriptor, const openavb_avdecc_configuration_cfg_t *pConfig)
{	// AVDECC_TODO - Specify the stream format information for MMA Stream Format.
	if (strcmp(pConfig->stream->map_fn,"openavbMapAVTPAudioInitialize") == 0)
	{
		pDescriptor->stream_formats[0].v = 0;
		pDescriptor->stream_formats[0].subtype = OPENAVB_AEM_STREAM_FORMAT_AVTP_AUDIO_SUBTYPE;
		pDescriptor->stream_formats[0].subtypes.avtp_audio.nominal_sample_rate = 0x05;
		pDescriptor->stream_formats[0].subtypes.avtp_audio.format = 2;
		pDescriptor->stream_formats[0].subtypes.avtp_audio.bit_depth = 32;
		pDescriptor->stream_formats[0].subtypes.avtp_audio.channels_per_frame = 2;
		pDescriptor->stream_formats[0].subtypes.avtp_audio.samples_per_frame = 6;
	}
	else if (strcmp(pConfig->stream->map_fn,"openavbMapUncmpAudioInitialize") == 0)
	{
		pDescriptor->stream_formats[0].v = 0;
		pDescriptor->stream_formats[0].subtype = OPENAVB_AEM_STREAM_FORMAT_61883_IIDC_SUBTYPE;
		pDescriptor->stream_formats[0].subtypes.iec_61883_iidc.sf = 1;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.fmt = OPENAVB_AEM_STREAM_FORMAT_FMT_61883_6;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.fdf_evt = OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_AM824;
		/*
		 *      static sampling rate. Requires fixing. 0x02 = 48kHz, 0x04 = 96kHz
		 */
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.fdf_sfc = 0x02;
		/*
		 *      static channel count. Requires fixing. 0x08 = 8 channels
		 */
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.dbs = 0x08;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.b = 0x00;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.nb = 0x01;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.label_iec_60958_cnt = 0;
		/*
		 *      static channel count. Requires fixing. 0x08 = 8 channels
		 */
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.label_mbla_cnt = pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.dbs;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.label_midi_cnt = 0;
		pDescriptor->stream_formats[0].subtypes.iec_61883_6_am824.label_smptecnt = 0;
	}
	else if (strcmp(pConfig->stream->map_fn,"openavbMapCtrlInitialize") == 0)
	{
		pDescriptor->stream_formats[0].v = 0;
		pDescriptor->stream_formats[0].subtype = OPENAVB_AEM_STREAM_FORMAT_AVTP_CONTROL_SUBTYPE;
		pDescriptor->stream_formats[0].subtypes.avtp_control.protocol_type = 3;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[0] = 0x90;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[1] = 0xe0;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[2] = 0xf0;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[3] = 0x00;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[4] = 0x00;
		pDescriptor->stream_formats[0].subtypes.avtp_control.format_id[5] = 0x00;
	}
	else if (strcmp(pConfig->stream->map_fn,"openavbMapMpeg2tsInitialize") == 0)
	{
		pDescriptor->stream_formats[0].v = 0;
		pDescriptor->stream_formats[0].subtype = OPENAVB_AEM_STREAM_FORMAT_61883_IIDC_SUBTYPE;
		pDescriptor->stream_formats[0].subtypes.iec_61883_4.sf = 1;
		pDescriptor->stream_formats[0].subtypes.iec_61883_4.fmt = OPENAVB_AEM_STREAM_FORMAT_FMT_61883_4;
	}
	else if (strcmp(pConfig->stream->map_fn,"openavbMapMjpegInitialize") == 0 || strcmp(pConfig->stream->map_fn,"openavbMapH264Initialize") == 0)
	{
		pDescriptor->stream_formats[0].v = 0;
		pDescriptor->stream_formats[0].subtype = OPENAVB_AEM_STREAM_FORMAT_AVTP_VIDEO_SUBTYPE;
		if (strcmp(pConfig->stream->map_fn,"openavbMapMjpegInitialize") == 0)
		{
			pDescriptor->stream_formats[0].subtypes.avtp_video.format = OPENAVB_AEM_RTP_PAYLOAD_SUBTYPE_RTP_MJPEG;
		}
		else
		{
			pDescriptor->stream_formats[0].subtypes.avtp_video.format = OPENAVB_AEM_RTP_PAYLOAD_SUBTYPE_RTP_H264;
		}
	}
	pDescriptor->number_of_formats = 1;

	// Make the current format the first format in the list.
	memcpy(&(pDescriptor->current_format), &(pDescriptor->stream_formats[0]), sizeof(openavb_aem_stream_format_t));
}

extern DLL_EXPORT bool openavbAemDescriptorStreamInputInitialize(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	(void) nConfigIdx;
	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// Specify a name.
	sprintf((char *)(pDescriptor->object_name), "Stream Input %u", pDescriptor->descriptor_index);

	// Specify the stream flags.
	// The stream configuration is ignored for Listeners, as Listeners will accept both Class A and Class B.
	pDescriptor->stream_flags |= ( OPENAVB_AEM_STREAM_FLAG_CLASS_A | OPENAVB_AEM_STREAM_FLAG_CLASS_B );

	// Specify the stream format information.
	fillInStreamFormat(pDescriptor, pConfig);

	// Save the stream configuration pointer.
	pDescriptor->stream = pConfig->stream;

	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorStreamOutputInitialize(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	(void) nConfigIdx;
	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// Specify a name.
	sprintf((char *)(pDescriptor->object_name), "Stream Output %u", pDescriptor->descriptor_index);

	// Specify the stream flags.
	if (pConfig->stream->sr_class == SR_CLASS_A) { pDescriptor->stream_flags |= OPENAVB_AEM_STREAM_FLAG_CLASS_A; }
	if (pConfig->stream->sr_class == SR_CLASS_B) { pDescriptor->stream_flags |= OPENAVB_AEM_STREAM_FLAG_CLASS_B; }

	// Specify the stream format information.
	fillInStreamFormat(pDescriptor, pConfig);

	// Save the stream configuration pointer.
	pDescriptor->stream = pConfig->stream;

	return TRUE;
}


////////////////////////////////
// Public functions for internal
// AVDECC use (not exported)
////////////////////////////////

void openavbAemStreamFormatToBuf(openavb_aem_stream_format_t *pSrc, U8 *pBuf)
{
	U8 *pDst = pBuf;

	// Tightly bound to caller not safety checks needed.

	memset(pBuf, 0, 8);

	if (pSrc->v == 0) {
		BIT_D2BHTONB(pDst, pSrc->v, 7, 0);
		BIT_D2BHTONB(pDst, pSrc->subtype, 0, 1);
		switch (pSrc->subtype) {
			case OPENAVB_AEM_STREAM_FORMAT_61883_IIDC_SUBTYPE:
				{
					if (pSrc->subtypes.iec_61883_iidc.sf == OPENAVB_AEM_STREAM_FORMAT_SF_IIDC) {
						BIT_D2BHTONB(pDst, pSrc->subtypes.iidc.sf, 7, 1);
						pDst += 3;
						OCT_D2BHTONB(pDst, pSrc->subtypes.iidc.iidc_format);
						OCT_D2BHTONB(pDst, pSrc->subtypes.iidc.iidc_mode);
						OCT_D2BHTONB(pDst, pSrc->subtypes.iidc.iidc_rate);
					}
					else {
						if (pSrc->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_4) {
							BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_4.sf, 7, 0);
							BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_4.fmt, 1, 1);
						}
						else if (pSrc->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_6) {
							if (pSrc->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_32BITS) {
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.sf, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.fmt, 1, 1);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.fdf_evt, 3, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.fdf_sfc, 0, 1);
								OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.dbs);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.b, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_32bit.nb, 6, 1);
							}
							else if (pSrc->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_FLOAT) {
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.sf, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.fmt, 1, 1);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.fdf_evt, 3, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.fdf_sfc, 0, 1);
								OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.dbs);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.b, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_float.nb, 6, 1);
							}
							else if (pSrc->subtypes.iec_61883_6.fdf_evt == OPENAVB_AEM_STREAM_FORMAT_FDF_EVT_61883_6_AM824) {
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.sf, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.fmt, 1, 1);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.fdf_evt, 3, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.fdf_sfc, 0, 1);
								OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.dbs);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.b, 7, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.nb, 6, 1);
								OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.label_iec_60958_cnt);
								OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.label_mbla_cnt);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.label_midi_cnt, 4, 0);
								BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_6_am824.label_smptecnt, 0, 1);
							}
						}
						else if (pSrc->subtypes.iec_61883.fmt == OPENAVB_AEM_STREAM_FORMAT_FMT_61883_8) {
							BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_8.sf, 7, 0);
							BIT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_8.fmt, 1, 1);
							pDst += 3;
							OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_8.video_mode);
							OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_8.compress_mode);
							OCT_D2BHTONB(pDst, pSrc->subtypes.iec_61883_8.color_space);
						}
					}
				}

				break;
			case OPENAVB_AEM_STREAM_FORMAT_MMA_SUBTYPE:
				// Nothing to do
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_AUDIO_SUBTYPE:
				BIT_D2BHTONB(pDst, pSrc->subtypes.avtp_audio.nominal_sample_rate, 0, 1);
				OCT_D2BHTONB(pDst, pSrc->subtypes.avtp_audio.format);
				OCT_D2BHTONB(pDst, pSrc->subtypes.avtp_audio.bit_depth);
				BIT_D2BHTONL(pDst, pSrc->subtypes.avtp_audio.channels_per_frame, 22, 0);
				BIT_D2BHTONL(pDst, pSrc->subtypes.avtp_audio.samples_per_frame, 12, 4);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_VIDEO_SUBTYPE:
				OCT_D2BHTONB(pDst, pSrc->subtypes.avtp_video.format);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_AVTP_CONTROL_SUBTYPE:
				BIT_D2BHTONB(pDst, pSrc->subtypes.avtp_control.protocol_type, 4, 1);
				OCT_D2BMEMCP(pDst, pSrc->subtypes.avtp_control.format_id);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_VENDOR_SUBTYPE:
				pDst++;
				OCT_D2BMEMCP(pDst, pSrc->subtypes.vendor.format_id);
				break;
			case OPENAVB_AEM_STREAM_FORMAT_EXPERIMENTAL_SUBTYPE:
				// Nothing to do
				break;
		}
	}
	else {
		BIT_D2BHTONB(pDst, pSrc->v, 7, 1);
		pDst++;
		OCT_D2BMEMCP(pDst, pSrc->subtypes.vendor_specific.format_id);
	}
}
