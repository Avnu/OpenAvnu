/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Locale Descriptor
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Locale IEEE Std 1722.1-2013 clause 7.2.11 
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_locale.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorLocaleToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_locale_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_LOCALE_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_locale_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->locale_identifier);
	OCT_D2BHTONS(pDst, pSrc->number_of_strings);
	OCT_D2BHTONS(pDst, pSrc->base_strings);

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorLocaleFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_locale_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_LOCALE_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_locale_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->locale_identifier, pSrc);
	OCT_B2DNTOHS(pDst->number_of_strings, pSrc);
	OCT_B2DNTOHS(pDst->base_strings, pSrc);

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_locale_t *openavbAemDescriptorLocaleNew(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_locale_t *pDescriptor;

	pDescriptor = malloc(sizeof(*pDescriptor));

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}
	memset(pDescriptor, 0, sizeof(*pDescriptor));

	pDescriptor->descriptorPvtPtr = malloc(sizeof(*pDescriptor->descriptorPvtPtr));
	if (!pDescriptor->descriptorPvtPtr) {
		free(pDescriptor);
		pDescriptor = NULL;
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_OUT_OF_MEMORY));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_locale_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorLocaleToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorLocaleFromBuf;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_LOCALE;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorLocaleSet_locale_identifier(openavb_aem_descriptor_locale_t *pDescriptor, const char *aLocaleIdentifier)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !aLocaleIdentifier) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	memset(pDescriptor->locale_identifier, 0, OPENAVB_AEM_STRLEN_MAX);
	strncpy((char *) pDescriptor->locale_identifier, aLocaleIdentifier, OPENAVB_AEM_STRLEN_MAX);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorLocaleSet_number_of_strings(openavb_aem_descriptor_locale_t *pDescriptor, U16 uNumberOfStrings)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->number_of_strings = uNumberOfStrings;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT bool openavbAemDescriptorLocaleSet_base_strings(openavb_aem_descriptor_locale_t *pDescriptor, U16 uBaseStrings)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	pDescriptor->base_strings = uBaseStrings;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}
