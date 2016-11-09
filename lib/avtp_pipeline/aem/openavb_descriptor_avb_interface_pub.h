/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC AVB Interface Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       20-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the AVB Interface Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H
#define OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// AVB INTERFACE Descriptor IEEE Std 1722.1-2013 clause 7.2.8
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U8 mac_address[6];												// Set internally
	U16 interface_flags;
	U8 clock_identity[8];
	U8 priority1;
	U8 clock_class;
	U16 offset_scaled_log_variance;
	U8 clock_accuracy;
	U8 priority2;
	U8 domain_number;
	U8 log_sync_interval;
	U8 log_announce_interval;
	U8 log_pdelay_interval;
	U16 port_number;
} openavb_aem_descriptor_avb_interface_t;

openavb_aem_descriptor_avb_interface_t *openavbAemDescriptorAvbInterfaceNew(void);

#endif // OPENAVB_DESCRIPTOR_AVB_INTERFACE_PUB_H
