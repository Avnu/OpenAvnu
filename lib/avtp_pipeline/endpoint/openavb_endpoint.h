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
* HEADER SUMMARY : Declarations used by the endpoint code.
*/

#ifndef OPENAVB_ENDPOINT_H
#define OPENAVB_ENDPOINT_H

#include "openavb_types.h"
#include "openavb_srp_api.h"
#include "openavb_endpoint_cfg.h"
#include "openavb_tl.h"

#define AVB_ENDPOINT_HANDLE_INVALID (-1)
#define ENDPOINT_RECONNECT_SECONDS 	10
#define AVB_ENDPOINT_UNIX_PATH 		"/tmp/avb_endpoint"

typedef enum {
	clientNone,
	clientTalker,
	clientListener
} clientRole_t;


bool openavbEptClntService(int h, int timeout);
void openavbEptSrvrService(void);
int avbEndpointLoop(void);


typedef enum {
	// client messages
	OPENAVB_ENDPOINT_TALKER_REGISTER,
	OPENAVB_ENDPOINT_LISTENER_ATTACH,
	OPENAVB_ENDPOINT_CLIENT_STOP,
	OPENAVB_ENDPOINT_VERSION_REQUEST,

	// server messages
	OPENAVB_ENDPOINT_TALKER_CALLBACK,
	OPENAVB_ENDPOINT_LISTENER_CALLBACK,
	OPENAVB_ENDPOINT_VERSION_CALLBACK,
} openavbEndpointMsgType_t;

//////////////////////////////
// Client message parameters
//////////////////////////////
typedef struct {
	U8 			destAddr[ETH_ALEN];
	AVBTSpec_t	tSpec;
	U8			srClass;
	U8	        srRank;
	U32			latency;
} openavbEndpointParams_TalkerRegister_t;

typedef struct {
	openavbSrpLsnrDeclSubtype_t lsnrDecl;
} openavbEndpointParams_ListenerAttach_t;

typedef struct {
} openavbEndpointParams_ClientStop_t;

typedef struct {
} openavbEndpointParams_VersionRequest_t;

//////////////////////////////
// Server messages parameters
//////////////////////////////
typedef struct {
	char		ifname[IFNAMSIZ];
	U8 			destAddr[ETH_ALEN];
	openavbSrpLsnrDeclSubtype_t lsnrDecl;
	U32			classRate;
	U16			vlanID;
	U8			priority;
	U16			fwmark;
} openavbEndpointParams_TalkerCallback_t;

typedef struct {
	openavbSrpAttribType_t tlkrDecl;
	char		ifname[IFNAMSIZ];
	U8 			destAddr[ETH_ALEN];
	AVBTSpec_t	tSpec;
	U8			srClass;
	U32			latency;
	openavbSrpFailInfo_t failInfo;
} openavbEndpointParams_ListenerCallback_t; 

typedef struct {
	U32 		AVBVersion;
} openavbEndpointParams_VersionCallback_t;

#define OPENAVB_ENDPOINT_MSG_LEN sizeof(openavbEndpointMessage_t)

typedef struct clientStream_t {
	struct clientStream_t *next; // next link list pointer

	int				clientHandle;		// ID that links this info to client (talker or listener)

	// Information provided by the client (talker or listener)
	AVBStreamID_t	streamID;			// stream identifier
	clientRole_t	role;				// is client a talker or listener?

	// Information provided by the client (talker)
	AVBTSpec_t 		tSpec;				// traffic specification (bandwidth for reservation)
	SRClassIdx_t	srClass;			// AVB class
	U8 				srRank;				// AVB rank
	U32				latency;			// internal latency
	
	// Information provided by SRP
	U8				priority;			// AVB priority to use for stream
	U16				vlanID;				// VLAN ID to use for stream
	U32				classRate;			// observation intervals per second

	// Information provided by MAAP
	void			*hndMaap;			// handle for MAAP address allocation
	U8				destAddr[ETH_ALEN];	// destination MAC address (from config or MAAP)

	// Information provided by QMgr
	int				fwmark;				// mark to identify packets of this stream
} clientStream_t;

int startPTP(void);
int stopPTP(void);

bool openavbEndpointServerOpen(void);
void openavbEndpointServerClose(void);
clientStream_t* findStream(AVBStreamID_t *streamID);
void delStream(clientStream_t* ps);
clientStream_t* addStream(int h, AVBStreamID_t *streamID);
void openavbEndPtLogAllStaticStreams(void);
bool x_talkerDeregister(clientStream_t *ps);
bool x_listenerDetach(clientStream_t *ps);


