/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol Message Handler Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       7-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the the 1722.1 (AVDECC) discovery protocol message handlers
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

openavbRC openavbAdpMessageHandlerStart(void);

void openavbAdpMessageHandlerStop(void);

openavbRC openavbAdpMessageSend(U8 messageType);

#endif // OPENAVB_ADP_MESSAGE_H
