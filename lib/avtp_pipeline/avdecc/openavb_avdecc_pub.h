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

// Discovery protocol public includes
#include "openavb_adp_pub.h"

// Connection management public includes
#include "openavb_acmp_pub.h"

// Enumeration and control public includes
#include "openavb_aecp_pub.h"

typedef struct {
	bool bListener;
	bool bTalker;
	char ifname[32];
	openavb_aem_descriptor_entity_t *pDescriptorEntity;
} openavb_avdecc_cfg_t;

// General initialization for AVDECC. This must be called before any other AVDECC APIs including AEM APIs
bool openavbAVDECCInitialize(openavb_avdecc_cfg_t *pAvdeccCfg);

// Start the AVDECC protocols. 
bool openavbAVDECCStart(void);

// Stop the AVDECC protocols. 
void openavbAVDECCStop(void);

// General Cleanup for AVDECC.
bool openavbAVDECCCleanup(void);

#endif // OPENAVB_AVDECC_PUB_H
