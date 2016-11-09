/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Jack IO Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       20-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Jack IO Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_JACK_IO_PUB_H
#define OPENAVB_DESCRIPTOR_JACK_IO_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// JACK IO Descriptor IEEE Std 1722.1-2013 clause 7.2.7
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;
	U16 descriptor_index;
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 jack_flags;
	U16 jack_type;
	U16 number_of_controls;
	U16 base_control;
} openavb_aem_descriptor_jack_io_t;

openavb_aem_descriptor_jack_io_t *openavbAemDescriptorJackInputNew(void);
openavb_aem_descriptor_jack_io_t *openavbAemDescriptorJackOutputNew(void);

#endif // OPENAVB_DESCRIPTOR_JACK_IO_PUB_H
