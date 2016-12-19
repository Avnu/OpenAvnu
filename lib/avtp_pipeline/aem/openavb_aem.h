/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Entity Model
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) Entity Model
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_AEM_H
#define OPENAVB_AEM_H 1

#include "openavb_array.h"
#include "openavb_avdecc.h"
#include "openavb_aem_pub.h"
#include "openavb_types_pub.h"
#include "openavb_result_codes.h"

typedef openavbRC (*openavb_aem_descriptor_to_buf_t)(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize);
typedef openavbRC (*openavb_aem_descriptor_from_buf_t)(void *pVoidDescriptor, U16 bufLength, U8 *pBuf);
typedef openavbRC (*openavb_aem_descriptor_update_t)(void *pVoidDescriptor);

struct openavb_descriptor_pvt {
	U32 size;
	bool bTopLevel; // See IEEE Std 1722.1-2013 clause 7.2.2 for a list of top level descriptors.
	openavb_aem_descriptor_to_buf_t toBuf;
	openavb_aem_descriptor_from_buf_t fromBuf;
	openavb_aem_descriptor_update_t update;
};

// Every descriptor must begin with these same members. The structure isn't embedded to make hosting applications cleaner.
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;
	U16 descriptor_type;
	U16 descriptor_index;
} openavb_aem_descriptor_common_t;

typedef struct {
	openavb_aem_descriptor_configuration_t *pDescriptorConfiguration;
	openavb_array_t descriptorsArray[OPENAVB_AEM_DESCRIPTOR_COUNT];
} openavb_aem_configuration_t;

typedef struct {
	openavb_aem_descriptor_entity_t *pDescriptorEntity;
	bool entityAcquired;
	U8 acquiredControllerId[8];
	bool entityLocked;
	U8 lockedControllerId[8];

	openavb_array_t aemConfigurations;
	openavb_array_t aemNonTopLevelDescriptorsArray[OPENAVB_AEM_DESCRIPTOR_COUNT];
} openavb_avdecc_entity_model_t;


// Create the Entity Model
openavbRC openavbAemCreate(openavb_aem_descriptor_entity_t *pDescriptorEntity);

// Destory the Entity Model
openavbRC openavbAemDestroy(void);

// Return the Entity Model
openavb_avdecc_entity_model_t *openavbAemGetModel(void);

// Checks if the Entity Model is valid
bool openavbAemCheckModel(bool bLog);

// Return the array of descriptors for a specific descroptor type for the specified configuration.
openavb_array_t openavbAemGetDescriptorArray(U16 configIdx, U16 descriptorType);

// Serialize a descriptor into a buffer. pBuf is filled with the descriptor data. descriptorSize is set to the size of the data placed into the buffer.
openavbRC openavbAemSerializeDescriptor(U16 configIdx, U16 descriptorType, U16 descriptorIdx, U16 bufSize, U8 *pBuf, U16 *descriptorSize);

#endif // OPENAVB_AEM_H
