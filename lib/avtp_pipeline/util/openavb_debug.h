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
* MODULE SUMMARY : Implementation of debugging aid functions.
*/

#ifndef OPENAVB_DEBUG_H
#define OPENAVB_DEBUG_H 1

#include "openavb_types.h"

#define DBG_VARx(x, y) x ## y
#define DBG_VAR(x, y) DBG_VARx(x, y)

// Dump the buffer to the console
void openavbDebugDumpBuf(U8 *pBuf, U32 len);

// Interval Testing
#define AVB_DBG_INTERVAL(interval, log)					\
	static U32 DBG_VAR(maxInterval,__LINE__) = 0; 		\
	static U32 DBG_VAR(minInterval,__LINE__) = (U32)-1; \
	static U32 DBG_VAR(cntInterval,__LINE__) = 0;		\
	static U32 DBG_VAR(accInterval,__LINE__) = 0;		\
	static U64 DBG_VAR(prevNS,__LINE__) = 0; 			\
	openavbDebugInterval(interval, log, &DBG_VAR(maxInterval,__LINE__), &DBG_VAR(minInterval,__LINE__), &DBG_VAR(cntInterval,__LINE__), &DBG_VAR(accInterval,__LINE__), &DBG_VAR(prevNS,__LINE__))
void openavbDebugInterval(U32 interval, bool log, U32 *maxInterval, U32 *minInterval, U32 *cntInterval, U32 *accInterval, U64 *prevNS);


#endif // OPENAVB_DEBUG_H
