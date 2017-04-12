/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol : Advertise interface State Machine Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       5-Dec-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Interface for the AVDECC Discovery Protocol : Advertise Interface State Machine
 * IEEE Std 1722.1-2013 clause 6.2.5.
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ADP_SM_ADVERTISE_INTERFACE_H
#define OPENAVB_ADP_SM_ADVERTISE_INTERFACE_H 1

#include "openavb_adp.h"

// State machine vars IEEE Std 1722.1-2013 clause 6.2.5.1
typedef struct {
	U8 advertisedGrandmasterID[8];
	bool rcvdDiscover;
	U8 entityID[8];
	bool doTerminate;
	bool doAdvertise;
	bool linkIsUp;
	bool lastLinkIsUp;
} openavb_adp_sm_advertise_interface_vars_t;

// State machine functions IEEE Std 1722.1-2013 clause 6.2.5.2
void openavbAdpSMAdvertiseInterface_txEntityAvailable(void);
void openavbAdpSMAdvertiseInterface_txEntityDeparting(void);

void openavbAdpSMAdvertiseInterfaceStart(void);
void openavbAdpSMAdvertiseInterfaceStop(void);

// Accessors
void openavbAdpSMAdvertiseInterfaceSet_advertisedGrandmasterID(U8 *pValue);
void openavbAdpSMAdvertiseInterfaceSet_rcvdDiscover(bool value);
void openavbAdpSMAdvertiseInterfaceSet_entityID(U8 *pValue);
void openavbAdpSMAdvertiseInterfaceSet_doTerminate(bool value);
void openavbAdpSMAdvertiseInterfaceSet_doAdvertise(bool value);
void openavbAdpSMAdvertiseInterfaceSet_linkIsUp(bool value);



#endif // OPENAVB_ADP_SM_ADVERTISE_INTERFACE_H
