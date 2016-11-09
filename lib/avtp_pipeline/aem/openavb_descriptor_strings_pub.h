/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Strings Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Strings Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_STRINGS_PUB_H
#define OPENAVB_DESCRIPTOR_STRINGS_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// STRINGS Descriptor IEEE Std 1722.1-2013 clause 7.2.12
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;									// Set internally
	U16 descriptor_index;									// Set internally
	U8 string_0[OPENAVB_AEM_STRLEN_MAX];
	U8 string_1[OPENAVB_AEM_STRLEN_MAX];
	U8 string_2[OPENAVB_AEM_STRLEN_MAX];
	U8 string_3[OPENAVB_AEM_STRLEN_MAX];
	U8 string_4[OPENAVB_AEM_STRLEN_MAX];
	U8 string_5[OPENAVB_AEM_STRLEN_MAX];
	U8 string_6[OPENAVB_AEM_STRLEN_MAX];
} openavb_aem_descriptor_strings_t;

openavb_aem_descriptor_strings_t *openavbAemDescriptorStringsNew(void);

#endif // OPENAVB_DESCRIPTOR_STRINGS_PUB_H
