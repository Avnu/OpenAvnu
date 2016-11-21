/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : AVDECC - Top level 1722.1 implementation
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Top level 1722.1 implementation
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#include "openavb_rawsock.h"
#include "openavb_avtp.h"

#define	AVB_LOG_COMPONENT	"AVDECC"
#include "openavb_log.h"

#include "openavb_aem.h"
#include "openavb_adp.h"
#include "openavb_acmp.h"
#include "openavb_aecp.h"

#define ADDR_PTR(A) (U8*)(&((A)->ether_addr_octet))

// Set during openavbAVDECCInitialize() and should not be changed thereafter
openavb_avdecc_cfg_t gAvdeccCfg;

// The MAC address that will be used by AVDECC
struct ether_addr openavbAVDECCMacAddr;

bool openavbAVDECCStartAdp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbRC rc = openavbAdpStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAdpStop();
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

void openavbAVDECCStopAdp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAdpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

bool openavbAVDECCStartCmp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbRC rc = openavbAcmpStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAcmpStop();
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

void openavbAVDECCStopCmp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAcmpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

bool openavbAVDECCStartEcp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbRC rc = openavbAecpStart();
	if (IS_OPENAVB_FAILURE(rc)) {
		openavbAecpStop();
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

void openavbAVDECCStopEcp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAecpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

void openavbAVDECCFindMacAddr(openavb_avdecc_cfg_t *pAvdeccCfg)
{
	// Open a rawsock may be the easiest cross platform way to get the MAC address.
	void *txSock = openavbRawsockOpen((const char *)pAvdeccCfg->ifname, FALSE, TRUE, ETHERTYPE_AVTP, 100, 1);
	if (txSock && openavbRawsockGetAddr(txSock, ADDR_PTR(&openavbAVDECCMacAddr))) {
		openavbRawsockClose(txSock);
		txSock = NULL;
	}
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT bool openavbAVDECCInitialize(openavb_avdecc_cfg_t *pAvdeccCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Check paramenters
	if (!pAvdeccCfg) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	memcpy(&gAvdeccCfg, pAvdeccCfg, sizeof(gAvdeccCfg));

	openavbAVDECCFindMacAddr(pAvdeccCfg);

	// Create the Entity Model
	openavbRC rc = openavbAemCreate(gAvdeccCfg.pDescriptorEntity);
	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

// Start the AVDECC protocols. 
bool openavbAVDECCStart()
{
	if (openavbAVDECCStartCmp()) {
		if (openavbAVDECCStartEcp()) {
			if (openavbAVDECCStartAdp()) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

// Stop the AVDECC protocols. 
extern DLL_EXPORT void openavbAVDECCStop(void)
{
	openavbAVDECCStopCmp();
	openavbAVDECCStopEcp();
	openavbAVDECCStopAdp();
}

extern DLL_EXPORT bool openavbAVDECCCleanup(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbRC rc = openavbAemDestroy();

	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}



