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
 * MODULE : AEM - AVDECC Control Descriptor
 * MODULE SUMMARY : Implements the AVDECC Control IEEE Std 1722.1-2013 clause 7.2.32 
 ******************************************************************
 */

#include <stdlib.h>

#define	AVB_LOG_COMPONENT	"AEM"
#include "openavb_log.h"

#include "openavb_rawsock.h"
#include "openavb_aem.h"
#include "openavb_descriptor_control.h"


////////////////////////////////
// Private (internal) functions
////////////////////////////////
openavbRC openavbAemDescriptorControlToBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf, U16 *descriptorSize)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_control_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf || !descriptorSize) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_CONTROL_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	*descriptorSize = 0;

	openavb_aem_descriptor_control_t *pSrc = pDescriptor;
	U8 *pDst = pBuf;

	OCT_D2BHTONS(pDst, pSrc->descriptor_type);
	OCT_D2BHTONS(pDst, pSrc->descriptor_index);
	OCT_D2BMEMCP(pDst, pSrc->object_name);
	BIT_D2BHTONS(pDst, pSrc->localized_description.offset, 3, 0);
	BIT_D2BHTONS(pDst, pSrc->localized_description.index, 0, 2);
	OCT_D2BHTONL(pDst, pSrc->block_latency);
	OCT_D2BHTONL(pDst, pSrc->control_latency);
	OCT_D2BHTONS(pDst, pSrc->control_domain);
	OCT_D2BHTONS(pDst, pSrc->control_value_type);
	OCT_D2BMEMCP(pDst, &pSrc->control_type);
	OCT_D2BHTONL(pDst, pSrc->reset_time);
	OCT_D2BHTONS(pDst, pSrc->values_offset);
	OCT_D2BHTONS(pDst, pSrc->number_of_values);
	OCT_D2BHTONS(pDst, pSrc->signal_type);
	OCT_D2BHTONS(pDst, pSrc->signal_index);
	OCT_D2BHTONS(pDst, pSrc->signal_output);

	int i1;
	for (i1 = 0; i1 < pSrc->number_of_values; i1++) {
		switch (pSrc->control_value_type) {
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT8:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT8:
				OCT_D2BHTONB(pDst, pSrc->value_details.linear_int8[i1].minimum);
				OCT_D2BHTONB(pDst, pSrc->value_details.linear_int8[i1].maximum);
				OCT_D2BHTONB(pDst, pSrc->value_details.linear_int8[i1].step);
				OCT_D2BHTONB(pDst, pSrc->value_details.linear_int8[i1].default_value);
				OCT_D2BHTONB(pDst, pSrc->value_details.linear_int8[i1].current);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int8[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int8[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int8[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT16:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT16:
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].minimum);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].maximum);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].step);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].default_value);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].current);
				OCT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int16[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT32:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT32:
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].minimum);
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].maximum);
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].step);
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].default_value);
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].current);
				OCT_D2BHTONL(pDst, pSrc->value_details.linear_int32[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int32[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int32[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT64:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT64:
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].minimum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].maximum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].step);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].default_value);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].current);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_int64[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int64[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_int64[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_FLOAT:
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].minimum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].maximum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].step);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].default_value);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].current);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_float[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_float[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_float[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_DOUBLE:
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].minimum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].maximum);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].step);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].default_value);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].current);
				OCT_D2BMEMCP(pDst, &pSrc->value_details.linear_double[i1].unit);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_double[i1].string.offset, 3, 0);
				BIT_D2BHTONS(pDst, pSrc->value_details.linear_double[i1].string.index, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_FLOAT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_DOUBLE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_STRING:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_FLOAT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_DOUBLE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_UTF8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_BODE_PLOT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SMPTE_TIME:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SAMPLE_RATE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_GPTP_TIME:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_VENDOR:
				// AVDECC_TODO
				break;
		}
	}

	*descriptorSize = pDst - pBuf;

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorControlFromBuf(void *pVoidDescriptor, U16 bufLength, U8 *pBuf)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_control_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor || !pBuf) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	if (bufLength < OPENAVB_DESCRIPTOR_CONTROL_BASE_LENGTH) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_BUFFER_TOO_SMALL), AVB_TRACE_AEM);
	}

	U8 *pSrc = pBuf;
	openavb_aem_descriptor_control_t *pDst = pDescriptor;

	OCT_B2DNTOHS(pDst->descriptor_type, pSrc);
	OCT_B2DNTOHS(pDst->descriptor_index, pSrc);
	OCT_B2DMEMCP(pDst->object_name, pSrc);
	BIT_B2DNTOHS(pDst->localized_description.offset, pSrc, 0xfff8, 3, 0);
	BIT_B2DNTOHS(pDst->localized_description.index, pSrc, 0x0007, 0, 2);
	OCT_B2DNTOHL(pDst->block_latency, pSrc);
	OCT_B2DNTOHL(pDst->control_latency, pSrc);
	OCT_B2DNTOHS(pDst->control_domain, pSrc);
	OCT_B2DNTOHS(pDst->control_value_type, pSrc);
	OCT_B2DMEMCP(&pDst->control_type, pSrc);
	OCT_B2DNTOHL(pDst->reset_time, pSrc);
	OCT_B2DNTOHS(pDst->values_offset, pSrc);
	OCT_B2DNTOHS(pDst->number_of_values, pSrc);
	OCT_B2DNTOHS(pDst->signal_type, pSrc);
	OCT_B2DNTOHS(pDst->signal_index, pSrc);
	OCT_B2DNTOHS(pDst->signal_output, pSrc);


	int i1;
	for (i1 = 0; i1 < pDst->number_of_values; i1++) {
		switch (pDst->control_value_type) {
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT8:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT8:
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].minimum, pSrc);
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].maximum, pSrc);
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].step, pSrc);
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].default_value, pSrc);
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].current, pSrc);
				OCT_B2DNTOHB(pDst->value_details.linear_int8[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_int8[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_int8[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT16:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT16:
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].minimum, pSrc);
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].maximum, pSrc);
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].step, pSrc);
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].default_value, pSrc);
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].current, pSrc);
				OCT_B2DNTOHS(pDst->value_details.linear_int16[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_int16[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_int16[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT32:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT32:
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].minimum, pSrc);
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].maximum, pSrc);
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].step, pSrc);
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].default_value, pSrc);
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].current, pSrc);
				OCT_B2DNTOHL(pDst->value_details.linear_int32[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_int32[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_int32[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_INT64:
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_UINT64:
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].minimum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].maximum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].step, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].default_value, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].current, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_int64[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_int64[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_int64[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_FLOAT:
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].minimum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].maximum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].step, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].default_value, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].current, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_float[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_float[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_float[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_LINEAR_DOUBLE:
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].minimum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].maximum, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].step, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].default_value, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].current, pSrc);
				OCT_B2DMEMCP(&pDst->value_details.linear_double[i1].unit, pSrc);
				BIT_B2DNTOHS(pDst->value_details.linear_double[i1].string.offset, pSrc, 0xfff8, 3, 0);
				BIT_B2DNTOHS(pDst->value_details.linear_double[i1].string.index, pSrc, 0x0007, 0, 2);
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_INT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_UINT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_FLOAT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_DOUBLE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SELECTOR_STRING:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT16:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT32:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_INT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_UINT64:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_FLOAT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_ARRAY_DOUBLE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_UTF8:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_BODE_PLOT:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SMPTE_TIME:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_SAMPLE_RATE:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_GPTP_TIME:
				// AVDECC_TODO
				break;
			case OPENAVB_AEM_CONTROL_VALUE_TYPE_CONTROL_VENDOR:
				// AVDECC_TODO
				break;
		}
	}

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

