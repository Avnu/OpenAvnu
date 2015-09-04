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
* HEADER SUMMARY : Motion JPEG mapping module public interface conforming to 1722A RTP payload encapsulation.
* 
* Refer to RFC 2435 for details of the fragment structure.
* 
* As defined 1722a the timestamp must be the same for each fragment of a JPEG frame. This means the interface module
* must set this same timestamp in the media queue for each item tht is a JPEG fragment of the frame. In addition the
* Interface module must set the lastFragment flag of the item public map data structure for each fragment placed into
* the media queue (TRUE if not the last fragment of the frame, FALSE if it is the last fragment of a frame).
* 
* The payload will be as defined in RFC 2435 and will include the JPEG headers as well as the JPEG data.
*/

#ifndef OPENAVB_MAP_MJPEG_PUB_H
#define OPENAVB_MAP_MJPEG_PUB_H 1

#include "openavb_types_pub.h"

/** \file
 * Motion JPEG mapping module public interface conforming to 1722A RTP payload
 * encapsulation.
 *
 * As defined 1722a the timestamp must be the same for each fragment of a JPEG
 * frame. This means the interface module must set this same timestamp in the
 * media queue for each item tht is a JPEG fragment of the frame.
 * In addition the Interface module must set the lastFragment flag of the item
 * public map data structure for each fragment placed into the media queue
 * (FALSE if not the last fragment of the frame, TRUE if it is the last
 * fragment of a frame).
 *
 *  The payload will be as defined in RFC 2435 and will include the JPEG header
 *  as well as the JPEG data.
 */

/** \note A define is used for the MediaQDataFormat identifier because it is
 * needed in separate execution units (static / dynamic libraries) that is why
 * a single static (static/extern pattern) definition can not be used.
 */
#define MapMjpegMediaQDataFormat "Mjpeg"

/// Additional public map data structure
typedef struct {
	/// Is this fragment the last one of this frame.
	bool lastFragment;
} media_q_item_map_mjpeg_pub_data_t;

#endif  // OPENAVB_MAP_MJPEG_PUB_H
