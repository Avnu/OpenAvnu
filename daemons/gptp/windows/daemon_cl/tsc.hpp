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

#define CPUFREQ_BASE 133

inline unsigned __int64 PLAT_rdtsc()
{
  return __rdtsc();
}

// 'millis' argument specifies time to measure TSC over.  A longer time is generally more reliable
// Returns TSC frequency
inline uint64_t getTSCFrequency( unsigned millis ) {
	uint64_t tsc1, tsc2, multiplier;
	unsigned msig, lsig;

	tsc1 = PLAT_rdtsc();
	Sleep( millis );
	tsc2 = PLAT_rdtsc();
	multiplier = (unsigned) (tsc2 - tsc1)/133;
	lsig = multiplier % 1000000;
	msig = (unsigned) multiplier / 1000000;
	if( lsig >= 750000 ) multiplier = (msig+1)*1000000;
	else if( lsig < 250000 ) multiplier = msig*1000000;
	else multiplier = msig*1000000 + 500000;
	return multiplier*CPUFREQ_BASE;
}

#endif/*TSC_HPP*/
