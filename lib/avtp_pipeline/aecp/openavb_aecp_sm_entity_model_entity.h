/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       13-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the AVDECC Enumeration and control protocol (AECP) : Entity Model Entity State Machine
 * IEEE Std 1722.1-2013 clause 9.2.2.3
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H
#define OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H 1

#include "openavb_aecp.h"

// State machine vars IEEE Std 1722.1-2013 clause 9.2.2.3.1.2
typedef struct {
	openavb_aecp_AEMCommandResponse_t rcvdCommand;
	bool rcvdAEMCommand;
	openavb_aecp_AEMCommandResponse_t unsolicited;
	bool doUnsolicited;

	// Not part of spec
	bool doTerminate;
} openavb_aecp_sm_entity_model_entity_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 9.2.2.3.1.3
void acquireEntity(void);
void lockEntity(void);
void processCommand(void);

void openavbAecpSMEntityModelEntityStart(void);
void openavbAecpSMEntityModelEntityStop(void);

// Accessors
void openavbAecpSMEntityModelEntitySet_rcvdCommand(openavb_aecp_AEMCommandResponse_t *rcvdCommand);
void openavbAecpSMEntityModelEntitySet_rcvdAEMCommand(bool value);
void openavbAecpSMEntityModelEntitySet_unsolicited(openavb_aecp_AEMCommandResponse_t *unsolicited);
void openavbAecpSMEntityModelEntitySet_doUnsolicited(bool value);
void openavbAecpSMEntityModelEntitySet_doTerminate(bool value);

#endif // OPENAVB_AECP_SM_ENTITY_MODEL_ENTITY_H
