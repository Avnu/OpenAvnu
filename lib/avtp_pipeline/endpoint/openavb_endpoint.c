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
* MODULE SUMMARY : The AVB endpoint is the "master process".
* It includes the SRP and QMgr libraries which handle SRP reservations
* and TX queuing.
*
* The actual AVTP talker/listener work is done in a separate processs.
* The aim of using separate processes is to (1) reduce the
* complexity in the central process; and (2) to allow multiple types
* of children for different AVTP encapsulations and data sources.
*
* Streamer processes contact the endpoint process through an IPC to
* declare their streams.  Currently, the IPC uses unix sockets.  The
* IPC methods are implemented in openavb_endpoint_client.c and
* openavb_endpoint_server.c.  The streamers (talkers and listeners)
* are referred to as our"clients", and the endpoint process is their
* "server".
*
* When SRP establishes a reservation (or when a reservation goes
* away), the endpoint communicates the event back to the streamer
* process through a callback (which also uses the IPC mechanism.)
*/

#include <stdlib.h>
#include <string.h>
#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_endpoint.h"
#include "openavb_endpoint_cfg.h"
#include "openavb_avtp.h"
#include "openavb_qmgr.h"
#include "openavb_maap.h"

#define	AVB_LOG_COMPONENT	"Endpoint"
#include "openavb_pub.h"
#include "openavb_log.h"

/* Information that we need to remember about each stream
 */


// list of streams that we're managing
clientStream_t* 				x_streamList;
// true until we are signalled to stop
bool endpointRunning = TRUE;
// data from our configuation file
openavb_endpoint_cfg_t 	x_cfg;

/*************************************************************
 * Functions to manage our list of streams.
 */

/* Log information on all statically configured streams.
 * (Dynamically configured streams are logged by SRP.)
*/
void openavbEndPtLogAllStaticStreams(void)
{
	bool hdrDone = FALSE;

	if(x_cfg.noSrp) {
		clientStream_t **lpp;
		for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
			if ((*lpp)->clientHandle != AVB_ENDPOINT_HANDLE_INVALID) {
				if (!hdrDone) {
					AVB_LOG_INFO("Statically Configured Streams:");
					AVB_LOG_INFO("                                     |   SR  |    Destination    | ----Max Frame(s)--- |");
					AVB_LOG_INFO("   Role   |        Stream Id         | Class |      Address      | Size | Per Interval |");
					hdrDone = TRUE;
				}
				if ((*lpp)->role == clientTalker) {
					AVB_LOGF_INFO("  Talker  | %02x:%02x:%02x:%02x:%02x:%02x - %4d |   %c   | %02x:%02x:%02x:%02x:%02x:%02x | %4u |      %2u      |",
								  (*lpp)->streamID.addr[0], (*lpp)->streamID.addr[1], (*lpp)->streamID.addr[2],
								  (*lpp)->streamID.addr[3], (*lpp)->streamID.addr[4], (*lpp)->streamID.addr[5],
								  (*lpp)->streamID.uniqueID,
								  AVB_CLASS_LABEL((*lpp)->srClass),
								  (*lpp)->destAddr[0], (*lpp)->destAddr[1], (*lpp)->destAddr[2],
								  (*lpp)->destAddr[3], (*lpp)->destAddr[4], (*lpp)->destAddr[5],
								  (*lpp)->tSpec.maxFrameSize, (*lpp)->tSpec.maxIntervalFrames );
				} else if ((*lpp)->role == clientListener) {
					AVB_LOGF_INFO(" Listener | %02x:%02x:%02x:%02x:%02x:%02x - %4d |   -   | --:--:--:--:--:-- |  --  |      --      |",
								  (*lpp)->streamID.addr[0], (*lpp)->streamID.addr[1], (*lpp)->streamID.addr[2],
								  (*lpp)->streamID.addr[3], (*lpp)->streamID.addr[4], (*lpp)->streamID.addr[5],
								  (*lpp)->streamID.uniqueID );

				}
			}
		}
	}
}

/* Called for each talker or listener stream declared by clients
 */
