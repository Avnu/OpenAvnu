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

#include "openavb_endpoint.h"

#define ADDR_PTR(A) (U8*)(&((A)->ether_addr_octet))

extern openavb_avdecc_cfg_t gAvdeccCfg;

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
	if (txSock && openavbRawsockGetAddr(txSock, gAvdeccCfg.ifmac)) {
		openavbRawsockClose(txSock);
		txSock = NULL;
	}
}

////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT bool openavbAVDECCInitialize(const char *ifname, const U8 *ifmac, const clientStream_t *streamList)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	// Check parameters
	if (!ifname || !ifmac || !streamList) {
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

	// Copy the supplied non-localized strings to the descriptor.
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

	// Add a configuration for each talker or listener stream.
	const clientStream_t *current_stream = streamList;
	while (current_stream != NULL) {
		// Create a new configuration.
		openavb_aem_descriptor_configuration_t *pConfiguration = openavbAemDescriptorConfigurationNew();
		U16 nConfigIdx = 0;
		if (!openavbAemAddDescriptor(pConfiguration, 0, &nConfigIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}

		// Add the localized strings to the configuration.
		if (!openavbAemDescriptorLocaleStringsHandlerAddToConfiguration(gAvdeccCfg.pAemDescriptorLocaleStringsHandler, nConfigIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC locale strings to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}

		// Add the descriptors needed for both talkers and listeners.
		U16 nResultIdx;
		if (!openavbAemAddDescriptor(openavbAemDescriptorAvbInterfaceNew(), openavbAemGetConfigIdx(), &nResultIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC AVB Interface to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}

		// TODO:  AVDECC_TODO Add other descriptors as needed.

		if (current_stream->role == clientTalker) {
			gAvdeccCfg.bTalker = TRUE;

			// TODO:  AVDECC_TODO Add other descriptors as needed.

			AVB_LOG_DEBUG("AVDECC talker configuration added");
		}
		if (current_stream->role == clientListener) {
			gAvdeccCfg.bListener = TRUE;

			// TODO:  AVDECC_TODO Add other descriptors as needed.

			AVB_LOG_DEBUG("AVDECC listener configuration added");
		}

		// Proceed to the next stream.
		current_stream = current_stream->next;
	}

	if (!gAvdeccCfg.bTalker && !gAvdeccCfg.bListener) {
		AVB_LOG_ERROR("No AVDECC Configurations -- Aborting");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Fill in the descriptor capabilities.
	// AVDECC_TODO:  Do we want to make the capabilities configurable?
	openavbAemDescriptorEntitySet_entity_capabilities(gAvdeccCfg.pDescriptorEntity,
		OPENAVB_ADP_ENTITY_CAPABILITIES_EFU_MODE |
		OPENAVB_ADP_ENTITY_CAPABILITIES_ADDRESS_ACCESS_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_AEM_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_CLASS_A_SUPPORTED |
		OPENAVB_ADP_ENTITY_CAPABILITIES_GPTP_SUPPORTED);

	if (gAvdeccCfg.bTalker) {
		// AVDECC_TODO:  Do we want to make the capabilities configurable?
		openavbAemDescriptorEntitySet_talker_capabilities(gAvdeccCfg.pDescriptorEntity, 1,
			OPENAVB_ADP_TALKER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_TALKER_CAPABILITIES_AUDIO_SOURCE |
			OPENAVB_ADP_TALKER_CAPABILITIES_MEDIA_CLOCK_SOURCE);
	}
	if (gAvdeccCfg.bListener) {
		// AVDECC_TODO:  Do we want to make the capabilities configurable?
		openavbAemDescriptorEntitySet_listener_capabilities(gAvdeccCfg.pDescriptorEntity, 1,
			OPENAVB_ADP_LISTENER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_LISTENER_CAPABILITIES_AUDIO_SINK);
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



