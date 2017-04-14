/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol : Listener State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the ACMP - AVDECC Connection Management Protocol : Listener State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.5
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_SM_LISTENER_H
#define OPENAVB_ACMP_SM_LISTENER_H 1

#include "openavb_list.h"
#include "openavb_acmp.h"

// State machine vars IEEE Std 1722.1-2013 clause 8.2.2.5.1
typedef struct {
	openavb_list_t inflight;
	openavb_array_t listenerStreamInfos;
	bool rcvdConnectRXCmd;
	bool rcvdDisconnectRXCmd;
	bool rcvdConnectTXResp;
	bool rcvdDisconnectTXResp;
	bool rcvdGetRXState;

	// Not part of spec
	bool doTerminate;
} openavb_acmp_sm_listener_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 8.2.2.5.2
bool openavbAcmpSMListener_validListenerUnique(U16 listenerUniqueId);
bool openavbAcmpSMListener_listenerIsConnected(openavb_acmp_ACMPCommandResponse_t *command);
bool openavbAcmpSMListener_listenerIsConnectedTo(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListener_txCommand(U8 messageType, openavb_acmp_ACMPCommandResponse_t *command, bool retry);
void openavbAcmpSMListener_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error);
U8 openavbAcmpSMListener_connectListener(openavb_acmp_ACMPCommandResponse_t *response);
U8 openavbAcmpSMListener_disconnectListener(openavb_acmp_ACMPCommandResponse_t *response);
void openavbAcmpSMListener_cancelTimeout(openavb_acmp_ACMPCommandResponse_t *commandResponse);
void openavbAcmpSMListener_removeInflight(openavb_acmp_ACMPCommandResponse_t *commandResponse);
U8 openavbAcmpSMListener_getState(openavb_acmp_ACMPCommandResponse_t *command);

bool openavbAcmpSMListenerStart(void);
void openavbAcmpSMListenerStop(void);

void openavbAcmpSMListenerSet_rcvdConnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdDisconnectRXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdConnectTXResp(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdDisconnectTXResp(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_rcvdGetRXState(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMListenerSet_doTerminate(bool value);

#endif // OPENAVB_ACMP_SM_LISTENER_H