openavbRC strmRegCb(void *pv,
						  openavbSrpAttribType_t tlkrDecl,
						  U8 destAddr[],
						  AVBTSpec_t *tSpec,
						  SRClassIdx_t srClass,
						  U32 accumLatency,
						  openavbSrpFailInfo_t *failInfo);

openavbRC strmAttachCb(void* pv,
							 openavbSrpLsnrDeclSubtype_t lsnrDecl);


#include "openavb_endpoint_osal.h"


/**************************  Endpoint Client-Server  *************************/
// There is a single endpoint server for the entire talker/listener device
// There is an endpoint client for each stream.

// Endpoint client open a connection to endpoint server.
// (Note that the server never opens a connection to the client.)
int  openavbEptClntOpenSrvrConnection(tl_state_t *pTLState);
// Endpoint client close its connection to the server.
void openavbEptClntCloseSrvrConnection(int h);
// Endpoint server close its connection to the client.
void openavbEptSrvrCloseClientConnection(int h);

// Immediately after establishing connection with the endpoint server,
// the endpoint client checks that the server's version is the same as its own.
// The client sends a request to the cleint for its version.
// The server handles the request and sends its version to the requesting client.
// The clinet compares the recevied version to its own.
bool openavbEptClntRequestVersionFromServer(int h);
bool openavbEptSrvrHndlVerRqstFromClient(int h);
void openavbEptSrvrSendServerVersionToClient(int h, U32 AVBVersion);
void openavbEptClntCheckVerMatchesSrvr(int h, U32 AVBVersion);


// Each talker registers its stream with SRP via Endpoint.
// Endpoint communication is from client to server
bool openavbEptClntRegisterStream(int            h,
                              AVBStreamID_t *streamID,
                              U8             destAddr[],
                              AVBTSpec_t    *tSpec,
                              U8             srClass,
                              U8             srRank,
                              U32            latency);
bool openavbEptSrvrRegisterStream(int             h,
                              AVBStreamID_t  *streamID,
                              U8              destAddr[],
                              AVBTSpec_t     *tSpec,
                              U8              srClass,
                              U8              srRank,
                              U32             latency);

// Each lister attaches to the stream it wants to receive;
// specifically the listener indicates interest / declaration to SRP.
// Endpoint communication is from client to server.
bool openavbEptClntAttachStream(int                      h,
                            AVBStreamID_t           *streamID,
                            openavbSrpLsnrDeclSubtype_t  ld);
bool openavbEptSrvrAttachStream(int                      h,
                            AVBStreamID_t           *streamID,
                            openavbSrpLsnrDeclSubtype_t  ld);


// SRP notifies the talker when its stream has been established to taken down.
// Endpoint communication is from server to client.
void openavbEptSrvrNotifyTlkrOfSrpCb(int                      h,
                                 AVBStreamID_t           *streamID,
                                 char                    *ifname,
                                 U8                       destAddr[],
                                 openavbSrpLsnrDeclSubtype_t  lsnrDecl,
                                 U32                      classRate,
                                 U16                      vlanID,
                                 U8                       priority,
                                 U16                      fwmark);
void openavbEptClntNotifyTlkrOfSrpCb(int                      h,
                                 AVBStreamID_t           *streamID,
                                 char                    *ifname,
                                 U8                       destAddr[],
                                 openavbSrpLsnrDeclSubtype_t  lsnrDecl,
                                 U32                      classRate,
                                 U16                      vlanID,
                                 U8                       priority,
                                 U16                      fwmark);

// SRP notifies the listener when its stream is available, failed or gone away.
// Endpoint communication is from server to client.
void openavbEptSrvrNotifyLstnrOfSrpCb(int                 h,
                                  AVBStreamID_t      *streamID,
                                  char               *ifname,
                                  U8                  destAddr[],
                                  openavbSrpAttribType_t  tlkrDecl,
                                  AVBTSpec_t         *tSpec,
                                  U8                  srClass,
                                  U32                 latency,
                                  openavbSrpFailInfo_t   *failInfo);
void openavbEptClntNotifyLstnrOfSrpCb(int                 h,
                                  AVBStreamID_t      *streamID,
                                  char               *ifname,
                                  U8                  destAddr[],
                                  openavbSrpAttribType_t  tlkrDecl,
                                  AVBTSpec_t         *tSpec,
                                  U8                  srClass,
                                  U32                 latency,
                                  openavbSrpFailInfo_t   *failInfo);


// A talker can withdraw its stream registration at any time;
// a listener can withdraw its stream attachement at any time;
// in ether case, endpoint communication is from client to server
bool openavbEptClntStopStream(int h, AVBStreamID_t *streamID);
bool openavbEptSrvrStopStream(int h, AVBStreamID_t *streamID);


#endif // OPENAVB_ENDPOINT_H
