/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Entity Model : Entity Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       20-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Entity Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
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
