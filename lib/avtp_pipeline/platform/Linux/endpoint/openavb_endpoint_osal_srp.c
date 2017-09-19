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

#include <stdlib.h>
#include <string.h>

#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_endpoint.h"
#include "openavb_srp.h"
#include "mrp_client.h"
#include "openavb_list.h"

#define	AVB_LOG_COMPONENT	"Endpoint SRP"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_pub.h"
#include "openavb_log.h"


/*******************************************************************************
 * SRP proxies
 ******************************************************************************/

static strmAttachCb_t _attachCb = NULL;
static strmRegCb_t _registerCb = NULL;

typedef struct {
	void* avtpHandle;
	bool talker;
	U8 streamId[8];
	U8 destAddr[6];
	U16 vlanId;
	int maxFrameSize;
	int maxIntervalFrames;
	int priority;
	int latency;
	int subtype;
} strElem_t;

openavb_list_t strElemList;

#define SID_FORMAT "%02x:%02x:%02x:%02x:%02x:%02x/%u"
#define SID_OCTETS(a) (a)[0],(a)[1],(a)[2],(a)[3],(a)[4],(a)[5],(a)[6]<<8|(a)[7]

// Callback for SRP to notify AVTP Talker that a Listener Declaration has been
// registered (or de-registered)
void mrp_attach_cb(unsigned char streamid[8], int subtype)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);

	AVB_LOGF_DEBUG("mrp_attach_cb "SID_FORMAT" subtype %d", SID_OCTETS(streamid), subtype);

	if (_attachCb) {
		openavb_list_node_t node = openavbListIterFirst(strElemList);
		while (node) {
			strElem_t *elem = openavbListData(node);
			if (elem && elem->talker && memcmp(streamid, elem->streamId, sizeof(elem->streamId)) == 0) {
				_attachCb(elem->avtpHandle, subtype);
				elem->subtype = subtype;
				AVB_LOGF_DEBUG("mrp_attach_cb subtype changed to %d", elem->subtype);
				break;
			}
			else if (elem) {
				AVB_LOGF_DEBUG("mrp_attach_cb skipping elem " SID_FORMAT ", talker %d",
					SID_OCTETS(elem->streamId), elem->talker ? 1 : 0);
			}
			node = openavbListIterNext(strElemList);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
}

// Callback for SRP to notify AVTP Listener that a Talker Declaration has been
// registered (or de-registered)
void mrp_register_cb(unsigned char streamid[8], int join, unsigned char destaddr[6], unsigned int max_frame_size, unsigned int max_interval_frames, uint16_t vid, unsigned int latency)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);

	AVB_LOGF_DEBUG("mrp_register_cb "SID_FORMAT" "ETH_FORMAT" join %d max_frame_size %d max_interval_frames %d vid %d latency %d",
	             SID_OCTETS(streamid), ETH_OCTETS(destaddr), join, max_frame_size, max_interval_frames, vid, latency);

	if (_registerCb) {
		openavb_list_node_t node = openavbListIterFirst(strElemList);
		while (node) {
			strElem_t *elem = openavbListData(node);
			if (elem && !elem->talker && memcmp(streamid, elem->streamId, sizeof(elem->streamId)) == 0) {
				AVBTSpec_t tSpec;
				tSpec.maxFrameSize = max_frame_size;
				tSpec.maxIntervalFrames = max_interval_frames;
				_registerCb(elem->avtpHandle,
						join ? openavbSrp_AtTyp_TalkerAdvertise : openavbSrp_AtTyp_None,
						destaddr,
						&tSpec,
						0, // SR_CLASS is ignored anyway
						latency,
						NULL
						);
				elem->subtype = join;
				AVB_LOGF_DEBUG("mrp_register_cb subtype changed to %d", elem->subtype);
				break;
			}
			else if (elem) {
				AVB_LOGF_DEBUG("mrp_register_cb skipping elem " SID_FORMAT ", talker %d",
					SID_OCTETS(elem->streamId), elem->talker ? 1 : 0);
			}
			node = openavbListIterNext(strElemList);
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
}

openavbRC openavbSrpInitialize(strmAttachCb_t attachCb, strmRegCb_t registerCb,
                               char* ifname, U32 TxRateKbps, bool bypassAsCapableCheck)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);
	int err;

	strElemList = openavbListNewList();

	_attachCb = attachCb;
	_registerCb = registerCb;


	err = mrp_connect();
	if (err) {
		AVB_LOGF_ERROR("mrp_connect failed: %s", strerror(errno));
		goto error;
	}
	err = mrp_monitor();
	if (err) {
		AVB_LOGF_ERROR("failed creating MRP monitor thread: %s", strerror(errno));
		goto error;
	}

	int class_a_id, a_priority;
	u_int16_t  a_vid;
	int class_b_id, b_priority;
	u_int16_t b_vid;

	err = mrp_get_domain(&class_a_id,&a_priority, &a_vid, &class_b_id, &b_priority, &b_vid);
	if (err) {
		AVB_LOG_DEBUG("mrp_get_domain failed");
		goto error;
	}

	AVB_LOGF_INFO("detected domain Class A PRIO=%d VID=%04x...", a_priority, (int)a_vid);
	AVB_LOGF_INFO("detected domain Class B PRIO=%d VID=%04x...", b_priority, (int)b_vid);

	err = mrp_register_domain(&domain_class_a_id, &domain_class_a_priority, &domain_class_a_vid);
	if (err) {
		AVB_LOG_DEBUG("mrp_register_domain failed");
		goto error;
	}

	err = mrp_register_domain(&domain_class_b_id, &domain_class_b_priority, &domain_class_b_vid);
	if (err) {
		AVB_LOG_DEBUG("mrp_register_domain failed");
		goto error;
	}

	err = mrp_join_vlan();
	if (err) {
		AVB_LOG_DEBUG("mrp_join_vlan failed");
		goto error;
	}

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;

