/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol : Talker State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the ACMP - AVDECC Connection Management Protocol : Talker State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.6
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_SM_TALKER_H
#define OPENAVB_ACMP_SM_TALKER_H 1

#include "openavb_acmp.h"

// State machine vars IEEE Std 1722.1-2013 clause 8.2.2.6.1
typedef struct {
	openavb_array_t talkerStreamInfos;
	bool rcvdConnectTX;
	bool rcvdDisconnectTX;
	bool rcvdGetTXState;
	bool rcvdGetTXConnection;

	// Not part of spec
	bool doTerminate;
} openavb_acmp_sm_talker_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 8.2.2.6.2
bool openavbAcmpSMTalker_validTalkerUnique(U16 talkerUniqueId);
U8 openavbAcmpSMTalker_connectTalker(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalker_txResponse(U8 messageType, openavb_acmp_ACMPCommandResponse_t *response, U8 error);
U8 openavbAcmpSMTalker_disconnectTalker(openavb_acmp_ACMPCommandResponse_t *command);
U8 openavbAcmpSMTalker_getState(openavb_acmp_ACMPCommandResponse_t *command);
U8 openavbAcmpSMTalker_getConnection(openavb_acmp_ACMPCommandResponse_t *command);

bool openavbAcmpSMTalkerStart(void);
void openavbAcmpSMTalkerStop(void);

void openavbAcmpSMTalkerSet_rcvdConnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdDisconnectTXCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdGetTXState(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_rcvdGetTXConnectionCmd(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMTalkerSet_doTerminate(bool value);

void openavbAcmpSMTalker_updateStreamInfo(openavb_tl_data_cfg_t *pCfg);

#endif // OPENAVB_ACMP_SM_TALKER_H
