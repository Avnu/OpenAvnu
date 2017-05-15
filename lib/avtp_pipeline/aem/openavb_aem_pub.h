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
 * MODULE : AEM - AVDECC Entity Model Public Interface
 * MODULE SUMMARY : Public Interface for the AVDECC Entity Model Functionality
 ******************************************************************
 */

#ifndef OPENAVB_AEM_PUB_H
#define OPENAVB_AEM_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"

typedef struct openavb_descriptor_pvt *openavb_descriptor_pvt_ptr_t;

typedef struct openavb_avdecc_configuration_cfg openavb_avdecc_configuration_cfg_t;

// Before calling any Entity Model functions the AVDECC function openavbAvdeccInitialize() must be called with the Entity Descriptor.
// The Entity Descriptor is the root of all descriptors and must exist to do most things in AVDECC.

// Gets the current configuration descriptor index for the Entity Model. Error checking is not performed on this function.
U16 openavbAemGetConfigIdx(void);

// Adds descriptors to the Entity Model. The Configuration Descriptor must be added first.
bool openavbAemAddDescriptor(void *pDescriptor, U16 configIdx, U16 *pResultIdx);

// Get a pointer to a descriptor in the Entity Model.
void *openavbAemGetDescriptor(U16 configIdx, U16 descriptorType, U16 descriptorIdx);

// Get the index of the descriptor in the Entity Model, or OPENAVB_AEM_DESCRIPTOR_INVALID if not found.
U16 openavbAemGetDescriptorIndex(U16 configIdx, const void *pDescriptor);

// Add a string to a standard descriptor U8 [64] string field.
bool openavbAemSetString(U8 *pMem, const char *pString);

// Checks if the Entity Model is acquired.
bool openavbAemIsAcquired(void);

// Checks if the Entity Model is locked.
bool openavbAemIsLocked(void);

// Sets the internal Entity Model acquired flag. Care should be used with this function it can impact controller access.
bool openavbAemAcquire(bool flag);

// Sets the internal Entity Model acquired flag. Care should be used with this function it can impact controller access.
bool openavbAemLock(bool flag);


#endif // OPENAVB_AEM_PUB_H
