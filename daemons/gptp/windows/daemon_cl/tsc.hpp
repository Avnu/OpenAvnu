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

#ifndef TSC_HPP
#define TSC_HPP

#include <intrin.h>
#include <stdint.h>
#include <VersionHelpers.h>

#define WIN10_MAJOR 0x0A
#define WIN10_MINOR 0x00
#define WIN10_SVC   0x00

/**
 * @brief  Gets the processor timestamp
 * @return processor timestamp
 */
inline unsigned __int64 PLAT_rdtsc()
{
	return __rdtsc();
}

#define CLOCK_INFO_CPUID_LEAF	(0x15)
#define SYSTEM_CLOCK_24MHZ	(24000000)
#define MAX_MODEL_LIST_LEN	(2)

typedef struct
{
	short		model_list[MAX_MODEL_LIST_LEN+1];	// Terminate model list with -1
	uint32_t	clock_rate;				// clock_rate of 0 is invalid
}
ModelNumberSystemClockRateMapping;

static ModelNumberSystemClockRateMapping ModelNumberSystemClockRateMap[] =
{
	{{ 0x5E, 0x4E, -1 }, SYSTEM_CLOCK_24MHZ },
	{{ -1 }, 0 },
};

/**
* @brief  Gets the system clock frequency using CPU model number
* @param  model_query model number to look up
* @return System frequency, or 0 on error
*/
inline uint32_t FindFrequencyByModel(uint8_t model_query) {
	ModelNumberSystemClockRateMapping *map = ModelNumberSystemClockRateMap;
	uint32_t ret = 0;

	while (map->clock_rate != 0)
	{
		short *model = map->model_list;

		while (*model != -1)
		{
			if (*model == model_query) {
				ret = map->clock_rate;
				break;
			}
			++model;
		}
		++map;
	}

	return ret;
}


/**
 * @brief  Gets the TSC frequnecy
 * @param  millis time in miliseconds
 * @return TSC frequency, or 0 on error
 */
inline uint64_t getTSCFrequency( unsigned millis ) {
	int max_cpuid_level;
	int tmp[4];

	// Find the max cpuid level, and if possible find clock info
	__cpuid(tmp, 0);
	max_cpuid_level = tmp[0];
	if (max_cpuid_level >= CLOCK_INFO_CPUID_LEAF)
		__cpuid(tmp, CLOCK_INFO_CPUID_LEAF);

	// For pre-Win10 on 6th Generation or greater Intel CPUs the raw hardware
	// clock will be returned, *else* use QPC for everything else
	//
	// EAX (tmp[0]) must be >= 1, See Intel SDM 17.15.4 "Invariant Time-keeping"
	if (!IsWindowsVersionOrGreater(WIN10_MAJOR, WIN10_MINOR, WIN10_SVC) && 
		max_cpuid_level >= CLOCK_INFO_CPUID_LEAF &&
		tmp[0] >= 1) {
		SYSTEM_INFO info;

		GetSystemInfo(&info);

		// Look at processor model (upper byte of revision) number to determine clock rate
		return FindFrequencyByModel(info.wProcessorRevision >> 8);
	}

	LARGE_INTEGER freq;

	if (QueryPerformanceFrequency(&freq))
		return freq.QuadPart;

	return 0;
}

#endif/*TSC_HPP*/
