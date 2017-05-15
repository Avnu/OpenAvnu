/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
 ******************************************************************
 * MODULE : AVDECC - Top level 1722.1 implementation
 * MODULE SUMMARY : Top level 1722.1 implementation
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

openavb_avdecc_cfg_t gAvdeccCfg;
openavb_tl_data_cfg_t * streamList = NULL;
openavb_aem_descriptor_configuration_t *pConfiguration = NULL;

static openavb_avdecc_configuration_cfg_t *pFirstConfigurationCfg = NULL;
static U8 talker_stream_sources = 0;
static U8 listener_stream_sources = 0;
static bool first_talker = 1;
static bool first_listener = 1;

bool openavbAvdeccStartAdp()
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

void openavbAvdeccStopAdp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAdpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

bool openavbAvdeccStartCmp()
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

void openavbAvdeccStopCmp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAcmpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

bool openavbAvdeccStartEcp()
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

void openavbAvdeccStopEcp()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);
	openavbAecpStop();
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

void openavbAvdeccFindMacAddr(void)
{
	// Open a rawsock may be the easiest cross platform way to get the MAC address.
	void *txSock = openavbRawsockOpen(gAvdeccCfg.ifname, FALSE, TRUE, ETHERTYPE_AVTP, 100, 1);
	if (txSock) {
		openavbRawsockGetAddr(txSock, gAvdeccCfg.ifmac);
		openavbRawsockClose(txSock);
		txSock = NULL;
	}
}

