/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC Enumeration and control protocol (ACMP) Message Handler Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       13-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the AVDECC Enumeration and control protocol (ACMP) message handlers
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_AECP_MESSAGE_H
#define OPENAVB_AECP_MESSAGE_H 1

#include "openavb_avdecc.h"

openavbRC openavbAecpMessageHandlerStart(void);

void openavbAecpMessageHandlerStop(void);

openavbRC openavbAecpMessageSend(openavb_aecp_AEMCommandResponse_t *AEMCommandResponse);

#endif // OPENAVB_AECP_MESSAGE_H