clientStream_t* addStream(int h, AVBStreamID_t *streamID)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	clientStream_t *newClientStream = NULL, **lpp;

	do {
		newClientStream = (clientStream_t *)calloc(1, sizeof(clientStream_t));
		if(newClientStream == NULL) {
			AVB_LOG_ERROR("addStream: Failed to malloc stream");
			break;
		}

		memcpy(newClientStream->streamID.addr, streamID->addr, ETH_ALEN);
		newClientStream->streamID.uniqueID = streamID->uniqueID;
		newClientStream->clientHandle = h;
		newClientStream->fwmark = INVALID_FWMARK;
	
		if(x_streamList == NULL) {
			x_streamList = newClientStream;
		}else {
			// insert at end
			for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
				if ((*lpp)->next == NULL) {
					(*lpp)->next = newClientStream;
					break;
				}
			}
		}
	} while (0);
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return newClientStream;
}

void delStream(clientStream_t* ps)
{
	clientStream_t **lpp;
	for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
		if((*lpp) == ps) {
			*lpp = (*lpp)->next;
			free(ps);
			break;
		}
	}
}

/* Find a stream in the list of streams we're handling
 */
clientStream_t* findStream(AVBStreamID_t *streamID)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	clientStream_t* ps = NULL;

	// Check for default stream MAC address, and fill it in with
	// interface MAC so that if the talker didn't fill in
	// the stream MAC, we use the one that the endpoint is
	// configured to use.
	//
	// Listener should never pass a default MAC in,
	// because the config code forces the listener to specify MAC in
	// its configuration.  Talker may send a default (empty) MAC in
	// the stream ID, in which case we fill it in.
	//
	// TODO: This is sketchy - it would probably be better to force every
	// client to send fully populated stream IDs.  I think the reason
	// I didn't do that is that it would cause duplicate configuration
	// (in the talker and in the endpoint.)  Perhaps the filling in of
	// the MAC could happen in the endpoint function which calls
	// findstream for the talker.
	//
	static const U8 emptyMAC[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
	if (memcmp(streamID->addr, emptyMAC, ETH_ALEN) == 0) {
		memcpy(streamID->addr, x_cfg.ifmac, ETH_ALEN);
		AVB_LOGF_DEBUG("Replaced default streamID MAC with interface MAC "ETH_FORMAT, ETH_OCTETS(streamID->addr));
	}

	clientStream_t **lpp;
	for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
		if (memcmp(streamID->addr, (*lpp)->streamID.addr, ETH_ALEN) == 0
			&& streamID->uniqueID == (*lpp)->streamID.uniqueID)
		{
			ps = *lpp;
			break;
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ps;
}

/* Find a stream by MAAP handle
 */
static clientStream_t* findStreamMaap(void* hndMaap)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);
	clientStream_t* ps = NULL;
	
	clientStream_t **lpp;
	for(lpp = &x_streamList; *lpp != NULL; lpp = &(*lpp)->next) {
		if ((*lpp)->hndMaap == hndMaap)
		{
			ps = *lpp;
			break;
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return ps;
}

/*************************************************************
 *
 * Internal function to cleanup streams
 * 
 */
bool x_talkerDeregister(clientStream_t *ps)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	openavbRC rc = OPENAVB_SUCCESS;

	if(!x_cfg.noSrp) {
		// Pass to SRP
		rc = openavbSrpDeregisterStream(&ps->streamID);
	}

	// Remove QMgr entry for stream
	if (ps->fwmark != INVALID_FWMARK) {
		openavbQmgrRemoveStream(ps->fwmark);
		ps->fwmark = INVALID_FWMARK;
	}

	// Release MAAP address allocation
	if (ps->hndMaap) {
		openavbMaapRelease(ps->hndMaap);
		ps->hndMaap = NULL;
	}
		
	// remove record
	delStream(ps);

	openavbEndPtLogAllStaticStreams();

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return IS_OPENAVB_SUCCESS(rc);
}

bool x_listenerDetach(clientStream_t *ps)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	openavbRC rc = OPENAVB_SUCCESS;

	if(!x_cfg.noSrp) {
		// Pass to SRP
		rc = openavbSrpDetachStream(&ps->streamID);
	}

	// remove record
	delStream(ps);

	openavbEndPtLogAllStaticStreams();

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return IS_OPENAVB_SUCCESS(rc);
}


/*************************************************
 * SRP CALLBACKS
 */

