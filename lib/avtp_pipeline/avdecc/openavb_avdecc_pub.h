/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC - Top level AVDECC (1722.1) Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       4-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Top level AVDECC (1722.1) Public Interface
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
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

	char ifname[IFNAMSIZ];
	U8 ifmac[ETH_ALEN];

	bool bListener;
	bool bTalker;
	bool bClassASupported;
	bool bClassBSupported;

	U16 vlanID;
	U8 vlanPCP;

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
