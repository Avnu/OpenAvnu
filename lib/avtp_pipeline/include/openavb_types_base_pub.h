/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
* MODULE : Public AVB Base Types (have no dependencies)
*/

#ifndef AVB_TYPES_BASE_PUB_H
#define AVB_TYPES_BASE_PUB_H 1

#include "openavb_types_base_tcal_pub.h"
#include <stdbool.h>
#include <inttypes.h>

/** \file
 * Common Base AVB Types that are exposed outside of the AVB 
 * stack. 
 */

/*
 * Useful types for OPENAVB AVB
 *
 */
/// Number of nanoseconds in second
#define NANOSECONDS_PER_SECOND		(1000000000ULL)
/// Number of nanoseconds in milisecond
#define NANOSECONDS_PER_MSEC		(1000000L)
/// Number of nanoseconds in microsecond
#define NANOSECONDS_PER_USEC		(1000L)
/// Number of microseconds in second
#define MICROSECONDS_PER_SECOND		(1000000L)
/// Number of microseconds in milisecond
#define MICROSECONDS_PER_MSEC		(1000L)

#ifndef TRUE // possible confict with gboolean
/// True boolean value
#define TRUE  true
/// False boolean value
#define FALSE false
#endif

#ifndef NULL
/// Null pointer value
#define NULL 0
#endif

/// Signed 8 bit type
typedef int8_t		S8;
/// Unsigned 8 bit type
typedef uint8_t		U8;
/// Signed 16 bit type
typedef int16_t		S16;
/// Unsigned 16 bit type
typedef uint16_t	U16;
/// Signed 32 bit type
typedef int32_t		S32;
/// Unsigned 32 bit type
typedef uint32_t	U32;
/// Signed 64 bit type
typedef int64_t		S64;
/// Unsigned 64 bit type
typedef uint64_t	U64;

/// Describes role of the host
typedef enum {
	/// Role undefined or wrong handle
	AVB_ROLE_UNDEFINED = 0,
	/// Host acts as a talker
	AVB_ROLE_TALKER,
	/// Host acts as a listener
	AVB_ROLE_LISTENER
} avb_role_t;



/// Supported AVB classes.
typedef enum {
	/// Stream reservation class A. 8000 packets per second
	SR_CLASS_A,
	/// Stream reservation class B. 4000 packets per second
	SR_CLASS_B,
//	SR_CLASS_C,
//	SR_CLASS_D,
	/// Number of supported stream reservation classes
	MAX_AVB_SR_CLASSES
} SRClassIdx_t;

/// Regular
#define SR_RANK_REGULAR   1
/// Emergency
#define SR_RANK_EMERGENCY 0

#endif // AVB_TYPES_BASE_PUB_H
