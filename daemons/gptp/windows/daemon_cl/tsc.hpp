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

#define NEHALEM_BASE_FREQUENCY       133330000
#define SANDY_BRIDGE_BASE_FREQUENCY  100000000
#define IVY_BRIDGE_BASE_FREQUENCY    100000000

inline unsigned __int64 PLAT_rdtsc()
{
  return __rdtsc();
}

inline uint64_t getTSCFrequency( unsigned millis ) {
	int CPUInfo[4];
	UINT8 model_msn;
	UINT16 multiplierx4;
	uint64_t frequency = 0;
	DWORD mhz;
	DWORD mhz_sz = sizeof(mhz);
	unsigned base_frequency;

	__cpuid( CPUInfo, 1 );
	model_msn = (CPUInfo[0] >> 16) & 0xF;
	if( model_msn > 3 || model_msn < 1 ) {
		printf( "Unknown CPU family: 0x%hhxx\n" );
		goto done;
	}
	fprintf( stderr, "CPU Family msn = 0x%hhx\n", model_msn );

	switch( model_msn ) {
	case 1:
		// Nehalem
		base_frequency = NEHALEM_BASE_FREQUENCY;
		break;
	case 2:
		// Sandy Bridge
		base_frequency = SANDY_BRIDGE_BASE_FREQUENCY;
		break;
	case 3:
		// Ivy Bridge
		base_frequency = IVY_BRIDGE_BASE_FREQUENCY;
		break;
	default:
		fprintf( stderr, "Internal Error\n" );
		abort();
	}

	if( RegGetValue( HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "~MHz", RRF_RT_REG_DWORD, NULL, &mhz, &mhz_sz ) != ERROR_SUCCESS ) {
		goto done;
	}
	multiplierx4 = (UINT16)((4*mhz*1000000ULL)/base_frequency);
	if( multiplierx4 % 4 == 3 ) ++multiplierx4;
	else if( multiplierx4 % 4 == 1 ) --multiplierx4;
	fprintf( stderr, "Multiplierx2: %hhu\n", multiplierx4/2 );

	frequency = (((uint64_t)multiplierx4)*base_frequency)/4;
	fprintf( stderr, "Frequency: %llu\n", frequency );

done:
	return frequency;
}

#endif/*TSC_HPP*/
