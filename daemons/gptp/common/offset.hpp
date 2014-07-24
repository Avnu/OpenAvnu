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

#ifndef OFFSET_HPP
#define OFFSET_HPP

#include <math.h>

/*
  * Meaning of clock_offset_t values:

  Integer64  <Master-Local      PHase OFFSET>
  LongDouble <Master-Local      FREQuency OFFSET>
  Integer64  <Local -System     PHase OFFSET>
  LongDouble <Local -System     FREQuency OFFSET>
  Integer64  <Local -Auxiliary  PHase OFFSET>
  LongDouble <Local -Auxiliary  FREQuency OFFSET>
  UInteger64 <802.1AS Master time of last update>

  local     ~= master + <master-local phase offset>
  system    ~= master + <master-local phase offset> +
  <local-system phase offset>
  auxiliary ~= master + <master-local phase offset> +
  <local-auxiliary phase offset>

  Dmaster ~= Dlocal       * <master-local frequency offset>  
  Dmaster ~= Dsystem      * <master-local frequency offset> *
  <local-system frequency offset>
  Dmaster ~= Dauxiliary   * <master-local frequency offset> *
  <local-auxiliary frequency offset>
*/

#pragma pack(push,1)

typedef struct {
	int64_t ml_phoffset;
	FrequencyRatio ml_freqoffset;
	int64_t ls_phoffset;
	FrequencyRatio ls_freqoffset;
	int64_t la_phoffset;
	FrequencyRatio la_freqoffset;
	uint64_t master_time;
	uint32_t  sync_count;
	uint32_t  pdelay_count;
} clock_offset_t;

#pragma pack(pop)

static inline uint64_t
MasterToLocalTime( uint64_t master_time_i, clock_offset_t *offset ) {
	int64_t master_clock_delta;
	int64_t local_clock_delta;
	uint64_t local_time = offset->master_time + offset->ml_phoffset;
        
	master_clock_delta = offset->master_time <= master_time_i ?
		master_time_i - offset->master_time :
		-1*(offset->master_time - master_time_i);
	local_clock_delta = (int64_t)
		roundl(((long double)master_clock_delta)/offset->ml_freqoffset);
        
	return local_clock_delta > 0 ?
		local_time + (uint64_t) local_clock_delta :
		local_time - (uint64_t) (-1*local_clock_delta);
}

static inline uint64_t
LocalToMasterTime( uint64_t local_time_i, clock_offset_t *offset ) {
	int64_t local_clock_delta;
	int64_t master_clock_delta;
	uint64_t local_time = offset->master_time + offset->ml_phoffset;
        
	local_clock_delta = local_time <= local_time_i ?
		local_time_i - local_time :
		-1*(local_time - local_time_i);
	master_clock_delta = (int64_t)
		roundl(((long double)local_clock_delta)*offset->ml_freqoffset);
        
	return master_clock_delta > 0 ?
		offset->master_time + (uint64_t) master_clock_delta :
		offset->master_time - (uint64_t) (-1*master_clock_delta);
}


#endif/*OFFSET_HPP*/
