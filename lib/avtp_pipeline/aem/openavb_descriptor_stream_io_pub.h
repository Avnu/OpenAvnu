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
 * MODULE : AEM - AVDECC Stream IO Descriptor Public Interface
 * MODULE SUMMARY : Public Interface for the Stream IO Descriptor
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_STREAM_IO_PUB_H
#define OPENAVB_DESCRIPTOR_STREAM_IO_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"
#include "openavb_tl_pub.h"
#include "openavb_avdecc_read_ini_pub.h"

#define OPENAVB_DESCRIPTOR_STREAM_IO_MAX_FORMATS (47)

// Possible statuses of the fast connect support for the Listener
typedef enum {
	OPENAVB_FAST_CONNECT_STATUS_UNKNOWN = 0, // Still need to determine if something is available
	OPENAVB_FAST_CONNECT_STATUS_NOT_AVAILABLE, // No fast connects available for the Listener, or is a Talker
	OPENAVB_FAST_CONNECT_STATUS_IN_PROGRESS, // Attempting fast connect
	OPENAVB_FAST_CONNECT_STATUS_TIMED_OUT, // Talker did not respond to fast connect attempts
	OPENAVB_FAST_CONNECT_STATUS_SUCCEEDED, // Fast connect was successful; don't need to try again
} openavb_fast_connect_status_t;

// STREAM IO Descriptor IEEE Std 1722.1-2013 clause 7.2.6
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;
	U16 descriptor_index;
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 clock_domain_index;
	U16 stream_flags;
	openavb_aem_stream_format_t current_format;
	U16 formats_offset;
	U16 number_of_formats;
	U8 backup_talker_entity_id_0[8];
	U16 backup_talker_unique_id_0;
	U8 backup_talker_entity_id_1[8];
	U16 backup_talker_unique_id_1;
	U8 backup_talker_entity_id_2[8];
	U16 backup_talker_unique_id_2;
	U8 backedup_talker_entity_id[8];
	U16 backedup_talker_unique_id;
	U16 avb_interface_index;
	U32 buffer_length;
	openavb_aem_stream_format_t stream_formats[OPENAVB_DESCRIPTOR_STREAM_IO_MAX_FORMATS];

	// Current status of the fast connect support for the Listener.
	openavb_fast_connect_status_t fast_connect_status;
	U8 fast_connect_talker_entity_id[8];
	struct timespec fast_connect_start_time;

	// OPENAVB_ACMP_FLAG values from CONNECT_TX_RESPONSE or CONNECT_RX_RESPONSE.
	U16 acmp_flags;

	// Streaming values for GET_STREAM_INFO from CONNECT_TX_RESPONSE or CONNECT_RX_RESPONSE.
	// These values are currently only used for the Listener, as the Talker can get them from it's current configuration.
	U8 acmp_stream_id[8];
	U8 acmp_dest_addr[6];
	U16 acmp_stream_vlan_id;

	// Also save a pointer to the supplied stream information.
	const openavb_tl_data_cfg_t *stream;

} openavb_aem_descriptor_stream_io_t;

openavb_aem_descriptor_stream_io_t *openavbAemDescriptorStreamInputNew(void);
openavb_aem_descriptor_stream_io_t *openavbAemDescriptorStreamOutputNew(void);

bool openavbAemDescriptorStreamInputInitialize(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig);
bool openavbAemDescriptorStreamOutputInitialize(openavb_aem_descriptor_stream_io_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig);

void openavbAemStreamFormatToBuf(openavb_aem_stream_format_t *pSrc, U8 *pBuf);

#endif // OPENAVB_DESCRIPTOR_STREAM_IO_PUB_H
