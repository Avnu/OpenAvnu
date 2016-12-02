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

extern openavb_avdecc_cfg_t gAvdeccCfg;

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

void openavbAVDECCFindMacAddr(void)
{
	// Open a rawsock may be the easiest cross platform way to get the MAC address.
	void *txSock = openavbRawsockOpen(gAvdeccCfg.ifname, FALSE, TRUE, ETHERTYPE_AVTP, 100, 1);
	if (txSock && openavbRawsockGetAddr(txSock, ADDR_PTR(&openavbAVDECCMacAddr))) {
		openavbRawsockClose(txSock);
		txSock = NULL;
	}
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT bool openavbAVDECCInitialize(const char *ifname, U8 *ifmac)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Check parameters
	if (!ifname || !ifmac) {
		AVB_RC_LOG(OPENAVB_AVDECC_FAILURE | OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	strncpy(gAvdeccCfg.ifname, ifname, sizeof(gAvdeccCfg.ifname));
	gAvdeccCfg.ifname[sizeof(gAvdeccCfg.ifname) - 1] = '\0'; // Make sure string is 0-terminated.
	memcpy(gAvdeccCfg.ifmac, ifmac, sizeof(gAvdeccCfg.ifmac));

	gAvdeccCfg.pDescriptorEntity = openavbAemDescriptorEntityNew();
	if (!gAvdeccCfg.pDescriptorEntity) {
		AVB_LOG_ERROR("Failed to allocate an AVDECC descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	if (!openavbAemDescriptorEntitySet_entity_id(gAvdeccCfg.pDescriptorEntity, NULL, gAvdeccCfg.ifmac, gAvdeccCfg.avdeccId)) {
		AVB_LOG_ERROR("Failed to set the AVDECC descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	openavbAVDECCFindMacAddr();

	// Create the Entity Model
	openavbRC rc = openavbAemCreate(gAvdeccCfg.pDescriptorEntity);
	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Fill in the descriptor capabilities.
	// TODO:  Do we want to make these configurable?
	openavbAemDescriptorEntitySet_entity_capabilities(gAvdeccCfg.pDescriptorEntity,
		OPENAVB_ADP_ENTITY_CAPABILITIES_EFU_MODE |
		OPENAVB_ADP_ENTITY_CAPABILITIES_ADDRESS_ACCESS_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_AEM_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_CLASS_A_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_GPTP_SUPPORTED);
	if (gAvdeccCfg.bTalker) {
		openavbAemDescriptorEntitySet_talker_capabilities(gAvdeccCfg.pDescriptorEntity, 1,
			OPENAVB_ADP_TALKER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_TALKER_CAPABILITIES_AUDIO_SOURCE |
			OPENAVB_ADP_TALKER_CAPABILITIES_MEDIA_CLOCK_SOURCE);
	}
	if (gAvdeccCfg.bListener) {
		openavbAemDescriptorEntitySet_listener_capabilities(gAvdeccCfg.pDescriptorEntity, 1,
			OPENAVB_ADP_LISTENER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_LISTENER_CAPABILITIES_AUDIO_SINK);
	}

	// Copy the supplied non-localized strings to the newly-created descriptor.
	openavbAemDescriptorEntitySet_entity_model_id(gAvdeccCfg.pDescriptorEntity, gAvdeccCfg.entity_model_id);
	openavbAemDescriptorEntitySet_entity_name(gAvdeccCfg.pDescriptorEntity, gAvdeccCfg.entity_name);
	openavbAemDescriptorEntitySet_firmware_version(gAvdeccCfg.pDescriptorEntity, gAvdeccCfg.firmware_version);
	openavbAemDescriptorEntitySet_group_name(gAvdeccCfg.pDescriptorEntity, gAvdeccCfg.group_name);
	openavbAemDescriptorEntitySet_serial_number(gAvdeccCfg.pDescriptorEntity, gAvdeccCfg.serial_number);

	// Initialize the localized strings support.
	gAvdeccCfg.pAemDescriptorLocaleStringsHandler = openavbAemDescriptorLocaleStringsHandlerNew();
	if (gAvdeccCfg.pAemDescriptorLocaleStringsHandler) {
		// Add the strings to the locale strings hander.
		openavbAemDescriptorLocaleStringsHandlerSet_local_string(
			gAvdeccCfg.pAemDescriptorLocaleStringsHandler, gAvdeccCfg.locale_identifier, gAvdeccCfg.vendor_name, LOCALE_STRING_VENDOR_NAME_INDEX);
		openavbAemDescriptorLocaleStringsHandlerSet_local_string(
			gAvdeccCfg.pAemDescriptorLocaleStringsHandler, gAvdeccCfg.locale_identifier, gAvdeccCfg.model_name, LOCALE_STRING_MODEL_NAME_INDEX);

		// Have the descriptor entity reference the locale strings.
		openavbAemDescriptorEntitySet_vendor_name(gAvdeccCfg.pDescriptorEntity, 0, LOCALE_STRING_VENDOR_NAME_INDEX);
		openavbAemDescriptorEntitySet_model_name(gAvdeccCfg.pDescriptorEntity, 0, LOCALE_STRING_MODEL_NAME_INDEX);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

// Start the AVDECC protocols. 
extern DLL_EXPORT bool openavbAVDECCStart()
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



