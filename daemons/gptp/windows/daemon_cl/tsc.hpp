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

#ifndef TSC_HPP
#define TSC_HPP

#include <intrin.h>
#include <stdint.h>

#define BASE_FREQUENCY  100000000

inline unsigned __int64 PLAT_rdtsc()
{
	return __rdtsc();
}

inline uint64_t getTSCFrequency( unsigned millis ) {
	UINT16 multiplierx2;
	uint64_t frequency = 0;
	DWORD mhz;
	DWORD mhz_sz = sizeof(mhz);

	if( RegGetValue( HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "~MHz", RRF_RT_REG_DWORD, NULL, &mhz, &mhz_sz ) != ERROR_SUCCESS ) {
		goto done;
	}
	fprintf( stderr, "mhz: %u\n", mhz );
	multiplierx2 = (UINT16)((2*mhz*1000000ULL)/BASE_FREQUENCY);
	if( multiplierx2 % 2 == 1 ) ++multiplierx2;
	fprintf( stderr, "Multiplier: %hhu\n", multiplierx2/2 );

	frequency = (((uint64_t)multiplierx2)*BASE_FREQUENCY)/2;
	fprintf( stderr, "Frequency: %llu\n", frequency );

done:
	return frequency;
}

#endif/*TSC_HPP*/
