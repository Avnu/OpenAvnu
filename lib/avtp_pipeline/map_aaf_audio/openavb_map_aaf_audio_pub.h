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
 * HEADER SUMMARY : AVTP Audio Format mapping module public interface
 *
 * AAF is defined in IEEE 1722a (still in draft as of Feb 2015).
 *
 * map_nv_tx_rate must be set in the .ini file.
 */

#ifndef OPENAVB_MAP_AVTP_AUDIO_PUB_H
#define OPENAVB_MAP_AVTP_AUDIO_PUB_H 1

// For now, use the same data format as the uncompressed audio mapping
// We will need to change the WAV and ALSA interface modules if we change this
//
#include "openavb_map_uncmp_audio_pub.h"
#define media_q_pub_map_aaf_audio_info_t	media_q_pub_map_uncmp_audio_info_t

// NOTE: A define is used for the MediaQDataFormat identifier because it is needed in separate execution units (static / dynamic libraries)
// that is why a single static (static/extern pattern) definition can not be used.
#define MapAVTPAudioMediaQDataFormat "AVTPAudioFormat"

#endif  // OPENAVB_MAP_AVTP_AUDIO_PUB_H