error:
	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_FAILURE;
}

void openavbSrpShutdown(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);
	int err = mrp_disconnect();
	if (err) {
		AVB_LOGF_ERROR("mrp_disconnect failed: %s", strerror(errno));
	}
	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
}

openavbRC openavbSrpRegisterStream(void* avtpHandle,
                                   AVBStreamID_t* _streamId,
                                   U8 DA[],
                                   AVBTSpec_t* tSpec,
                                   SRClassIdx_t SRClassIdx,
                                   bool Rank,
                                   U32 Latency)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);
	int err;

	// convert streamId to 8 byte format
	uint8_t streamId[8];
	memcpy(streamId, _streamId->addr, sizeof(_streamId->addr));
	streamId[6] = _streamId->uniqueID >> 8;
	streamId[7] = _streamId->uniqueID & 0xFF;

	openavb_list_node_t node = openavbListNew(strElemList, sizeof(strElem_t));
	strElem_t* elem = openavbListData(node);

	elem->avtpHandle = avtpHandle;
	elem->talker = true;
	memcpy(elem->streamId, streamId, sizeof(elem->streamId));
	memcpy(elem->destAddr, DA, sizeof(elem->destAddr));
	elem->maxFrameSize = tSpec->maxFrameSize;
	elem->maxIntervalFrames = tSpec->maxIntervalFrames;
	elem->latency = Latency;
	elem->subtype = openavbSrp_LDSt_None;

	switch (SRClassIdx) {
	case SR_CLASS_A:
		elem->vlanId = domain_class_a_vid;
		elem->priority = domain_class_a_priority;

		err = mrp_advertise_stream(streamId,
		                     DA,
		                     domain_class_a_vid,
		                     tSpec->maxFrameSize,
		                     tSpec->maxIntervalFrames,
		                     domain_class_a_priority,
		                     Latency);
		break;
	case SR_CLASS_B:
		elem->vlanId = domain_class_b_vid;
		elem->priority = domain_class_b_priority;

		err = mrp_advertise_stream(streamId,
		                     DA,
		                     domain_class_b_vid,
		                     tSpec->maxFrameSize,
		                     tSpec->maxIntervalFrames,
		                     domain_class_b_priority,
		                     Latency);
		break;
	default:
		AVB_LOGF_ERROR("unknown SRClassIdx %d", (int)SRClassIdx);
		goto error;
	}

	if (err) {
		AVB_LOG_ERROR("mrp_advertise_stream failed");
		goto error;
	}

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;

error:
	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_FAILURE;
}

