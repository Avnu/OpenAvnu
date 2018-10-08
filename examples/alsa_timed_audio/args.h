/******************************************************************************

  Copyright (c) 2018, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef ISAUDK_ARGS_H
#define ISAUDK_ARGS_H

#include <sdk.h>

#define STRING_PTR_TYPE char *
#define TIME_TYPE uint64_t

extern char *isaudk_input_file_arg;
#define ISAUDK_INPUT_FILE_VAR input_file
#define ISAUDK_INPUT_FILE_IDX 0
#define ISAUDK_INPUT_FILE_TYPE STRING_PTR_TYPE
#define ISAUDK_INPUT_FILE_ARG isaudk_input_file_arg
#define ISAUDK_INPUT_FILE_PARSE_FN generic_string_parse

extern char *isaudk_output_file_arg;
#define ISAUDK_OUTPUT_FILE_VAR output_file
#define ISAUDK_OUTPUT_FILE_IDX 1
#define ISAUDK_OUTPUT_FILE_TYPE STRING_PTR_TYPE
#define ISAUDK_OUTPUT_FILE_ARG isaudk_output_file_arg
#define ISAUDK_OUTPUT_FILE_PARSE_FN generic_string_parse

extern char *isaudk_start_time_arg;
#define ISAUDK_START_TIME_VAR start_time
#define ISAUDK_START_TIME_IDX 2
#define ISAUDK_START_TIME_TYPE TIME_TYPE
#define ISAUDK_START_TIME_ARG isaudk_start_time_arg
#define ISAUDK_START_TIME_PARSE_FN generic_u64_parse

extern char *isaudk_duration_arg;
#define ISAUDK_DURATION_VAR duration
#define ISAUDK_DURATION_IDX 3
#define ISAUDK_DURATION_TYPE uint32_t
#define ISAUDK_DURATION_ARG isaudk_duration_arg
#define ISAUDK_DURATION_PARSE_FN generic_u32_parse

extern char *isaudk_systime_arg;
#define ISAUDK_SYSTIME_VAR systime
#define ISAUDK_SYSTIME_IDX 4
#define ISAUDK_SYSTIME_TYPE uint64_t
#define ISAUDK_SYSTIME_ARG isaudk_systime_arg
#define ISAUDK_SYSTIME_PARSE_FN generic_u64_parse

extern char *isaudk_nettime_arg;
#define ISAUDK_NETTIME_VAR nettime
#define ISAUDK_NETTIME_IDX 5
#define ISAUDK_NETTIME_TYPE uint64_t
#define ISAUDK_NETTIME_ARG isaudk_nettime_arg
#define ISAUDK_NETTIME_PARSE_FN generic_u64_parse

#define ISAUDK_DECLARE_VAR(x) ISAUDK_##x##_TYPE * ISAUDK_##x##_VAR
#define _ISAUDK_DECLARE_ARG(x,y,z) { .label = ISAUDK_##x##_ARG, \
			.arg = { . ISAUDK_##x##_VAR = y },		\
			.found = false, .dynamic = false, .optional = z, \
				 .index = ISAUDK_##x##_IDX }

#define ISAUDK_DECLARE_OPTIONAL_ARG(x,y) _ISAUDK_DECLARE_ARG(x,y,true)
#define ISAUDK_DECLARE_REQUIRED_ARG(x,y) _ISAUDK_DECLARE_ARG(x,y,false)

typedef union
{
	ISAUDK_DECLARE_VAR(OUTPUT_FILE);
	ISAUDK_DECLARE_VAR(INPUT_FILE);
	ISAUDK_DECLARE_VAR(START_TIME);
	ISAUDK_DECLARE_VAR(DURATION);
	ISAUDK_DECLARE_VAR(SYSTIME);
	ISAUDK_DECLARE_VAR(NETTIME);
} _isaudk_arg_t;

struct isaudk_arg
{
	char *label;
	unsigned index;
	bool found;
	bool optional;
	bool dynamic;
	_isaudk_arg_t arg;
};

typedef enum
{
	ISAUDK_PARSE_SUCCESS,
	ISAUDK_PARSE_ARG_FAIL,
	ISAUDK_PARSE_ARG_NOT_FOUND,
	ISAUDK_PARSE_ARG_UNKNOWN,
	ISAUDK_PARSE_ARG_UNKNOWN_INDEX,
} isaudk_parse_error_t;

int isaudk_parse_args( struct isaudk_arg *args, size_t count, int argc,
		       char **argv, isaudk_parse_error_t *parse_error );

#endif/*ISAUDK_ARGS_H*/
