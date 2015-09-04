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
 * MODULE SUMMARY :
 *
 * Stream clients (talkers or listeners) must connect to the central
 * "endpoint" process to create a reservation for their traffic.
 *
 * This code implements the endpoint (server) side of the IPC.
 * 
 * It provides proxy functions for the endpoint to call.  The arguments
 * for those calls are packed into messages, which are unpacked in the
 * streamer processes and then used to call the real functions.
 *
 * Current IPC uses unix sockets.  Can change this by creating a new
 * implementations in openavb_endoint_client.c and openavb_endpoint_server.c
 */

#include <stdlib.h>
#include <string.h>

#include "openavb_endpoint.h"
#include "openavb_trace.h"

//#define AVB_LOG_LEVEL  AVB_LOG_LEVEL_DEBUG
#define	AVB_LOG_COMPONENT	"Endpoint Server"
#include "openavb_pub.h"
#include "openavb_log.h"
#include "openavb_qmgr.h"  // for INVALID_FWMARK
#include "openavb_maap.h"

// forward declarations
static bool openavbEptSrvrReceiveFromClient(int h, openavbEndpointMessage_t *msg);

#include "openavb_endpoint_server_osal.c"

// the following are from openavb_endpoint.c
extern openavb_endpoint_cfg_t  x_cfg;
extern clientStream_t*         x_streamList;


