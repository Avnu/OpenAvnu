/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ACMP - AVDECC Connection Management Protocol Message Handler Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       10-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the 1722.1 ACMP - AVDECC Connection Management Protocol message handlers
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ADP_MESSAGE_H
#define OPENAVB_ADP_MESSAGE_H 1

#include "openavb_avdecc.h"

openavbRC openavbAcmpMessageHandlerStart(void);

void openavbAcmpMessageHandlerStop(void);

openavbRC openavbAcmpMessageSend(U8 messageType, openavb_acmp_ACMPCommandResponse_t *pACMPCommandResponse, U8 status);

#endif // OPENAVB_ADP_MESSAGE_H
