/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Entity Model : Configuration Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       2-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Configuration Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_CONFIGURATION_PUB_H
#define OPENAVB_DESCRIPTOR_CONFIGURATION_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

#define OPENAVB_DESCRIPTOR_CONFIGURATION_MAX_DESCRIPTOR_COUNTS (108)

// CONFIGURATION Descriptor IEEE Std 1722.1-2013 clause 7.2.2
typedef struct {
	U16 descriptor_type;
	U16 count;
} openavb_aem_descriptor_configuration_descriptor_counts_t;

typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;														// Set internally
	U16 descriptor_index;														// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 descriptor_counts_count;												// Set internally
	U16 descriptor_counts_offset;												// Set internally
	openavb_aem_descriptor_configuration_descriptor_counts_t descriptor_counts[OPENAVB_DESCRIPTOR_CONFIGURATION_MAX_DESCRIPTOR_COUNTS];	// Set internally
} openavb_aem_descriptor_configuration_t;

openavb_aem_descriptor_configuration_t *openavbAemDescriptorConfigurationNew(void);

bool openavbAemDescriptorConfigurationInitialize(openavb_aem_descriptor_configuration_t *pDescriptor, U16 nConfigIdx, const clientStream_t *stream);

#endif // OPENAVB_DESCRIPTOR_CONFIGURATION_PUB_H
