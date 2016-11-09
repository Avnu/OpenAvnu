/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AEM - AVDECC Control Descriptor Public Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       26-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Pulbic Interface for the Control Desciptor
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_DESCRIPTOR_CONTROL_PUB_H
#define OPENAVB_DESCRIPTOR_CONTROL_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_aem_types_pub.h"
#include "openavb_aem_pub.h"

// CONTROL Descriptor IEEE Std 1722.1-2013 clause 7.2.22
typedef struct {
	openavb_descriptor_pvt_ptr_t descriptorPvtPtr;

	U16 descriptor_type;											// Set internally
	U16 descriptor_index;											// Set internally
	U8 object_name[OPENAVB_AEM_STRLEN_MAX];
	openavb_aem_string_ref_t localized_description;
	U32 block_latency;
	U32 control_latency;
	U16 control_domain;
	U16 control_value_type;
	U64 control_type;
	U32 reset_time;
	U16 values_offset;
	U16 number_of_values;
	U16 signal_type;
	U16 signal_index;
	U16 signal_output;
	union {
		openavb_aem_control_value_format_control_linear_int8_t linear_int8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_INT8];
		openavb_aem_control_value_format_control_linear_uint8_t linear_uint8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_UINT8];
		openavb_aem_control_value_format_control_linear_int16_t linear_int16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_INT16];
		openavb_aem_control_value_format_control_linear_uint16_t linear_uint16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_UINT16];
		openavb_aem_control_value_format_control_linear_int32_t linear_int32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_INT32];
		openavb_aem_control_value_format_control_linear_uint32_t linear_uint32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_UINT32];
		openavb_aem_control_value_format_control_linear_int64_t linear_int64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_INT64];
		openavb_aem_control_value_format_control_linear_uint64_t linear_uint64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_UINT64];
		openavb_aem_control_value_format_control_linear_float_t linear_float[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_FLOAT];
		openavb_aem_control_value_format_control_linear_double_t linear_double[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_LINEAR_DOUBLE];
		openavb_aem_control_value_format_control_selector_int8_t selector_int8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_INT8];
		openavb_aem_control_value_format_control_selector_uint8_t selector_uint8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_UINT8];
		openavb_aem_control_value_format_control_selector_int16_t selector_int16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_INT16];
		openavb_aem_control_value_format_control_selector_uint16_t selector_uint16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_UINT16];
		openavb_aem_control_value_format_control_selector_int32_t selector_int32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_INT32];
		openavb_aem_control_value_format_control_selector_uint32_t selector_uint32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_UINT32];
		openavb_aem_control_value_format_control_selector_int64_t selector_int64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_INT64];
		openavb_aem_control_value_format_control_selector_uint64_t selector_uint64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_UINT64];
		openavb_aem_control_value_format_control_selector_float_t selector_float[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_FLOAT];
		openavb_aem_control_value_format_control_selector_double_t selector_double[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_DOUBLE];
		openavb_aem_control_value_format_control_selector_string_t selector_string[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SELECTOR_STRING];
		openavb_aem_control_value_format_control_array_int8_t array_int8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_INT8];
		openavb_aem_control_value_format_control_array_uint8_t array_uint8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_UINT8];
		openavb_aem_control_value_format_control_array_int16_t array_int16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_INT16];
		openavb_aem_control_value_format_control_array_uint16_t array_uint16[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_UINT16];
		openavb_aem_control_value_format_control_array_int32_t array_int32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_INT32];
		openavb_aem_control_value_format_control_array_uint32_t array_uint32[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_UINT32];
		openavb_aem_control_value_format_control_array_int64_t array_int64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_INT64];
		openavb_aem_control_value_format_control_array_uint64_t array_uint64[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_UINT64];
		openavb_aem_control_value_format_control_array_float_t array_float[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_FLOAT];
		openavb_aem_control_value_format_control_array_double_t array_double[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_ARRAY_DOUBLE];
		openavb_aem_control_value_format_control_utf8_t utf8[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_UTF8];
		openavb_aem_control_value_format_control_bode_plot_t bode_plot[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_BODE_PLOT];
		openavb_aem_control_value_format_control_smpte_time_t smpte_time[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SMPTE_TIME];
		openavb_aem_control_value_format_control_sample_rate_t sample_rate[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_SAMPLE_RATE];
		openavb_aem_control_value_format_control_gptp_time_t gptp_time[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_GPTP_TIME];
		openavb_aem_control_value_format_control_vendor_t vendor[OPENAVB_AEM_VALUE_MAX_COUNT_CONTROL_VENDOR];
	} value_details;
} openavb_aem_descriptor_control_t;


openavb_aem_descriptor_control_t *openavbAemDescriptorControlNew(void);

#endif // OPENAVB_DESCRIPTOR_CONTROL_PUB_H
