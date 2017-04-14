/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol : Controller State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for ACMP - AVDECC Connection Management Protocol : Controller State Machine
 * IEEE Std 1722.1-2013 clause 8.2.2.4
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ACMP_SM_CONTROLLER_H
#define OPENAVB_ACMP_SM_CONTROLLER_H 1

#include "openavb_acmp.h"

// State machine vars IEEE Std 1722.1-2013 clause 8.2.2.4.1
typedef struct {
	openavb_list_t inflight;
	bool rcvdResponse;

	// Not part of spec
	bool doTerminate;
} openavb_acmp_sm_controller_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 8.2.2.4.2
void openavbAcmpSMController_txCommand(U8 messageType, openavb_acmp_ACMPCommandResponse_t *command, bool retry);
void openavbAcmpSMController_cancelTimeout(openavb_acmp_ACMPCommandResponse_t *commandResponse);
void openavbAcmpSMController_removeInflight(openavb_acmp_ACMPCommandResponse_t *commandResponse);
void openavbAcmpSMController_processResponse(openavb_acmp_ACMPCommandResponse_t *commandResponse);
void openavbAcmpSMController_makeCommand(openavb_acmp_ACMPCommandParams_t *param);

bool openavbAcmpSMControllerStart(void);
void openavbAcmpSMControllerStop(void);

void openavbAcmpSMControllerSet_rcvdResponse(openavb_acmp_ACMPCommandResponse_t *command);
void openavbAcmpSMControllerSet_doTerminate(bool value);

#endif // OPENAVB_ACMP_SM_CONTROLLER_H