/* SRP tells us about a listener peer (Listener Ready or Failed)
 */
openavbRC strmAttachCb(void* pv,
							 openavbSrpLsnrDeclSubtype_t lsnrDecl)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	openavbRC rc = OPENAVB_FAILURE;
	clientStream_t *ps = (clientStream_t*)pv;

	AVB_LOGF_INFO("SRP talker callback uid=%d: lsnrDecl=%x", ps->streamID.uniqueID, lsnrDecl);

	if (lsnrDecl == openavbSrp_LDSt_Ready
		|| lsnrDecl == openavbSrp_LDSt_Ready_Failed)
	{
		// Somebody is listening - get ready to stream
		
		if (ps->fwmark != INVALID_FWMARK) {
			AVB_LOG_DEBUG("attach callback: already setup queues");
			rc = OPENAVB_SUCCESS;
		}
		else {
			AVB_LOG_DEBUG("Attach callback: setting up queues for streaming");

			rc = openavbSrpGetClassParams(ps->srClass, &ps->priority, &ps->vlanID, &ps->classRate);
			if (IS_OPENAVB_SUCCESS(rc)) {
				ps->fwmark = openavbQmgrAddStream(ps->srClass, ps->classRate, ps->tSpec.maxIntervalFrames, ps->tSpec.maxFrameSize);
				if (ps->fwmark == INVALID_FWMARK) {
					AVB_LOG_ERROR("Error in attach callback: unable to setup stream queues");
					rc = OPENAVB_FAILURE;
				}
				else {
					rc = OPENAVB_SUCCESS;
				}
			}
			else {
				AVB_LOG_ERROR("Error in attach callback: unable to get class params");
				rc = OPENAVB_FAILURE;
			}
		}
	}
	else {
		// Nobody listening
		if (ps->fwmark != INVALID_FWMARK) {
			AVB_LOG_DEBUG("Attach callback: tearing down queues");
			openavbQmgrRemoveStream(ps->fwmark);
			ps->fwmark = INVALID_FWMARK;
		}
		rc = OPENAVB_SUCCESS;
	}

	if (IS_OPENAVB_SUCCESS(rc)) {

		openavbEptSrvrNotifyTlkrOfSrpCb(ps->clientHandle,
		                             &ps->streamID,
		                             x_cfg.ifname,
		                             ps->destAddr,
		                             lsnrDecl,
		                             ps->classRate,
		                             ps->vlanID,
		                             ps->priority,
		                             ps->fwmark);
		rc = OPENAVB_SUCCESS;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return rc;
}

/* SRP tells us about talker peer (Talker Registration or De-registration)
 */
openavbRC strmRegCb(void *pv,
						  openavbSrpAttribType_t tlkrDecl,
						  U8 destAddr[],
						  AVBTSpec_t *tSpec,
						  SRClassIdx_t srClass,
						  U32 accumLatency,
						  openavbSrpFailInfo_t *failInfo)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	clientStream_t *ps = (clientStream_t*)pv;
	AVB_LOGF_INFO("SRP listener callback uid=%d: tlkrDecl=%x", ps->streamID.uniqueID, tlkrDecl);

	openavbEptSrvrNotifyLstnrOfSrpCb(ps->clientHandle,
								&ps->streamID,
								x_cfg.ifname,
								destAddr,
								tlkrDecl,
								tSpec,
								srClass,
								accumLatency,
								failInfo);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return OPENAVB_SUCCESS;
}


/*************************************************************
 * MAAP Restart - our destination MAC has been changed.
 *
 * This is a clunky way to handle it - but it works, and this code
 * won't be called often.  (MAAP sends probes before settling on an
 * address, so only a buggy or malicious peer should send us down this
 * path.)
 *
 * A better way to handle this would require SRP and the
 * talkers/listeners to look at destination addresses (in addition
 * to StreamID and talker/listner declaration) and explicitly handle
 * destination address changes.
 */
