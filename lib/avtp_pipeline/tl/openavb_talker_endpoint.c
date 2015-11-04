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
* MODULE SUMMARY : Talker implementation for use with endpoint 
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_tl.h"
#include "openavb_endpoint.h"
#include "openavb_avtp.h"
#include "openavb_talker.h"
#include "openavb_time.h"

// DEBUG Uncomment to turn on logging for just this module.
//#define AVB_LOG_ON	1

#define	AVB_LOG_COMPONENT	"Talker"
#include "openavb_log.h"

/* Talker callback comes from endpoint, to indicate when listeners
 * come and go. We may need to start or stop the talker thread.
 */
void openavbEptClntNotifyTlkrOfSrpCb(int                      endpointHandle,
                                 AVBStreamID_t           *streamID,
                                 char                    *ifname,
                                 U8                       destAddr[],
                                 openavbSrpLsnrDeclSubtype_t  lsnrDecl,
                                 U32                      classRate,
                                 U16                      vlanID,
                                 U8                       priority,
                                 U16                      fwmark)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	tl_state_t *pTLState = TLHandleListGet(endpointHandle);
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;

	if (!pTLState) {
		AVB_LOG_WARNING("Unable to get talker from endpoint handle.");
		return;
	}

	AVB_LOGF_DEBUG("%s streaming=%d, lsnrDecl=%d", __FUNCTION__, pTLState->bStreaming, lsnrDecl);

	if (!pTLState->bStreaming) {
		if (lsnrDecl == openavbSrp_LDSt_Ready
			|| lsnrDecl == openavbSrp_LDSt_Ready_Failed) {

			// Save the data provided by endpoint/SRP
			strncpy(pTalkerData->ifname, ifname, IFNAMSIZ);
			memcpy(&pTalkerData->streamID, streamID, sizeof(AVBStreamID_t));
			memcpy(&pTalkerData->destAddr, destAddr, ETH_ALEN);
			pTalkerData->classRate = classRate;
			pTalkerData->vlanID = vlanID;
			pTalkerData->vlanPCP = priority;
			pTalkerData->fwmark = fwmark;

			// We should start streaming
			AVB_LOGF_INFO("Starting stream: "STREAMID_FORMAT, STREAMID_ARGS(streamID));
			talkerStartStream(pTLState);
		}
		else if (lsnrDecl == openavbSrp_LDSt_Stream_Info) {
			// Stream information is available does NOT mean listener is ready. Stream not started yet.
			strncpy(pTalkerData->ifname, ifname, IFNAMSIZ);
			memcpy(&pTalkerData->streamID, streamID, sizeof(AVBStreamID_t));
			memcpy(&pTalkerData->destAddr, destAddr, ETH_ALEN);
			pTalkerData->classRate = classRate;
			pTalkerData->vlanID = vlanID;
			pTalkerData->vlanPCP = priority;
			pTalkerData->fwmark = fwmark;
		}
	}
	else {
		if (lsnrDecl != openavbSrp_LDSt_Ready
			&& lsnrDecl != openavbSrp_LDSt_Ready_Failed) {
			// Nobody is listening any more
			AVB_LOGF_INFO("Stopping stream: "STREAMID_FORMAT, STREAMID_ARGS(streamID));
			talkerStopStream(pTLState);
		}
	}


	AVB_TRACE_EXIT(AVB_TRACE_TL);
}

bool openavbTLRunTalkerInit(tl_state_t *pTLState)
{
	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;

	AVBStreamID_t streamID;
	memset(&streamID, 0, sizeof(AVBStreamID_t));
	if (pCfg->stream_addr.mac)
		memcpy(streamID.addr, pCfg->stream_addr.mac, ETH_ALEN);
	streamID.uniqueID = pCfg->stream_uid;

	unsigned int maxBitrate = 0;
	if (pCfg->intf_cb.intf_get_src_bitrate_cb != NULL) {
		maxBitrate = pCfg->intf_cb.intf_get_src_bitrate_cb(pTLState->pMediaQ);
	}
	if (maxBitrate > 0) {
		if (pCfg->map_cb.map_set_src_bitrate_cb != NULL) {
			pCfg->map_cb.map_set_src_bitrate_cb(pTLState->pMediaQ, maxBitrate);
		}

		if (pCfg->map_cb.map_get_max_interval_frames_cb != NULL) {
			unsigned int map_intv_frames = pCfg->map_cb.map_get_max_interval_frames_cb(pTLState->pMediaQ, pTLState->cfg.sr_class);
			pCfg->max_interval_frames = map_intv_frames > 0 ? map_intv_frames : pCfg->max_interval_frames;
		}
	}
	pTalkerData->tSpec.maxIntervalFrames = pCfg->max_interval_frames;

	// The TSpec frame size is the L2 payload - i.e. no Ethernet headers, VLAN, FCS, etc...
	pTalkerData->tSpec.maxFrameSize = pCfg->map_cb.map_max_data_size_cb(pTLState->pMediaQ);

	AVB_LOGF_INFO("Register "STREAMID_FORMAT": class: %c frame size: %d  frame interval: %d", STREAMID_ARGS(&streamID), AVB_CLASS_LABEL(pCfg->sr_class), pTalkerData->tSpec.maxFrameSize, pTalkerData->tSpec.maxIntervalFrames);

	// Tell endpoint to register our stream.
	// SRP will send out talker declarations on the LAN.
	// If there are listeners, we'll get callback (above.)
	return (openavbEptClntRegisterStream(pTLState->endpointHandle,
	                                                &streamID,
	                                                pCfg->dest_addr.mac->ether_addr_octet,
	                                                &pTalkerData->tSpec,
	                                                pCfg->sr_class,
	                                                pCfg->sr_rank,
	                                                pCfg->internal_latency));
}

void openavbTLRunTalkerFinish(tl_state_t *pTLState)
{
}
