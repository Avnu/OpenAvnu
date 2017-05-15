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
 * MODULE : AEM - AVDECC AVB Interface Descriptor Public Interface
 * MODULE SUMMARY : Public Interface for the AVB Interface Descriptor
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H
#define OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// AVB INTERFACE Descriptor IEEE Std 1722.1-2013 clause 7.2.8
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];							// Set internally
	openavb_aem_string_ref_t localized_description;
	U8 mac_address[6];												// Set internally
	U16 interface_flags;
	U8 clock_identity[8];
	U8 priority1;
	U8 clock_class;
	S16 offset_scaled_log_variance;
	U8 clock_accuracy;
	U8 priority2;
	U8 domain_number;
	S8 log_sync_interval;
	S8 log_announce_interval;
	S8 log_pdelay_interval;
	U16 port_number;
} openavb_aem_descriptor_avb_interface_t;

openavb_aem_descriptor_avb_interface_t *openavbAemDescriptorAvbInterfaceNew(void);

bool openavbAemDescriptorAvbInterfaceInitialize(openavb_aem_descriptor_avb_interface_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig);

#endif // OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H
