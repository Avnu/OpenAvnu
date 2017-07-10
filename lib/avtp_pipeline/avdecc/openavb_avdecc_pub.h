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
 * MODULE : AVDECC - Top level AVDECC (1722.1) Public Interface
 * MODULE SUMMARY : Top level AVDECC (1722.1) Public Interface
 ******************************************************************
 */

#ifndef OPENAVB_AVDECC_PUB_H
#define OPENAVB_AVDECC_PUB_H 1

// Entity Model public includes
#include "openavb_aem_pub.h"
#include "openavb_descriptor_entity_pub.h"
#include "openavb_descriptor_configuration_pub.h"
#include "openavb_descriptor_audio_unit_pub.h"
#include "openavb_descriptor_stream_io_pub.h"
#include "openavb_descriptor_jack_io_pub.h"
#include "openavb_descriptor_avb_interface_pub.h"
#include "openavb_descriptor_clock_source_pub.h"
#include "openavb_descriptor_locale_pub.h"
#include "openavb_descriptor_strings_pub.h"
#include "openavb_descriptor_control_pub.h"
#include "openavb_descriptor_stream_port_io_pub.h"
#include "openavb_descriptor_external_port_io_pub.h"
#include "openavb_descriptor_audio_cluster_pub.h"
#include "openavb_descriptor_audio_map_pub.h"
#include "openavb_descriptor_clock_domain_pub.h"
#include "openavb_descriptor_locale_strings_handler_pub.h"

// Discovery protocol public includes
#include "openavb_adp_pub.h"

// Connection management public includes
#include "openavb_acmp_pub.h"

// Enumeration and control public includes
#include "openavb_aecp_pub.h"

// openavb_tl_data_cfg_t definition
#include "openavb_avdecc_read_ini_pub.h"

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

// Indexes for localized strings.
// (These are defined for internal use, and can be changed as needed.)
#define LOCALE_STRING_VENDOR_NAME_INDEX 0
#define LOCALE_STRING_MODEL_NAME_INDEX  1

typedef struct {

	char ifname[IFNAMSIZ + 10]; // Include space for the socket type prefix (e.g. "simple:eth0")
	U8 ifmac[ETH_ALEN];

	bool bListener;
	bool bTalker;
	bool bClassASupported;
	bool bClassBSupported;

	U16 vlanID;
	U8 vlanPCP;

	bool bFastConnectSupported; // FAST_CONNECT and SAVED_STATE supported

	U8 valid_time; // Number of 2-second units

	// Information to add to the descriptor.
	unsigned avdeccId;
	U8 entity_model_id[8];
	char entity_name[OPENAVB_AEM_STRLEN_MAX];
	char firmware_version[OPENAVB_AEM_STRLEN_MAX];
	char group_name[OPENAVB_AEM_STRLEN_MAX];
	char serial_number[OPENAVB_AEM_STRLEN_MAX];

	// Localization strings.
	char locale_identifier[OPENAVB_AEM_STRLEN_MAX];
	char vendor_name[OPENAVB_AEM_STRLEN_MAX];
	char model_name[OPENAVB_AEM_STRLEN_MAX];

	openavb_aem_descriptor_entity_t *pDescriptorEntity;
	openavb_aem_descriptor_locale_strings_handler_t *pAemDescriptorLocaleStringsHandler;
} openavb_avdecc_cfg_t;

typedef struct openavb_avdecc_configuration_cfg {
	struct openavb_avdecc_configuration_cfg *next; // next link list pointer

	// Pointer to the endpoint information.
	openavb_tl_data_cfg_t *stream;

	// Friendly name
	char friendly_name[OPENAVB_AEM_STRLEN_MAX];

	// AVDECC_TODO:  Add additional information as needed.

} openavb_avdecc_configuration_cfg_t;


// General initialization for AVDECC. This must be called before any other AVDECC APIs including AEM APIs
bool openavbAvdeccInitialize(void);

// Start the AVDECC protocols. 
bool openavbAvdeccStart(void);

// Stop the AVDECC protocols. 
void openavbAvdeccStop(void);

// General Cleanup for AVDECC.
bool openavbAvdeccCleanup(void);

#endif // OPENAVB_AVDECC_PUB_H
