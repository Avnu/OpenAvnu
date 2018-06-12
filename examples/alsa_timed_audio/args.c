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

#include <args.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

char *isaudk_input_file_arg = "if";
char *isaudk_output_file_arg = "of";
char *isaudk_start_time_arg = "st";
char *isaudk_duration_arg = "dn";
char *isaudk_systime_arg = "syst";
char *isaudk_nettime_arg = "netwkt";

// Change string to lower case
static inline void str_lower( char *str )
{
	char *c;
	for( c = str; *c != '\0'; ++c )
		*c = tolower(*c);
}

static inline isaudk_parse_error_t
generic_string_parse( char **dest, char *src, bool *dynamic )
{
	*dynamic = true;
	*dest = (__typeof__(*dest))
		malloc((size_t) sizeof(char)*(strlen(src)+1));
	strncpy( *dest, src, strlen(src)+1 );

	return ISAUDK_PARSE_SUCCESS;
}

static inline isaudk_parse_error_t
generic_u64_parse( uint64_t *dest, char *src, bool *dynamic )
{
	*dynamic = false;
	*dest = strtoull( src, NULL, 0 );

	if( *dest != ULLONG_MAX )
		return ISAUDK_PARSE_SUCCESS;
	else
		return ISAUDK_PARSE_ARG_FAIL;
}

static inline isaudk_parse_error_t
generic_u32_parse( uint32_t *dest, char *src, bool *dynamic )
{
	uint64_t tmp;

	*dynamic = false;
	tmp = strtoull( src, NULL, 0 );

	if( tmp != ULLONG_MAX && tmp <= UINT_MAX )
	{
		*dest = tmp & UINT_MAX;
		return ISAUDK_PARSE_SUCCESS;
	}
	else
		return ISAUDK_PARSE_ARG_FAIL;
}

// Sort arguments, bubble sort
static void sort_arg( struct isaudk_arg **arg, size_t count )
{
	size_t i;
	struct isaudk_arg *tmp;
	int inorder = 0;

	while( !inorder )
	{
		inorder = 1;
		for( i = 0; i < count-1; ++i ) {
			if( strcmp( arg[i]->label, arg[i+1]->label ) > 0 ) {
				tmp		= arg[i];
				arg[i]		= arg[i+1];
				arg[i+1]	= tmp;
				inorder = 0;
			}
		}
	}
}

// Find entry corresponding to argument
static struct isaudk_arg *find_arg_n( struct isaudk_arg **arg, size_t count,
				      char *match, size_t n )
{
	size_t i;

	for( i = 0; i < count; ++i )
	{
		int order = strncmp( match, arg[i]->label, n );
		if( order == 0 )
			return arg[i];
		if( order < 0 )
			return NULL;
	}
	return NULL;
}

// Find length of flag, stop at punctuation or NUL
static size_t flaglen( char *flag )
{
	char *saveflag = flag;

	while( !ispunct( *flag ) && *flag != '\0' )
		++flag;
	return flag - saveflag;
}

#define PARSE_OPTION(opt) \
case ISAUDK_##opt##_IDX:				\
	match->found = true;				\
	*parse_error =					\
	ISAUDK_##opt##_PARSE_FN				\
		( match->arg.ISAUDK_##opt##_VAR,	\
		  argv[i]+fln+2, &match->dynamic );	\
							\
	if( *parse_error != ISAUDK_PARSE_SUCCESS )	\
		return i;				\
	break;

int isaudk_parse_args( struct isaudk_arg *_args, size_t count, int argc,
		       char **argv, isaudk_parse_error_t *parse_error )
{
	int i;
	size_t j;

	struct isaudk_arg **args;
	args = (__typeof__(args))
		malloc((size_t) sizeof(struct isaudk_arg *) * count);

	for( j = 0; j < count; ++j )
	{
		args[j] = _args+j;
	}

	sort_arg( args, count );

	i = 0;
	while( i < argc )
	{
		if( ispunct( *argv[i] ))
		{
			struct isaudk_arg *match;
			size_t fln;

			fln = flaglen( argv[i]+1 );
			match = find_arg_n( args, count, argv[i]+1, fln );
			if( match == NULL )
			{
				*parse_error = ISAUDK_PARSE_ARG_UNKNOWN;
				return i;
			}

			switch( match->index )
			{
			default:
				// Fatal internal error
				*parse_error = ISAUDK_PARSE_ARG_UNKNOWN_INDEX;
				return i;

				PARSE_OPTION(INPUT_FILE)
				PARSE_OPTION(OUTPUT_FILE)
				PARSE_OPTION(START_TIME)
				PARSE_OPTION(DURATION)
				PARSE_OPTION(SYSTIME)
				PARSE_OPTION(NETTIME)
			}
			++i;
		}
		else
		{
			// Non-option argument encountered
			*parse_error = ISAUDK_PARSE_SUCCESS;
			break;
		}
	}

	// Check for required arguments that were not found
	for( j = 0; j < count; ++j )
	{
		if( args[j]->optional == false && args[j]->found == false )
		{
			*parse_error = ISAUDK_PARSE_ARG_NOT_FOUND;
			return i;
		}
	}

	return ISAUDK_PARSE_SUCCESS;
}
