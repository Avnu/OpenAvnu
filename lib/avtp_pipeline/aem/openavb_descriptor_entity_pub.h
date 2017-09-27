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
 * MODULE : AEM - AVDECC Entity Model : Entity Descriptor Public Interface
 * MODULE SUMMARY : Public Interface for the Entity Descriptor
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_ENTITY_PUB_H
#define OPENAVB_DESCRIPTOR_ENTITY_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// ENTITY Descriptor IEEE Std 1722.1-2013 clause 7.2.1
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;									// Set internally
	U16 descriptor_index;									// Set internally
	U8 entity_id[8];
	U8 entity_model_id[8];
	U32 entity_capabilities;
	U16 talker_stream_sources;
	U16 talker_capabilities;
	U16 listener_stream_sinks;
	U16 listener_capabilities;
	U32 controller_capabilities;
	U32 available_index;
	U8 association_id[8];
	U8 entity_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t vendor_name_string;
	openavb_aem_string_ref_t model_name_string;
	U8 firmware_version[OPENAVB_AEM_STRLEN_MAX];
	U8 group_name[OPENAVB_AEM_STRLEN_MAX];
	U8 serial_number[OPENAVB_AEM_STRLEN_MAX];
	U16 configurations_count;								// Set internally
	U16 current_configuration;								// Set internally
} openavb_aem_descriptor_entity_t;

// Create a new Entity Descriptor
openavb_aem_descriptor_entity_t *openavbAemDescriptorEntityNew(void);

// Set the Entity ID using the common approach of mac address and an ID. Either pass in aMacAddr (ascii) or nMacAddr (network)
bool openavbAemDescriptorEntitySet_entity_id(openavb_aem_descriptor_entity_t *pDescriptor, char *aMacAddr, U8 *nMacAddr, U16 id);

// Set the Entity Model ID.  Pass in a pointer to an array of 8 bytes of arbitrary data.
bool openavbAemDescriptorEntitySet_entity_model_id(openavb_aem_descriptor_entity_t *pDescriptor, U8 pData[8]);

// Set the Entity Capabilities.  Use the OPENAVB_ADP_ENTITY_CAPABILITIES_... flags from adp/openavb_adp_pub.h.
bool openavbAemDescriptorEntitySet_entity_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U32 capabilities);

// Set the Talker Capabilities.  Use the OPENAVB_ADP_TALKER_CAPABILITIES_... flags from adp/openavb_adp_pub.h.
bool openavbAemDescriptorEntitySet_talker_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U16 num_sources, U16 capabilities);

// Set the Listener Capabilities.  Use the OPENAVB_ADP_LISTENER_CAPABILITIES_... flags from adp/openavb_adp_pub.h.
bool openavbAemDescriptorEntitySet_listener_capabilities(openavb_aem_descriptor_entity_t *pDescriptor, U16 num_sinks, U16 capabilities);

// Set the Entity Name.
bool openavbAemDescriptorEntitySet_entity_name(openavb_aem_descriptor_entity_t *pDescriptor, char *aName);

// Set the Vendor Name.
bool openavbAemDescriptorEntitySet_vendor_name(openavb_aem_descriptor_entity_t *pDescriptor, U16 nOffset, U8 nIndex);

// Set the Model Name.
bool openavbAemDescriptorEntitySet_model_name(openavb_aem_descriptor_entity_t *pDescriptor, U16 nOffset, U8 nIndex);

// Set the Firmware Version.
bool openavbAemDescriptorEntitySet_firmware_version(openavb_aem_descriptor_entity_t *pDescriptor, char *aFirmwareVersion);

// Set the Group Name.
bool openavbAemDescriptorEntitySet_group_name(openavb_aem_descriptor_entity_t *pDescriptor, char *aGroupName);

// Set the Serial Number.
bool openavbAemDescriptorEntitySet_serial_number(openavb_aem_descriptor_entity_t *pDescriptor, char *aSerialNumber);

#endif // OPENAVB_DESCRIPTOR_ENTITY_PUB_H
