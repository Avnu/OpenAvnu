/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
 ******************************************************************
 * MODULE : AEM - AVDECC Entity Model
 * MODULE SUMMARY : Implements the 1722.1 (AVDECC) Entity Model
 ******************************************************************
 */

#ifndef OPENAVB_AEM_H
#define OPENAVB_AEM_H 1

#include "openavb_array.h"
#include "openavb_avdecc.h"
#include "openavb_aem_pub.h"
#include "openavb_types_pub.h"
#include "openavb_result_codes.h"
#include "openavb_endpoint.h"

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
