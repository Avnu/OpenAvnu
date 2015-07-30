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
* HEADER SUMMARY : Mpeg2 TS mapping module public interface
* 
* Refer to IEC 61883-4 for details of the "source packet" structure.
* 
* This mapping module module as a talker requires an interface module to push
* one source packet of 192 octets into the media queue along with a media queue
* time value when the first data block of the source packet was obtained.
* There is no need to place the timestamp witin the source packet header, 
* this will be done within the mapping module were the max transit time will be
* added. The mapping module may bundle multiple source packets into one avtp packet
* before sending it.
* 
* This mapping module module as a listener will parse source packets from the
* avtp packet and place each source packet of 192 octets into the media queue along
* with the correct time stamp from the source packet header. The interface module
* will pull each source packet from the media queue and present has needed.
* 
* The protocol_specific_header, CIP header and the mpeg2 ts source packet header are
* all taken care of in the mapping module.
*/

#ifndef OPENAVB_MAP_MPEG2TS_PUB_H
#define OPENAVB_MAP_MPEG2TS_PUB_H 1

#include "openavb_types_pub.h"

// NOTE: A define is used for the MediaQDataFormat identifier because it is needed in separate execution units (static / dynamic libraries)
// that is why a single static (static/extern pattern) definition can not be used.
#define MapMpeg2tsMediaQDataFormat "Mpeg2ts"

#endif  // OPENAVB_MAP_MPEG2TS_PUB_H
