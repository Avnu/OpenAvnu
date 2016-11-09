/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Locale Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Locale Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_LOCALE_PUB_H
#define OPENAVB_DESCRIPTOR_LOCALE_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// LOCALE Descriptor IEEE Std 1722.1-2013 clause 7.2.11
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;									// Set internally
	U16 descriptor_index;									// Set internally
	U8 locale_identifier[OPENAVB_AEM_STRLEN_MAX];
	U16 number_of_strings;
	U16 base_strings;
} openavb_aem_descriptor_locale_t;

openavb_aem_descriptor_locale_t *openavbAemDescriptorLocaleNew(void);

#endif // OPENAVB_DESCRIPTOR_LOCALE_PUB_H
