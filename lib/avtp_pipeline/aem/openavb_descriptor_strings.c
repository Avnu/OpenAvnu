/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Strings Descriptor
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Implements the AVDECC Strings IEEE Std 1722.1-2013 clause 7.2.12 
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
#include "openavb_descriptor_strings.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorStringsToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_strings_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_STRINGS_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_strings_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	memcpy(pDst, pSrc->string, sizeof(pSrc->string));
	pDst += sizeof(pSrc->string);
	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorStringsFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_strings_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_STRINGS_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_strings_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	memcpy(pDst->string, pSrc, sizeof(pDst->string));
	pSrc += sizeof(pDst->string);

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_strings_t *openavbAemDescriptorStringsNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_strings_t *pDescriptor;

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

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_strings_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = FALSE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorStringsToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorStringsFromBuf;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_STRINGS;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT const char * openavbAemDescriptorStringsGet_string(openavb_aem_descriptor_strings_t *pDescriptor, U8 index)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || index >= OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return NULL;
	}

	const char *ret = (const char *) (pDescriptor->string[index]);
	if (*ret == '\0') {
		/* The string is empty. */
		ret = NULL;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return ret;
}

extern DLL_EXPORT bool openavbAemDescriptorStringsSet_string(openavb_aem_descriptor_strings_t *pDescriptor, const char *pString, U8 index)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor || !pString || index >= OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return FALSE;
	}

	openavbAemSetString(pDescriptor->string[index], pString);

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return TRUE;
}

extern DLL_EXPORT U16 openavbAemDescriptorStringsGet_number_of_strings(openavb_aem_descriptor_strings_t *pDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);
	int nFirst, nLast;

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return 0;
	}

	for (nFirst = 0; nFirst < OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS; ++nFirst) {
		if (pDescriptor->string[nFirst][0] != '\0') { break; }
	}
	if (nFirst >= OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS) {
		// There are no strings!
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return 0;
	}
	for (nLast = OPENAVB_AEM_NUM_DESCRIPTOR_STRINGS - 1; nLast > nFirst; --nLast) {
		if (pDescriptor->string[nLast][0] != '\0') { break; }
	}

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return (nLast - nFirst + 1);
}

extern DLL_EXPORT U16 openavbAemDescriptorStringsGet_base_strings(openavb_aem_descriptor_strings_t *pDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	if (!pDescriptor) {
		AVB_RC_LOG(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT));
		AVB_TRACE_EXIT(AVB_TRACE_AEM);
		return 0;
	}

	U16 ret = pDescriptor->descriptor_index;
	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return ret;
}
