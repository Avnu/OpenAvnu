/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Clock Domain Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       26-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Clock Domain Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_PUB_H
#define OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

#define OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_MAX_CLOCK_SOURCES (249)

// CLOCK DOMAIN Descriptor IEEE Std 1722.1-2013 clause 7.2.32
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 clock_source_index;
	U16 clock_sources_offset;
	U16 clock_sources_count;
	U16 clock_sources[OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_MAX_CLOCK_SOURCES];
} openavb_aem_descriptor_clock_domain_t;

openavb_aem_descriptor_clock_domain_t *openavbAemDescriptorClockDomainNew(void);

#endif // OPENAVB_DESCRIPTOR_CLOCK_DOMAIN_PUB_H
