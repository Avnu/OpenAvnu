/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Audio Map Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       26-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Audio Map Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_AUDIO_MAP_PUB_H
#define OPENAVB_DESCRIPTOR_AUDIO_MAP_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

#define OPENAVB_DESCRIPTOR_AUDIO_MAP_MAX_SAMPLING_RATES (62)

// AUDIO UNIT Descriptor : Audio Mappings Format IEEE Std 1722.1-2013 clause 7.2.19.1
typedef struct {
	U16 mapping_stream_index;
	U16 mapping_stream_channel;
	U16 mapping_cluster_offset;
	U16 mapping_cluster_channel;
} openavb_aem_descriptor_audio_map_audio_mapping_format_t;

// AUDIO MAP Descriptor IEEE Std 1722.1-2013 clause 7.2.19
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U16 mappings_offset;
	U16 number_of_mappings;
	openavb_aem_descriptor_audio_map_audio_mapping_format_t mapping_formats[OPENAVB_DESCRIPTOR_AUDIO_MAP_MAX_SAMPLING_RATES];
} openavb_aem_descriptor_audio_map_t;

openavb_aem_descriptor_audio_map_t *openavbAemDescriptorAudioMapNew();

#endif // OPENAVB_DESCRIPTOR_AUDIO_MAP_PUB_H
