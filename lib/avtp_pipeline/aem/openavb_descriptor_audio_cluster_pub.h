/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Audio Cluster Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       20-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Audio Cluster Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_AUDIO_CLUSTER_PUB_H
#define OPENAVB_DESCRIPTOR_AUDIO_CLUSTER_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// AUDIO CLUSTER Descriptor IEEE Std 1722.1-2013 clause 7.2.16
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U16 signal_type;
	U16 signal_index;
	U16 signal_output;
	U32 path_latency;
	U32 block_latency;
	U16 channel_count;
	U8 format;
} openavb_aem_descriptor_audio_cluster_t;

openavb_aem_descriptor_audio_cluster_t *openavbAemDescriptorAudioClusterNew(void);

#endif // OPENAVB_DESCRIPTOR_AUDIO_CLUSTER_PUB_H
