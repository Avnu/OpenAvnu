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

#ifndef UTILS_H
#define UTILS_H

#include <sdk.h>

#define container_addr(member, addr, type) \
	(type *)(((char *)addr) - ((char *)&((type *)0)->member))

static inline bool
isaudk_is_format_equal( const struct isaudk_format *a,
			const struct isaudk_format *b )
{
	if( a->encoding != b->encoding )
		return false;
	if( a->channels != b->channels )
		return false;
	return true;
}

static inline void *
get_buffer_pointer( struct isaudk_format *format,
		    isaudk_sample_block_t *buffer )
{
	switch( format->encoding )
	{
	default:
		// Unsupported format request
		return NULL;
	case ISAUDK_ENC_PS16:
		return buffer->PS16;
	}

	return NULL;
}

#define UINT8_ADD( x, y )			\
	(( (x) + (y) ) % ( UCHAR_MAX + 1 ))

#endif/*UTILS_H*/
