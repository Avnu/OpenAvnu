/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
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

#ifndef _OSDEP_H_
#define _OSDEP_H_

/* generic boolean compatibility */
#undef TRUE
#undef FALSE
#define TRUE true
#define FALSE false
#ifdef GCC_VERSION
#if ( GCC_VERSION < 3000 )
#define _Bool char
#endif
#else
#define _Bool char
#endif
#ifndef bool
#define bool _Bool
#define true 1
#define false 0
#endif

typedef u_int64_t	u64;
typedef u_int32_t	u32;
typedef u_int16_t	u16;
typedef u_int8_t		u8;
typedef int64_t		s64;
typedef int32_t		s32;
typedef int16_t		s16;
typedef int8_t		s8;

#define __le16		u16
#define __le32		u32
#define __le64		u64

#define mb()
#define rmb()
#define wmb()
#define prefetch(x)

#define E1000_REGISTER(hw, reg) reg

#define E1000_WRITE_FLUSH(a) E1000_READ_REG(a, E1000_STATUS)

/* Register READ/WRITE macros */

#define E1000_READ_REG(hw, reg) \
    (*(u32 *)(((hw)->hw_addr + reg)))

#define E1000_WRITE_REG(hw, reg, value) \
    (*(u32 *)(((hw)->hw_addr + reg)) = value)

#endif  /* _OSDEP_H_ */

