/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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

#ifndef OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
#define OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H 1

#include "openavb_acmp.h"
#include "openavb_descriptor_stream_io_pub.h"
#include "openavb_avdecc_msg.h"


// Run a single talker or listener.
bool openavbAVDECCRunListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo);
bool openavbAVDECCRunTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Stop a single talker or listener.
bool openavbAVDECCStopListener(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx, openavb_acmp_ListenerStreamInfo_t *pListenerStreamInfo);
bool openavbAVDECCStopTalker(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Get talker stream details. Structure members in TalkerStreamInfo will be filled.
bool openavbAVDECCGetTalkerStreamInfo(openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput, U16 configIdx, openavb_acmp_TalkerStreamInfo_t *pTalkerStreamInfo);

// Set talker stream details. The settings will be passed to the Talker to use for future connections
bool openavbAVDECCSetTalkerStreamInfo(
	openavb_aem_descriptor_stream_io_t *pDescriptorStreamOutput,
	U8 sr_class,
	U8 stream_id_valid, const U8 stream_src_mac[6], U16 stream_uid,
	U8 stream_dest_valid, const U8 stream_dest_mac[6],
	U8 stream_vlan_id_valid, U16 stream_vlan_id);

// Get the streaming state (stopped, running, paused, etc.) AVDECC told the talker or listener to use.
openavbAvdeccMsgStateType_t openavbAVDECCGetRequestedState(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx);

// Get the streaming state (stopped, running, paused, etc.) last reported by the talker or listener.
openavbAvdeccMsgStateType_t openavbAVDECCGetStreamingState(openavb_aem_descriptor_stream_io_t *pDescriptorStreamInput, U16 configIdx);

// Pause or resume the stream.
void openavbAVDECCPauseStream(openavb_aem_descriptor_stream_io_t *pDescriptor, bool bPause);

// Get the current counter value in pValue.  Returns TRUE if the counter is supported, FALSE otherwise.
bool openavbAVDECCGetCounterValue(void *pDescriptor, U16 descriptorType, U32 counterFlag, U32 *pValue);

#endif // OPENAVB_AVDECC_PIPELINE_INTERACTION_PUB_H
