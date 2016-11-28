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

#define OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS (7)

// STRINGS Descriptor IEEE Std 1722.1-2013 clause 7.2.12
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;									// Set internally
	U16 descriptor_index;									// Set internally
	U8 string[OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS][OPENAVB_AEM_STRLEN_MAX];
} openavb_aem_descriptor_strings_t;

openavb_aem_descriptor_strings_t *openavbAemDescriptorStringsNew(void);

// Returns the descriptor string at the specified index, or NULL if not specified.
const char * openavbAemDescriptorStringsGet_string(openavb_aem_descriptor_strings_t *pDescriptor, U8 index);

// Set a descriptor string at the specified index.
bool openavbAemDescriptorStringsSet_string(openavb_aem_descriptor_strings_t *pDescriptor, const char *pString, U8 index);

// Returns the number of strings descriptors in this locale.
U16 openavbAemDescriptorStringsGet_number_of_strings(openavb_aem_descriptor_strings_t *pDescriptor);

// Returns the desriptor index of the first strings descriptor for this locale.
U16 openavbAemDescriptorStringsGet_base_strings(openavb_aem_descriptor_strings_t *pDescriptor);

#endif // OPENAVB_DESCRIPTOR_STRINGS_PUB_H
