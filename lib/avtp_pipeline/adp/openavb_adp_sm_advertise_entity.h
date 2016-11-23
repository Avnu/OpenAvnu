/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol : Advertise Entity State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       5-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the AVDECC Discovery Protocol : Advertise Entity State Machine
 * IEEE Std 1722.1-2013 clause 6.2.4.
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ADP_SM_ADVERTISE_ENTITY_H
#define OPENAVB_ADP_SM_ADVERTISE_ENTITY_H 1

#include "openavb_adp.h"

// State machine vars IEEE Std 1722.1-2013 clause 6.2.4.1
typedef struct {
	struct timespec reannounceTimerTimeout;
	bool needsAdvertise;
	bool doTerminate;
} openavb_adp_sm_advertise_entity_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 6.2.4.2
void openavbAdpSMAdvertiseEntity_sendAvailable(void);

void openavbAdpSMAdvertiseEntityStart(void);
void openavbAdpSMAdvertiseEntityStop(void);

// Accessors
void openavbAdpSMAdvertiseEntitySet_needsAdvertise(bool value);
void openavbAdpSMAdvertiseEntitySet_doTerminate(bool value);


#endif // OPENAVB_ADP_SM_ADVERTISE_ENTITY_H