static void maapRestartCallback(void* handle, struct ether_addr *addr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	static clientStream_t* ps;
	openavbRC rc;

	ps = findStreamMaap(handle);
	AVB_LOGF_STATUS("MAAP restart callback: handle=%p, stream=%p", handle, ps);

	if (ps) {
		memcpy(ps->destAddr, addr->ether_addr_octet, ETH_ALEN);

		// Pretend that our listeners went away
		strmAttachCb(ps, (openavbSrpLsnrDeclSubtype_t)0);

		if(!x_cfg.noSrp) {
			// Remove the old registration with SRP
			openavbSrpDeregisterStream(&ps->streamID);

			// Re-register with the new address
			rc = openavbSrpRegisterStream((void*)ps,
			                          &ps->streamID,
			                          ps->destAddr,
			                          &ps->tSpec,
			                          ps->srClass,
			                          ps->srRank,
			                          ps->latency);
		} else {
			rc = OPENAVB_SUCCESS;
		}

		if (!IS_OPENAVB_SUCCESS(rc)) {
			AVB_LOG_ERROR("MAAP restart: failed to re-register talker stream");
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}


int avbEndpointLoop(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	int retVal = -1;
	openavbRC rc = OPENAVB_SUCCESS;
	do {

		if (!x_cfg.bypassAsCapableCheck && (startPTP() < 0)) {
			// make sure ptp, a seperate process, starts and is using the same interface as endpoint
			AVB_LOG_ERROR("PTP failed to start - Exiting");
			break;
		} else if(x_cfg.bypassAsCapableCheck) {
			AVB_LOG_WARNING(" ");
			AVB_LOG_WARNING("Configuration 'gptp_asCapable_not_required = 1' is set.");
			AVB_LOG_WARNING("This configuration bypasses the requirement for gPTP");
			AVB_LOG_WARNING("and openavb_gptp is not started automatically.");
			AVB_LOG_WARNING("An appropriate ptp MUST be started seperately.");
			AVB_LOG_WARNING("Any network which does not use ptp to synchronize time");
			AVB_LOG_WARNING("on each and every network device is NOT an AVB network.");
			AVB_LOG_WARNING("Such a network WILL NOT FUNCTION PROPERLY.");
			AVB_LOG_WARNING(" ");
		}

		x_streamList = NULL;

		if (!openavbQmgrInitialize(x_cfg.fqtss_mode, x_cfg.ifindex, x_cfg.ifname, x_cfg.mtu, x_cfg.link_kbit, x_cfg.nsr_kbit)) {
			AVB_LOG_ERROR("Failed to initialize QMgr");
			break;
		}

		if (!openavbMaapInitialize(x_cfg.ifname, maapRestartCallback)) {
			AVB_LOG_ERROR("Failed to initialize MAAP");
			openavbQmgrFinalize();
			break;
		}

		if(!x_cfg.noSrp) {
			// Initialize SRP
			rc = openavbSrpInitialize(strmAttachCb, strmRegCb, x_cfg.ifname, x_cfg.link_kbit, x_cfg.bypassAsCapableCheck);
		} else {
			rc = OPENAVB_SUCCESS;
			AVB_LOG_WARNING(" ");
			AVB_LOG_WARNING("Configuration 'preconfigured = 1' is set.");
			AVB_LOG_WARNING("SRP is disabled. Streams MUST be configured manually");
			AVB_LOG_WARNING("on each and every device in the network, without exception.");
			AVB_LOG_WARNING("AN AVB NETWORK WILL NOT FUNCTION AS EXPECTED UNLESS ALL");
			AVB_LOG_WARNING("STREAMS ARE PROPERLY CONFIGURED ON ALL NETWORK DEVICES.");
			AVB_LOG_WARNING(" ");
		}

		if (!IS_OPENAVB_SUCCESS(rc)) {
			AVB_LOG_ERROR("Failed to initialize SRP");
			openavbMaapFinalize();
			openavbQmgrFinalize();
			break;
		}

		if (openavbEndpointServerOpen()) {

			while (endpointRunning) {
				openavbEptSrvrService();
			}

			openavbEndpointServerClose();
		}
				
		if(!x_cfg.noSrp) {
			// Shutdown SRP
			openavbSrpShutdown();
		}

		openavbMaapFinalize();
		openavbQmgrFinalize();

		retVal = 0;
	
	} while (0);

	if (!x_cfg.bypassAsCapableCheck && (stopPTP() < 0)) {
		AVB_LOG_WARNING("Failed to execute PTP stop command: killall -s SIGINT openavb_gptp");
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return retVal;
}
