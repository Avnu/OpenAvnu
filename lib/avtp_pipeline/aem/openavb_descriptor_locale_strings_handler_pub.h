/*
 ******************************************************************
 * COPYRIGHT � Harman International
 *----------------------------------------------------------------*
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


// Argument is the index for the configuration that the locale strings should be associated with.
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
