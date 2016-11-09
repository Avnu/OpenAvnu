/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Clock Source Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       20-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Clock Source Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_CLOCK_SOURCE_PUB_H
#define OPENAVB_DESCRIPTOR_CLOCK_SOURCE_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// CLOCK SOURCE Descriptor IEEE Std 1722.1-2013 clause 7.2.9
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 clock_source_flags;
	U16 clock_source_type;
	U8 clock_source_identifier[8];
	U16 clock_source_location_type;
	U16 clock_source_location_index;
} openavb_aem_descriptor_clock_source_t;

openavb_aem_descriptor_clock_source_t *openavbAemDescriptorClockSourceNew(void);

#endif // OPENAVB_DESCRIPTOR_CLOCK_SOURCE_PUB_H