bool openavbAvdeccAddConfiguration(openavb_tl_data_cfg_t *stream)
{
	bool first_time = 0;
	// Create a new config to hold the configuration information.
	openavb_avdecc_configuration_cfg_t *pCfg = malloc(sizeof(openavb_avdecc_configuration_cfg_t));
	if (!pCfg) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	memset(pCfg, 0, sizeof(openavb_avdecc_configuration_cfg_t));

	// Add a pointer to the supplied stream information.
	pCfg->stream = stream;

	// Add the new config to the end of the list of configurations.
	if (pFirstConfigurationCfg == NULL) {
		pFirstConfigurationCfg = pCfg;
	} else {
		openavb_avdecc_configuration_cfg_t *pLast = pFirstConfigurationCfg;
		while (pLast->next != NULL) {
			pLast = pLast->next;
		}
		pLast->next = pCfg;
	}

	// Create a new configuration.
	U16 nConfigIdx = 0;
	if (pConfiguration == NULL)
	{
		first_time = 1;
		pConfiguration = openavbAemDescriptorConfigurationNew();
		if (!openavbAemAddDescriptor(pConfiguration, OPENAVB_AEM_DESCRIPTOR_INVALID, &nConfigIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}
	// Specify a default user-friendly name to use.
	// AVDECC_TODO - Allow the user to specify a friendly name, or use the name if the .INI file.
	if (pCfg->friendly_name[0] == '\0') {
		snprintf((char *) pCfg->friendly_name, OPENAVB_AEM_STRLEN_MAX, "Configuration %u", nConfigIdx);
	}

	// Save the stream information in the configuration.
	if (!openavbAemDescriptorConfigurationInitialize(pConfiguration, nConfigIdx, pCfg)) {
		AVB_LOG_ERROR("Error initializing AVDECC configuration");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Add the descriptors needed for both talkers and listeners.
	U16 nResultIdx;
	if (first_time)
	{
		openavb_aem_descriptor_avb_interface_t *pNewAvbInterface = openavbAemDescriptorAvbInterfaceNew();
		if (!openavbAemAddDescriptor(pNewAvbInterface, nConfigIdx, &nResultIdx) ||
				!openavbAemDescriptorAvbInterfaceInitialize(pNewAvbInterface, nConfigIdx, pCfg)) {
			AVB_LOG_ERROR("Error adding AVDECC AVB Interface to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
		openavb_aem_descriptor_audio_unit_t *pNewAudioUnit = openavbAemDescriptorAudioUnitNew();
		if (!openavbAemAddDescriptor(pNewAudioUnit, nConfigIdx, &nResultIdx) ||
				!openavbAemDescriptorAudioUnitInitialize(pNewAudioUnit, nConfigIdx, pCfg)) {
			AVB_LOG_ERROR("Error adding AVDECC Audio Unit to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}
	else
	{
		openavb_aem_descriptor_audio_unit_t *pAudioUnitDescriptor =
		(openavb_aem_descriptor_audio_unit_t *) openavbAemGetDescriptor(nConfigIdx, OPENAVB_AEM_DESCRIPTOR_AUDIO_UNIT, 0);
		if (pAudioUnitDescriptor != NULL)
		{
			if (!openavbAemDescriptorAudioUnitInitialize(pAudioUnitDescriptor, nConfigIdx, pCfg)) {
				AVB_LOG_ERROR("Error updating AVDECC Audio Unit to configuration");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return FALSE;
			}
		}
		else
		{
			AVB_LOG_ERROR("Error getting AVDECC Audio Unit descriptor");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}

	// AVDECC_TODO:  Add other descriptors as needed.  Future options include:
	//  VIDEO_UNIT
	//  SENSOR_UNIT
	//  CONTROL

	if (stream->role == AVB_ROLE_TALKER) {
		gAvdeccCfg.bTalker = TRUE;

		openavb_aem_descriptor_stream_io_t *pNewStreamOutput = openavbAemDescriptorStreamOutputNew();
		if (!openavbAemAddDescriptor(pNewStreamOutput, nConfigIdx, &nResultIdx) ||
				!openavbAemDescriptorStreamOutputInitialize(pNewStreamOutput, nConfigIdx, pCfg)) {
			AVB_LOG_ERROR("Error adding AVDECC Stream Output to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
		if (first_talker)
		{
			first_talker = 0;
			openavb_aem_descriptor_clock_source_t *pNewClockSource = openavbAemDescriptorClockSourceNew();
			if (!openavbAemAddDescriptor(pNewClockSource, nConfigIdx, &nResultIdx) ||
					!openavbAemDescriptorClockSourceInitialize(pNewClockSource, nConfigIdx, pCfg)) {
				AVB_LOG_ERROR("Error adding AVDECC Clock Source to configuration");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return FALSE;
			}
			openavb_aem_descriptor_clock_domain_t *pNewClockDomain = openavbAemDescriptorClockDomainNew();
			if (!openavbAemAddDescriptor(pNewClockDomain, nConfigIdx, &nResultIdx) ||
					!openavbAemDescriptorClockDomainInitialize(pNewClockDomain, nConfigIdx, pCfg)) {
				AVB_LOG_ERROR("Error adding AVDECC Clock Domain to configuration");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return FALSE;
			}
		}

		// AVDECC_TODO:  Add other descriptors as needed.  Future options include:
		//  JACK_INPUT
		talker_stream_sources++;

		// Add the class specific to the talker.
		if (stream->sr_class == SR_CLASS_A) { gAvdeccCfg.bClassASupported = TRUE; }
		if (stream->sr_class == SR_CLASS_B) { gAvdeccCfg.bClassBSupported = TRUE; }

		AVB_LOG_DEBUG("AVDECC talker configuration added");
	}
	if (stream->role == AVB_ROLE_LISTENER) {
		gAvdeccCfg.bListener = TRUE;

		openavb_aem_descriptor_stream_io_t *pNewStreamInput = openavbAemDescriptorStreamInputNew();
		if (!openavbAemAddDescriptor(pNewStreamInput, nConfigIdx, &nResultIdx) ||
				!openavbAemDescriptorStreamInputInitialize(pNewStreamInput, nConfigIdx, pCfg)) {
			AVB_LOG_ERROR("Error adding AVDECC Stream Input to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
		if (first_listener)
		{
			openavb_aem_descriptor_clock_source_t *pNewClockSource = openavbAemDescriptorClockSourceNew();
			if (!openavbAemAddDescriptor(pNewClockSource, nConfigIdx, &nResultIdx) ||
					!openavbAemDescriptorClockSourceInitialize(pNewClockSource, nConfigIdx, pCfg)) {
				AVB_LOG_ERROR("Error adding AVDECC Clock Source to configuration");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return FALSE;
			}
			openavb_aem_descriptor_clock_domain_t *pNewClockDomain = openavbAemDescriptorClockDomainNew();
			if (!openavbAemAddDescriptor(pNewClockDomain, nConfigIdx, &nResultIdx) ||
					!openavbAemDescriptorClockDomainInitialize(pNewClockDomain, nConfigIdx, pCfg)) {
				AVB_LOG_ERROR("Error adding AVDECC Clock Domain to configuration");
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return FALSE;
			}
		}

		// AVDECC_TODO:  Add other descriptors as needed.  Future options include:
		//  JACK_OUTPUT
		listener_stream_sources++;

		// Listeners support both Class A and Class B.
		gAvdeccCfg.bClassASupported = TRUE;
		gAvdeccCfg.bClassBSupported = TRUE;

		AVB_LOG_DEBUG("AVDECC listener configuration added");
	}

	if (first_time)
	{
		// Add the localized strings to the configuration.
		if (!openavbAemDescriptorLocaleStringsHandlerAddToConfiguration(gAvdeccCfg.pAemDescriptorLocaleStringsHandler, nConfigIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC locale strings to configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}

	return TRUE;
}


////////////////////////////////
// Public functions
////////////////////////////////
extern DLL_EXPORT bool openavbAvdeccInitialize()
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	gAvdeccCfg.pDescriptorEntity = openavbAemDescriptorEntityNew();
	if (!gAvdeccCfg.pDescriptorEntity) {
		AVB_LOG_ERROR("Failed to allocate an AVDECC descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	openavbAvdeccFindMacAddr();

	if (!openavbAemDescriptorEntitySet_entity_id(gAvdeccCfg.pDescriptorEntity, NULL, gAvdeccCfg.ifmac, gAvdeccCfg.avdeccId)) {
		AVB_LOG_ERROR("Failed to set the AVDECC descriptor");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

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

	gAvdeccCfg.bTalker = gAvdeccCfg.bListener = FALSE;

	// Add a configuration for each talker or listener stream.
	openavb_tl_data_cfg_t *current_stream = streamList;
	while (current_stream != NULL) {
		// Create a new configuration with the information from this stream.
		if (!openavbAvdeccAddConfiguration(current_stream)) {
			AVB_LOG_ERROR("Error adding AVDECC configuration");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}

		// Proceed to the next stream.
		current_stream = current_stream->next;
	}

	if (!gAvdeccCfg.bTalker && !gAvdeccCfg.bListener) {
		AVB_LOG_ERROR("No AVDECC Configurations -- Aborting");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	// Add non-top-level descriptors.  These are independent of the configurations.
	// STRINGS are handled by gAvdeccCfg.pAemDescriptorLocaleStringsHandler, so not included here.
	U16 nResultIdx;
	if (!openavbAemAddDescriptor(openavbAemDescriptorAudioClusterNew(), OPENAVB_AEM_DESCRIPTOR_INVALID, &nResultIdx)) {
		AVB_LOG_ERROR("Error adding AVDECC Audio Cluster");
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (gAvdeccCfg.bTalker) {
		if (!openavbAemAddDescriptor(openavbAemDescriptorStreamPortOutputNew(), OPENAVB_AEM_DESCRIPTOR_INVALID, &nResultIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC Output Stream Port");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}
	if (gAvdeccCfg.bListener) {
		if (!openavbAemAddDescriptor(openavbAemDescriptorStreamPortInputNew(), OPENAVB_AEM_DESCRIPTOR_INVALID, &nResultIdx)) {
			AVB_LOG_ERROR("Error adding AVDECC Input Stream Port");
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}

	// AVDECC_TODO:  Add other descriptors as needed.  Future options include:
	//  EXTERNAL_PORT_INPUT
	//  EXTERNAL_PORT_OUTPUT
	//  INTERNAL_PORT_INPUT
	//  INTERNAL_PORT_OUTPUT
	//  VIDEO_CLUSTER
	//  SENSOR_CLUSTER
	//  AUDIO_MAP
	//  VIDEO_MAP
	//  SENSOR_MAP

	// Fill in the descriptor capabilities.
	// AVDECC_TODO:  Set these based on the available capabilities.
	if (!gAvdeccCfg.bClassASupported && !gAvdeccCfg.bClassBSupported) {
		// If the user didn't specify a traffic class, assume both are supported.
		gAvdeccCfg.bClassASupported = gAvdeccCfg.bClassBSupported = TRUE;
	}
	openavbAemDescriptorEntitySet_entity_capabilities(gAvdeccCfg.pDescriptorEntity,
		OPENAVB_ADP_ENTITY_CAPABILITIES_AEM_SUPPORTED |
		(gAvdeccCfg.bClassASupported ? OPENAVB_ADP_ENTITY_CAPABILITIES_CLASS_A_SUPPORTED : 0) |
		(gAvdeccCfg.bClassBSupported ? OPENAVB_ADP_ENTITY_CAPABILITIES_CLASS_B_SUPPORTED : 0) |
		OPENAVB_ADP_ENTITY_CAPABILITIES_GPTP_SUPPORTED);

	if (gAvdeccCfg.bTalker) {
		// AVDECC_TODO:  Set these based on the available capabilities.
		openavbAemDescriptorEntitySet_talker_capabilities(gAvdeccCfg.pDescriptorEntity, talker_stream_sources,
			OPENAVB_ADP_TALKER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_TALKER_CAPABILITIES_AUDIO_SOURCE |
			OPENAVB_ADP_TALKER_CAPABILITIES_MEDIA_CLOCK_SOURCE);
	}
	if (gAvdeccCfg.bListener) {
		// AVDECC_TODO:  Set these based on the available capabilities.
		openavbAemDescriptorEntitySet_listener_capabilities(gAvdeccCfg.pDescriptorEntity, listener_stream_sources,
			OPENAVB_ADP_LISTENER_CAPABILITIES_IMPLEMENTED |
			OPENAVB_ADP_LISTENER_CAPABILITIES_AUDIO_SINK);
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}

// Start the AVDECC protocols.
extern DLL_EXPORT bool openavbAvdeccStart()
{
	if (!openavbAvdeccStartCmp()) {
		AVB_LOG_ERROR("openavbAvdeccStartCmp() failure!");
		return FALSE;
	}
	if (!openavbAvdeccStartEcp()) {
		AVB_LOG_ERROR("openavbAvdeccStartEcp() failure!");
		return FALSE;
	}
	if (!openavbAvdeccStartAdp()) {
		AVB_LOG_ERROR("openavbAvdeccStartAdp() failure!");
		return FALSE;
	}

	return TRUE;
}

// Stop the AVDECC protocols.
extern DLL_EXPORT void openavbAvdeccStop(void)
{
	openavbAvdeccStopCmp();
	openavbAvdeccStopEcp();
	openavbAvdeccStopAdp();
}

extern DLL_EXPORT bool openavbAvdeccCleanup(void)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbRC rc = openavbAemDestroy();

	while (pFirstConfigurationCfg) {
		openavb_avdecc_configuration_cfg_t *pDel = pFirstConfigurationCfg;
		pFirstConfigurationCfg = pFirstConfigurationCfg->next;
		free(pDel);
	}

	if (IS_OPENAVB_FAILURE(rc)) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}