openavbRC openavbSrpDeregisterStream(AVBStreamID_t* _streamId)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);

	// convert streamId to 8 byte format
	uint8_t streamId[8];
	memcpy(streamId, _streamId->addr, sizeof(_streamId->addr));
	streamId[6] = _streamId->uniqueID >> 8;
	streamId[7] = _streamId->uniqueID & 0xFF;

	openavb_list_node_t node = openavbListIterFirst(strElemList);
	while (node) {
		strElem_t *elem = openavbListData(node);
		if (elem && memcmp(streamId, elem->streamId, sizeof(elem->streamId)) == 0) {
			int err = mrp_unadvertise_stream(elem->streamId, elem->destAddr, elem->vlanId, elem->maxFrameSize, elem->maxIntervalFrames, elem->priority, elem->latency);
			if (err) {
				AVB_LOG_ERROR("mrp_unadvertise_stream failed");
			}
			openavbListDelete(strElemList, node);
			break;
		}
		node = openavbListIterNext(strElemList);
	}
	if (!node)
		AVB_LOGF_ERROR("%s: unknown stream "SID_FORMAT, __func__, SID_OCTETS(streamId));
	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;
}

openavbRC openavbSrpAttachStream(void* avtpHandle,
                                 AVBStreamID_t* _streamId,
                                 openavbSrpLsnrDeclSubtype_t type)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);

	// convert streamId to 8 byte format
	uint8_t streamId[8];
	memcpy(streamId, _streamId->addr, sizeof(_streamId->addr));
	streamId[6] = _streamId->uniqueID >> 8;
	streamId[7] = _streamId->uniqueID & 0xFF;

	AVB_LOGF_DEBUG("openavbSrpAttachStream "SID_FORMAT, SID_OCTETS(streamId));

	openavb_list_node_t node = openavbListIterFirst(strElemList);
	// lets check if this streamId is on our list
	while (node) {
		strElem_t *elem = openavbListData(node);
		if (elem && memcmp(streamId, elem->streamId, sizeof(elem->streamId)) == 0) {
			break;
		}
		node = openavbListIterNext(strElemList);
	}
	if (!node) {
		// not found so add it
		node = openavbListNew(strElemList, sizeof(strElem_t));
		strElem_t* elem = openavbListData(node);

		elem->avtpHandle = avtpHandle;
		elem->talker = false;
		memcpy(elem->streamId, streamId, sizeof(elem->streamId));
		elem->subtype = type;
	}

	int err = mrp_send_ready(streamId);
	if (err) {
		AVB_LOG_ERROR("mrp_send_ready failed");
	}

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;
}

openavbRC openavbSrpDetachStream(AVBStreamID_t* _streamId)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);

	// convert streamId to 8 byte format
	uint8_t streamId[8];
	memcpy(streamId, _streamId->addr, sizeof(_streamId->addr));
	streamId[6] = _streamId->uniqueID >> 8;
	streamId[7] = _streamId->uniqueID & 0xFF;

	AVB_LOGF_DEBUG("openavbSrpDetachStream "SID_FORMAT, SID_OCTETS(streamId));

	openavb_list_node_t node = openavbListIterFirst(strElemList);
	while (node) {
		strElem_t *elem = openavbListData(node);
		if (elem && memcmp(streamId, elem->streamId, sizeof(elem->streamId)) == 0) {
			int err = mrp_send_leave(streamId);
			if (err) {
				AVB_LOG_ERROR("mrp_send_leave failed");
			}
			openavbListDelete(strElemList, node);
			break;
		}
		node = openavbListIterNext(strElemList);
	}
	if (!node)
		AVB_LOGF_ERROR("%s: unknown stream "SID_FORMAT, __func__, SID_OCTETS(streamId));

	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;
}

openavbRC openavbSrpGetClassParams(SRClassIdx_t SRClassIdx, U8* priority, U16* vid, U32* inverseIntervalSec)
{
	AVB_TRACE_ENTRY(AVB_TRACE_SRP_PUBLIC);
	switch (SRClassIdx) {
	case SR_CLASS_A:
		*priority = domain_class_a_priority;
		*vid = domain_class_a_vid;
		*inverseIntervalSec = 8000;
		break;
	case SR_CLASS_B:
		*priority = domain_class_b_priority;
		*vid = domain_class_b_vid;
		*inverseIntervalSec = 4000;
		break;
	default:
		AVB_LOGF_ERROR("unknown SRClassIdx %d", (int)SRClassIdx);
		AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
		return OPENAVB_SRP_FAILURE;
	}
	AVB_TRACE_EXIT(AVB_TRACE_SRP_PUBLIC);
	return OPENAVB_SRP_SUCCESS;
}
