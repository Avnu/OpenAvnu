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


#ifndef IPCDEF_HPP
#define IPCDEF_HPP

#include <stdint.h>
#include <ptptypes.hpp>

#define PIPE_PREFIX "\\\\.\\pipe\\"
#define P802_1AS_PIPENAME "gptp-update"
#define OUTSTANDING_MESSAGES 10

#pragma pack(push,1)

class WindowsNPipeMessage {
private:
	int64_t ml_phoffset;
	int64_t ls_phoffset;
	FrequencyRatio ml_freqoffset;
	FrequencyRatio ls_freq_offset;
	uint64_t local_time;
public:
	WindowsNPipeMessage() {};
	WindowsNPipeMessage( int64_t ml_phoffset, int64_t ls_phoffset, FrequencyRatio ml_freqoffset, FrequencyRatio ls_freq_offset, uint64_t local_time ) {
		this->ml_phoffset = ml_phoffset;
		this->ls_phoffset = ls_phoffset;
		this->ml_freqoffset = ml_freqoffset;
		this->ls_freq_offset = ls_freq_offset;
		this->local_time = local_time;

	}
	bool write( HANDLE pipe ) {
		DWORD bytes_written;
		DWORD last_error = ERROR_SUCCESS;
		if( WriteFile( pipe, this, sizeof(*this), &bytes_written, NULL ) == 0 ) {
			last_error = GetLastError();
		}
		if( last_error == ERROR_SUCCESS || last_error == ERROR_PIPE_LISTENING ) {
			return true;
		}
		return false;
	}
	bool read( HANDLE pipe ) {
		DWORD bytes_written;
		if( ReadFile( pipe, this, sizeof(*this), &bytes_written, NULL ) == 0 ) return false;
		return true;
	}
	int64_t getMasterLocalOffset() { return ml_phoffset; }
	FrequencyRatio getMasterLocalFreqOffset() { return ml_freqoffset; }
	int64_t getLocalSystemOffset() { return ls_phoffset; }
	FrequencyRatio getLocalSystemFreqOffset() { return ls_freq_offset; }
	uint64_t getLocalTime() { return local_time; }
};

#pragma pack(pop)

#endif