openavbRC openavbAemDescriptorControlUpdate(void *pVoidDescriptor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_control_t *pDescriptor = pVoidDescriptor;

	if (!pDescriptor) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	AVB_RC_TRACE_RET(OPENAVB_AVDECC_SUCCESS, AVB_TRACE_AEM);
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT openavb_aem_descriptor_control_t *openavbAemDescriptorControlNew()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AEM);

	openavb_aem_descriptor_control_t *pDescriptor;

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

	pDescriptor->descriptorPvtPtr->size = sizeof(openavb_aem_descriptor_control_t);
	pDescriptor->descriptorPvtPtr->bTopLevel = TRUE;
	pDescriptor->descriptorPvtPtr->toBuf = openavbAemDescriptorControlToBuf;
	pDescriptor->descriptorPvtPtr->fromBuf = openavbAemDescriptorControlFromBuf;
	pDescriptor->descriptorPvtPtr->update = openavbAemDescriptorControlUpdate;

	pDescriptor->descriptor_type = OPENAVB_AEM_DESCRIPTOR_CONTROL;
	pDescriptor->values_offset = OPENAVB_DESCRIPTOR_CONTROL_BASE_LENGTH;

	// Default to no localized strings.
	pDescriptor->localized_description.offset = OPENAVB_AEM_NO_STRING_OFFSET;
	pDescriptor->localized_description.index = OPENAVB_AEM_NO_STRING_INDEX;

	AVB_TRACE_EXIT(AVB_TRACE_AEM);
	return pDescriptor;
}

extern DLL_EXPORT bool openavbAemDescriptorDescriptorControlInitialize(openavb_aem_descriptor_control_t *pDescriptor, U16 nConfigIdx, const openavb_avdecc_configuration_cfg_t *pConfig)
{
	(void) nConfigIdx;
	if (!pDescriptor || !pConfig) {
		AVB_RC_LOG_TRACE_RET(AVB_RC(OPENAVB_AVDECC_FAILURE | OPENAVB_RC_INVALID_ARGUMENT), AVB_TRACE_AEM);
	}

	// AVDECC_TODO - Any updates needed?

	return TRUE;
}
