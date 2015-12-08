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
* MODULE SUMMARY : Talker implementation for use without endpoint 
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "openavb_tl.h"
#include "openavb_avtp.h"
#include "openavb_talker.h"
#include "openavb_qmgr.h"
#include "openavb_endpoint.h"

#define	AVB_LOG_COMPONENT	"Talker"
#include "openavb_log.h"
#include "openavb_trace.h"

// SR Class default Priority (PCP) values per IEEE 802.1Q-2011 Table 6-6
#define SR_CLASS_A_DEFAULT_PRIORITY 3
#define SR_CLASS_B_DEFAULT_PRIORITY 2

// SR Class default VLAN Id values per IEEE 802.1Q-2011 Table 9-2
#define SR_CLASS_A_DEFAULT_VID 2
#define SR_CLASS_B_DEFAULT_VID 2

// Returns TRUE, to say we're connected and registers tspec with FQTSS tspec should be initialized
bool openavbTLRunTalkerInit(tl_state_t *pTLState)
{
	openavb_tl_cfg_t *pCfg = &pTLState->cfg;
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	//avtp_stream_t *pStream = (avtp_stream_t *)(pTalkerData->avtpHandle);

	strncpy(pTalkerData->ifname, pCfg->ifname, IFNAMSIZ);
 
	// CORE_TODO: It would be good to have some parts of endpoint moved into non-endpoint general code to handle some the stream
	// configuration values.
	// strncpy(pTalkerData->ifname, pCfg->ifname, IFNAMSIZ);
	if (pCfg->stream_addr.mac) {
		memcpy(pTalkerData->streamID.addr, pCfg->stream_addr.mac, ETH_ALEN);
	}else {
		AVB_LOG_WARNING("Stream Address Not Set");
	}
		 
	pTalkerData->streamID.uniqueID = pCfg->stream_uid;
	if (pCfg->sr_class == SR_CLASS_A) {
		pTalkerData->classRate = 8000;
		pTalkerData->vlanID = pCfg->vlan_id == VLAN_NULL ?
					SR_CLASS_A_DEFAULT_VID : pCfg->vlan_id;
		pTalkerData->vlanPCP = SR_CLASS_A_DEFAULT_PRIORITY;
	}
	else if (pCfg->sr_class == SR_CLASS_B) {
		pTalkerData->classRate = 4000;
		pTalkerData->vlanID = pCfg->vlan_id == VLAN_NULL ?
					SR_CLASS_B_DEFAULT_VID : pCfg->vlan_id;
		pTalkerData->vlanPCP = SR_CLASS_B_DEFAULT_PRIORITY;
	}
	memcpy(&pTalkerData->destAddr, &pCfg->dest_addr.mac->ether_addr_octet, ETH_ALEN);

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
	pTalkerData->tSpec.maxFrameSize = pCfg->map_cb.map_max_data_size_cb(pTLState->pMediaQ);
	
	// TODO_COREAVB : This wakeRate should also be set in the endpoint case and removed from the tasker.c start stream
	if (!pCfg->map_cb.map_transmit_interval_cb(pTLState->pMediaQ)) {
		pTalkerData->wakeRate = pTalkerData->classRate / pCfg->batch_factor;
	}
	else {
		// Override the class observation interval with the one provided by the mapping module.
		pTalkerData->wakeRate = pCfg->map_cb.map_transmit_interval_cb(pTLState->pMediaQ) / pCfg->batch_factor;
	}

	if_info_t ifinfo;
	openavbCheckInterface(pTalkerData->ifname, &ifinfo);

	pTalkerData->fwmark = openavbQmgrAddStream((SRClassIdx_t)pCfg->sr_class,
					       pTalkerData->wakeRate,
					       pTalkerData->tSpec.maxIntervalFrames,
					       pTalkerData->tSpec.maxFrameSize);

	if (pTalkerData->fwmark == INVALID_FWMARK)
		return FALSE;
	
	AVB_LOGF_INFO("Dest Addr: "ETH_FORMAT, ETH_OCTETS(pTalkerData->destAddr));
	AVB_LOGF_INFO("Starting stream: "STREAMID_FORMAT, STREAMID_ARGS(&pTalkerData->streamID));
	talkerStartStream(pTLState);
	
	return TRUE;
}

void openavbTLRunTalkerFinish(tl_state_t *pTLState)
{
	talker_data_t *pTalkerData = pTLState->pPvtTalkerData;
	openavbQmgrRemoveStream(pTalkerData->fwmark);
}

void openavbEptClntNotifyTlkrOfSrpCb(
int                      endpointHandle,
AVBStreamID_t           *streamID,
char                    *ifname,
U8                       destAddr[],
openavbSrpLsnrDeclSubtype_t  lsnrDecl,
U32                      classRate,
U16                      vlanID,
U8                       priority,
U16                      fwmark)
{
}