static bool openavbEptSrvrReceiveFromClient(int h, openavbEndpointMessage_t *msg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (!msg) {
		AVB_LOG_ERROR("Receiving message; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}
			
	bool ret = FALSE;
	switch (msg->type) {
		case OPENAVB_ENDPOINT_TALKER_REGISTER:
			AVB_LOGF_DEBUG("TalkerRegister from client uid=%d", msg->streamID.uniqueID);
			ret = openavbEptSrvrRegisterStream(h, &msg->streamID,
			                               msg->params.talkerRegister.destAddr,
			                               &msg->params.talkerRegister.tSpec,
			                               msg->params.talkerRegister.srClass,
			                               msg->params.talkerRegister.srRank,
			                               msg->params.talkerRegister.latency);
			break;
		case OPENAVB_ENDPOINT_LISTENER_ATTACH:
			AVB_LOGF_DEBUG("ListenerAttach from client uid=%d", msg->streamID.uniqueID);
			ret = openavbEptSrvrAttachStream(h, &msg->streamID,
			                             msg->params.listenerAttach.lsnrDecl);
			break;
		case OPENAVB_ENDPOINT_CLIENT_STOP:
			AVB_LOGF_DEBUG("Stop from client uid=%d", msg->streamID.uniqueID);
			ret = openavbEptSrvrStopStream(h, &msg->streamID);
			break;
		case OPENAVB_ENDPOINT_VERSION_REQUEST:
			AVB_LOG_DEBUG("Version request from client");
			ret = openavbEptSrvrHndlVerRqstFromClient(h);
			break;
		default:
			AVB_LOG_ERROR("Unexpected message received at server");
			break;
	}

	AVB_LOGF_VERBOSE("Message handled, ret=%d", ret);
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ret;
}

void openavbEptSrvrNotifyTlkrOfSrpCb(int h,
                                  AVBStreamID_t           *streamID,
                                  char                    *ifname,
                                  U8                       destAddr[ETH_ALEN],
                                  openavbSrpLsnrDeclSubtype_t  lsnrDecl,
                                  U32                      classRate,
                                  U16                      vlanID,
                                  U8                       priority,
                                  U16                      fwmark) 
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t  msgBuf;

	if (!streamID || !ifname || !destAddr) {
		AVB_LOG_ERROR("Talker callback; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return;
	}

	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_TALKER_CALLBACK;
	memcpy(&(msgBuf.streamID), streamID, sizeof(AVBStreamID_t));
	strncpy(msgBuf.params.talkerCallback.ifname, ifname, IFNAMSIZ - 1);
	memcpy(msgBuf.params.talkerCallback.destAddr, destAddr, ETH_ALEN);
	msgBuf.params.talkerCallback.lsnrDecl = lsnrDecl;
	msgBuf.params.talkerCallback.classRate = classRate;
	msgBuf.params.talkerCallback.vlanID = vlanID;
	msgBuf.params.talkerCallback.priority = priority;
	msgBuf.params.talkerCallback.fwmark = fwmark;
	openavbEptSrvrSendToClient(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

void openavbEptSrvrNotifyLstnrOfSrpCb(int h,
								 AVBStreamID_t	*streamID,
								 char			*ifname,
								 U8 			destAddr[],
								 openavbSrpAttribType_t tlkrDecl,
								 AVBTSpec_t		*tSpec,
								 U8				srClass,
								 U32			latency,
								 openavbSrpFailInfo_t* failInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	openavbEndpointMessage_t  msgBuf;

	// Check for valid parameters. DestAddr is optional and checked later.
	if (!streamID || !ifname) {		
		AVB_LOG_ERROR("Listener callback; invalid argument passed");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return;
	}

	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_LISTENER_CALLBACK;
	memcpy(&(msgBuf.streamID), streamID, sizeof(AVBStreamID_t));
	strncpy(msgBuf.params.listenerCallback.ifname, ifname, IFNAMSIZ - 1);
	if (destAddr) 
		memcpy(msgBuf.params.listenerCallback.destAddr, destAddr, ETH_ALEN);
	msgBuf.params.listenerCallback.tlkrDecl = tlkrDecl;
	if (tSpec)
		memcpy(&msgBuf.params.listenerCallback.tSpec, tSpec, sizeof(AVBTSpec_t));
	msgBuf.params.listenerCallback.srClass = srClass;
	msgBuf.params.listenerCallback.latency = latency;
	if (failInfo)
		memcpy(&msgBuf.params.listenerCallback.failInfo, failInfo, sizeof(openavbSrpFailInfo_t));
	openavbEptSrvrSendToClient(h, &msgBuf);
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

void openavbEptSrvrSendServerVersionToClient(int h, U32 AVBVersion)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	openavbEndpointMessage_t  msgBuf;
	memset(&msgBuf, 0, OPENAVB_ENDPOINT_MSG_LEN);
	msgBuf.type = OPENAVB_ENDPOINT_VERSION_CALLBACK;
	msgBuf.params.versionCallback.AVBVersion = AVBVersion;
	openavbEptSrvrSendToClient(h, &msgBuf);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

/* Talker client registers a stream
 */
bool openavbEptSrvrRegisterStream(int h,
                              AVBStreamID_t *streamID,
                              U8 destAddr[],
                              AVBTSpec_t *tSpec,
                              U8 srClass,
                              U8 srRank,
                              U32 latency)
{
	openavbRC rc = OPENAVB_SUCCESS;

	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	clientStream_t *ps = findStream(streamID);
	
	if (ps && ps->clientHandle != h) {
		AVB_LOGF_ERROR("Error registering talker; multiple clients for stream %d", streamID->uniqueID);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	ps = addStream(h, streamID);
	if (!ps) {
		AVB_LOGF_ERROR("Error registering talker; unable to add client stream %d", streamID->uniqueID);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}
	ps->role = clientTalker;
	ps->tSpec = *tSpec;
	ps->srClass = (SRClassIdx_t)srClass;
	ps->srRank  = srRank;
	ps->latency = latency;
	ps->fwmark = INVALID_FWMARK;

	if (memcmp(ps->destAddr, destAddr, ETH_ALEN) == 0) {
		// no client-supplied address, use MAAP
		struct ether_addr addr;
		ps->hndMaap = openavbMaapAllocate(1, &addr);
		if (ps->hndMaap) {
			memcpy(ps->destAddr, addr.ether_addr_octet, ETH_ALEN);
			strmAttachCb((void*)ps, openavbSrp_LDSt_Stream_Info);		// Inform talker about MAAP
		}
		else {
			AVB_LOG_ERROR("Error registering talker: MAAP failed to allocate MAC address");
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			delStream(ps);
			return FALSE;
		}
	}
	else {
		// client-supplied destination MAC address
		memcpy(ps->destAddr, destAddr, ETH_ALEN);
		ps->hndMaap = NULL;
	}

	// Do SRP talker register
	AVB_LOGF_DEBUG("REGISTER: ps=%p, streamID=%d, tspec=%d,%d, srClass=%d, srRank=%d, latency=%d, da="ETH_FORMAT"",
				   ps, streamID->uniqueID,
				   tSpec->maxFrameSize, tSpec->maxIntervalFrames,
				   ps->srClass, ps->srRank, ps->latency,
				   ETH_OCTETS(ps->destAddr));


	if(x_cfg.noSrp) {
		// we are operating in a mode supporting preconfigured streams; SRP is not in use,
		// so, as a proxy for SRP, which would normally make this call after establishing
		// the stream, call the callback from here
		strmAttachCb((void*)ps, openavbSrp_LDSt_Ready);
	} else {
		// normal SRP operation
		rc = openavbSrpRegisterStream((void*)ps, &ps->streamID,
		                          ps->destAddr, &ps->tSpec,
		                          ps->srClass, ps->srRank,
		                          ps->latency);
		if (!IS_OPENAVB_SUCCESS(rc)) {
			if (ps->hndMaap)
				openavbMaapRelease(ps->hndMaap);
			delStream(ps);
		}
	}

	openavbEndPtLogAllStaticStreams();
	
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return IS_OPENAVB_SUCCESS(rc);
}

/* Listener client attaches to a stream
 */
bool openavbEptSrvrAttachStream(int h,
                            AVBStreamID_t *streamID,
                            openavbSrpLsnrDeclSubtype_t ld)
{
	openavbRC rc = OPENAVB_SUCCESS;
	static U8 emptyMAC[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
	static AVBTSpec_t emptytSpec = {0, 0};

	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	clientStream_t *ps = findStream(streamID);
	if (ps && ps->clientHandle != h) {
		AVB_LOGF_ERROR("Error attaching listener: multiple clients for stream %d", streamID->uniqueID);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	if (!ps) {
		ps = addStream(h, streamID);
		if (!ps) {
			AVB_LOGF_ERROR("Error attaching listener: unable to add client stream %d", streamID->uniqueID);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return FALSE;
		}
		ps->role = clientListener;
	}

	if(x_cfg.noSrp) {
		// we are operating in a mode supporting preconfigured streams; SRP is not in use,
		if(ld == openavbSrp_LDSt_Interest) {
			// As a proxy for SRP, which would normally make this call after confirming
			// availability of the stream, call the callback from here
			strmRegCb((void*)ps, openavbSrp_AtTyp_TalkerAdvertise,
					  emptyMAC, // a flag to listener to read info from configuration file
					  &emptytSpec,
					  MAX_AVB_SR_CLASSES, // srClass - value doesn't matter because openavbEptSrvrNotifyLstnrOfSrpCb() throws it away
					  1, // accumLatency
					  NULL); // *failInfo
		}
	} else {
		// Normal SRP Operation so pass to SRP
		rc = openavbSrpAttachStream((void*)ps, streamID, ld);
		if (!IS_OPENAVB_SUCCESS(rc))
			delStream(ps);
	}

	openavbEndPtLogAllStaticStreams();

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return IS_OPENAVB_SUCCESS(rc);
}

/* Client (talker or listener) going away
 */
bool openavbEptSrvrStopStream(int h, AVBStreamID_t *streamID)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	clientStream_t *ps = findStream(streamID);
	if (!ps || ps->clientHandle != h) {
		AVB_LOGF_ERROR("Error stopping client: missing record for stream %d", streamID->uniqueID);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return FALSE;
	}

	bool rc = FALSE;
	if (ps->role == clientTalker)
		rc = x_talkerDeregister(ps);
	else if (ps->role == clientListener)
		rc = x_listenerDetach(ps);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return rc;
}

/* Client version request
 */
bool openavbEptSrvrHndlVerRqstFromClient(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	openavbEptSrvrSendServerVersionToClient(h, AVB_CORE_VER_FULL);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return TRUE;
}

/* Called if a client closes their end of the IPC 
 */
void openavbEptSrvrCloseClientConnection(int h)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	clientStream_t **lpp;
	for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
		if ((*lpp)->clientHandle == h)
		{
			if ((*lpp)->role == clientTalker)
				x_talkerDeregister((*lpp));
			else if ((*lpp)->role == clientListener)
				x_listenerDetach((*lpp));
			break;
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}

