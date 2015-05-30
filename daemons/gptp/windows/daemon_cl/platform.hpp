/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation 
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

/**@file*/

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <stdio.h>

/**
 * Provides strncpy
 */
errno_t PLAT_strncpy( char *dest, const char *src, rsize_t max );
/**
 * provides snprintf
 */
#define PLAT_snprintf(buffer,count,...) _snprintf_s(buffer,count,count,__VA_ARGS__);

/**
 * @brief  Converts the unsigned short integer hostshort
 * from host byte order to network byte order.
 * @param s short host byte order
 * @return short value in network order
 */
uint16_t PLAT_htons( uint16_t s );

/**
 * @brief  Converts the unsigned integer hostlong
 * from host byte order to network byte order.
 * @param  l Host long byte order
 * @return value in network byte order
 */
uint32_t PLAT_htonl( uint32_t l );

/**
 * @brief  Converts the unsigned short integer netshort
 * from network byte order to host byte order.
 * @param s Network order short integer
 * @return host order value
 */
uint16_t PLAT_ntohs( uint16_t s );

/**
 * @brief  Converts the unsigned integer netlong
 * from network byte order to host byte order.
 * @param l Long value in network order
 * @return Long value on host byte order
 */
uint32_t PLAT_ntohl( uint32_t l );

#endif
