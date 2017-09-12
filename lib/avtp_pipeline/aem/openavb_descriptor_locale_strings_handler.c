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
 * MODULE : AEM - AVDECC Locale Strings Handler Descriptor
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_descriptor_locale_strings_handler.h"


////////////////////////////////
// Public functions
////////////////////////////////

extern DLL_EXPORT openavb_aem_descriptor_locale_strings_handler_t *openavbAemDescriptorLocaleStringsHandlerNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(*pDescriptor));

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT void openavbAemDescriptorLocaleStringsHandlerFree(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (pDescriptor) {
		while (pDescriptor->pFirstGroup) {
			openavb_aem_descriptor_locale_strings_handler_group_t *pDel = pDescriptor->pFirstGroup;
			pDescriptor->pFirstGroup = pDel->pNext;
			free(pDel->pLocale); // TODO BDT_DEBUG Will this be freed elsewhere?
			free(pDel->pStrings); // TODO BDT_DEBUG Will this be freed elsewhere?
			free(pDel);
		}
		free(pDescriptor);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
}

extern DLL_EXPORT openavb_aem_descriptor_locale_strings_handler_group_t * openavbAemDescriptorLocaleStringsHandlerGet_group(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	const char *aLocaleIdentifier,
	bool bCanCreate)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aLocaleIdentifier) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	// Do we have a matching item?
	openavb_aem_descriptor_locale_strings_handler_group_t *pTest = pDescriptor->pFirstGroup;
	while (pTest) {
		if (strncmp(aLocaleIdentifier, (const char *) pTest->pLocale->locale_identifier, OPENAVB_AEM_STRLEN_MAX) == 0) {
			AVB_TRACE_EXIT(AVB_TRACE_AEM);
			return pTest;
		}
		pTest = pTest->pNext;
	}

	// Can we allocate a new language structure?
	if (bCanCreate) {
		// Allocate a new group.
		openavb_aem_descriptor_locale_strings_handler_group_t *pNew = malloc(sizeof(openavb_aem_descriptor_locale_strings_handler_group_t));
		if (!pNew) {
			AVB_TRACE_EXIT(AVB_TRACE_AEM);
			return NULL;
		}
		pNew->pLocale = openavbAemDescriptorLocaleNew();
		pNew->pStrings = openavbAemDescriptorStringsNew();
		if (!pNew->pLocale || !pNew->pStrings) {
			free(pNew->pLocale);
			free(pNew->pStrings);
			free(pNew);
			AVB_TRACE_EXIT(AVB_TRACE_AEM);
			return NULL;
		}

		openavbAemDescriptorLocaleSet_locale_identifier(pNew->pLocale, aLocaleIdentifier);

		// Add the new group to the start of the linked list.
		pNew->pNext = pDescriptor->pFirstGroup;
		pDescriptor->pFirstGroup = pNew;

		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return pNew;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return NULL;
}

extern DLL_EXPORT const char * openavbAemDescriptorLocaleStringsHandlerGet_local_string(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	const char *aLocaleIdentifier,
	U8 uStringIndex)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aLocaleIdentifier || uStringIndex >= OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	// Get the group structure to get this string from.
	openavb_aem_descriptor_locale_strings_handler_group_t *pGroup = openavbAemDescriptorLocaleStringsHandlerGet_group(pDescriptor, aLocaleIdentifier, FALSE);
	if (!pGroup) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	const char * ret = openavbAemDescriptorStringsGet_string(pGroup->pStrings, uStringIndex);
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return ret;
}

extern DLL_EXPORT bool openavbAemDescriptorLocaleStringsHandlerSet_local_string(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	const char *aLocaleIdentifier,
	const char *aString,
	U8 uStringIndex)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aLocaleIdentifier || !aString || uStringIndex >= OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	// Get the group structure to add this string to.  Create it if needed.
	openavb_aem_descriptor_locale_strings_handler_group_t *pGroup = openavbAemDescriptorLocaleStringsHandlerGet_group(pDescriptor, aLocaleIdentifier, TRUE);
	if (!pGroup) {
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	bool ret = openavbAemDescriptorStringsSet_string(pGroup->pStrings, aString, uStringIndex);
	if (ret) {
		openavbAemDescriptorLocaleSet_number_of_strings(pGroup->pLocale, 1);
		openavbAemDescriptorLocaleSet_base_strings(pGroup->pLocale, 0);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return ret;
}

extern DLL_EXPORT bool openavbAemDescriptorLocaleStringsHandlerAddToConfiguration(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor, U16 nConfigIdx)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_locale_strings_handler_group_t *pCurrent = pDescriptor->pFirstGroup;
	while (pCurrent) {
		U16 nResultIdx;

		// Add the locale to the configuration.
		if (!openavbAemAddDescriptor(pCurrent->pLocale, nConfigIdx, &nResultIdx)) {
			AVB_LOG_ERROR("Locale Add Descriptor Failure");
			AVB_TRACE_EXIT(AVB_TRACE_AEM);
			return false;
		}

		// Add the strings descriptor, if it hasn't already been added.
		if (openavbAemGetDescriptorIndex(OPENAVB_AEM_DESCRIPTOR_INVALID, pCurrent->pStrings) == OPENAVB_AEM_DESCRIPTOR_INVALID) {
			if (!openavbAemAddDescriptor(pCurrent->pStrings, OPENAVB_AEM_DESCRIPTOR_INVALID /* Not configuration-specific */, &nResultIdx)) {
				AVB_LOG_ERROR("Strings Add Descriptor Failure");
				AVB_TRACE_EXIT(AVB_TRACE_AEM);
				return false;
			}
		}

		pCurrent = pCurrent->pNext;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return true;
}
