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
 * MODULE : AEM - AVDECC Locale Strings Handler Public Interface
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_LOCALE_STRINGS_HANDLER_PUB_H
#define OPENAVB_DESCRIPTOR_LOCALE_STRINGS_HANDLER_PUB_H 1

#include "openavb_descriptor_locale_pub.h"
#include "openavb_descriptor_strings_pub.h"

// Grouping that holds a locale, and the strings for that locale.
struct openavb_aem_descriptor_locale_strings_handler_group {
	openavb_aem_descriptor_locale_t  *pLocale;
	openavb_aem_descriptor_strings_t *pStrings;
	struct openavb_aem_descriptor_locale_strings_handler_group *pNext;
};
typedef struct openavb_aem_descriptor_locale_strings_handler_group openavb_aem_descriptor_locale_strings_handler_group_t;

// Grouping that holds a locale, and the strings for that locale.
typedef struct {
	openavb_aem_descriptor_locale_strings_handler_group_t *pFirstGroup;
} openavb_aem_descriptor_locale_strings_handler_t;


openavb_aem_descriptor_locale_strings_handler_t *openavbAemDescriptorLocaleStringsHandlerNew(void);

void openavbAemDescriptorLocaleStringsHandlerFree(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor);

// Return the group structure for the specified locale.
// If bCanCreate is TRUE, a structure one will be allocated if needed.
openavb_aem_descriptor_locale_strings_handler_group_t * openavbAemDescriptorLocaleStringsHandlerGet_group(
		openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
		const char *aLocaleIdentifier,
		bool bCanCreate);

// Returns the string at the specified index for the given locale,
//  or NULL if the string has not been specified.
const char * openavbAemDescriptorLocaleStringsHandlerGet_local_string(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	const char *aLocaleIdentifier,
	U8 uStringIndex);

// Set a string at the specified index for the given locale.
// If there was a previous string value, it will be overwritten.
// The index must be a value less than OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS.
bool openavbAemDescriptorLocaleStringsHandlerSet_local_string(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	const char *aLocaleIdentifier,
	const char *aString,
	U8 uStringIndex);

// Add the strings to the supplied configuration.
bool openavbAemDescriptorLocaleStringsHandlerAddToConfiguration(
	openavb_aem_descriptor_locale_strings_handler_t *pDescriptor,
	U16 nConfigIdx);

#endif // OPENAVB_DESCRIPTOR_LOCALE_STRINGS_HANDLER_PUB_H
