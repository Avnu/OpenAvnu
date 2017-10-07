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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "ini.h"

#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_audio_pub.h"
#include "openavb_avdecc_pub.h"
#include "openavb_avdecc_read_ini_pub.h"
#include "openavb_avdecc_save_state.h"

#define	AVB_LOG_COMPONENT	"TL INI"
#include "openavb_log.h"

#define MATCH(A, B)(strcasecmp((A), (B)) == 0)
#define MATCH_LEFT(A, B, C)(strncasecmp((A), (B), (C)) == 0)

extern openavb_avdecc_cfg_t gAvdeccCfg;

static bool parse_mac(const char *str, cfg_mac_t *mac)
{
	memset(&mac->buffer, 0, sizeof(struct ether_addr));

	mac->mac = ether_aton_r(str, &mac->buffer);
	if (mac->mac)
		return TRUE;

	AVB_LOGF_ERROR("Failed to parse addr: %s", str);
	return FALSE;
}

static void openavbIniCfgInit(openavb_tl_data_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	memset(pCfg, 0, sizeof(openavb_tl_data_cfg_t));

	// Set default values.
	// (These values should match those set in openavbTLInitCfg().)
	pCfg->role = AVB_ROLE_UNDEFINED;
	pCfg->initial_state = TL_INIT_STATE_UNSPECIFIED;
	pCfg->stream_uid = 0xFFFF;
	pCfg->max_interval_frames = 1;
	pCfg->max_frame_size = 1500;
	pCfg->max_transit_usec = 50000;
	pCfg->max_transmit_deficit_usec = 50000;
	pCfg->internal_latency = 0;
	pCfg->max_stale = MICROSECONDS_PER_SECOND;
	pCfg->batch_factor = 1;
	pCfg->report_seconds = 0;
	pCfg->start_paused = FALSE;
	pCfg->sr_class = SR_CLASS_B;
	pCfg->sr_rank = SR_RANK_REGULAR;
	pCfg->raw_tx_buffers = 8;
	pCfg->raw_rx_buffers = 100;
	pCfg->tx_blocking_in_intf =  0;
	pCfg->rx_signal_mode = 1;
	pCfg->vlan_id = 0xFFFF;
	pCfg->fixed_timestamp = 0;
	pCfg->spin_wait = FALSE;
	pCfg->thread_rt_priority = 0;
	pCfg->thread_affinity = 0xFFFFFFFF;

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
}

// callback function - called for each name/value pair by ini parsing library
static int openavbIniCfgCallback(void *user, const char *tlSection, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavb_tl_data_cfg_t *pCfg = (openavb_tl_data_cfg_t *)user;

	AVB_LOGF_DEBUG("name=[%s] value=[%s]", name, value);

	bool valOK = FALSE;
	char *pEnd;

	if (MATCH(name, "role")) {
		if (MATCH(value, "talker")) {
			pCfg->role = AVB_ROLE_TALKER;
			valOK = TRUE;
		}
		else if (MATCH(value, "listener")) {
			pCfg->role = AVB_ROLE_LISTENER;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "initial_state")) {
		if (MATCH(value, "running")) {
			pCfg->initial_state = TL_INIT_STATE_RUNNING;
			valOK = TRUE;
		}
		else if (MATCH(value, "stopped")) {
			pCfg->initial_state = TL_INIT_STATE_STOPPED;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "dest_addr")) {
		valOK = parse_mac(value, &pCfg->dest_addr);
	}
	else if (MATCH(name, "stream_addr")) {
		valOK = parse_mac(value, &pCfg->stream_addr);
	}
	else if (MATCH(name, "stream_uid")) {
		errno = 0;
		pCfg->stream_uid = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->stream_uid <= UINT16_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "max_interval_frames")) {
		errno = 0;
		pCfg->max_interval_frames = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->max_interval_frames <= UINT16_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "max_frame_size")) {
		errno = 0;
		pCfg->max_frame_size = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->max_interval_frames <= UINT16_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "sr_class")) {
		if (strlen(value) == 1) {
			if (tolower(value[0]) == 'a') {
				pCfg->sr_class = SR_CLASS_A;
				valOK = TRUE;
			}
			else if (tolower(value[0]) == 'b') {
				pCfg->sr_class = SR_CLASS_B;
				valOK = TRUE;
			}
		}
	}
	else if (MATCH(name, "sr_rank")) {
		if (strlen(value) == 1) {
			if (value[0] == '1') {
				pCfg->sr_rank = SR_RANK_REGULAR;
				valOK = TRUE;
			}
			else if (value[0] == '0') {
				pCfg->sr_rank = SR_RANK_EMERGENCY;
				valOK = TRUE;
			}
		}
	}
	else if (MATCH(name, "max_transit_usec")) {
		errno = 0;
		pCfg->max_transit_usec = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->max_transit_usec <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "max_transmit_deficit_usec")) {
		errno = 0;
		pCfg->max_transmit_deficit_usec = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->max_transmit_deficit_usec <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "internal_latency")) {
		errno = 0;
		pCfg->internal_latency = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->internal_latency <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "batch_factor")) {
		errno = 0;
		pCfg->batch_factor = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->batch_factor > 0
			&& pCfg->batch_factor <= INT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "max_stale")) {
		errno = 0;
		pCfg->max_stale = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->max_stale >= 0
			&& pCfg->max_stale <= INT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "raw_tx_buffers")) {
		errno = 0;
		pCfg->raw_tx_buffers = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->raw_tx_buffers <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "raw_rx_buffers")) {
		errno = 0;
		pCfg->raw_rx_buffers = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->raw_rx_buffers <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "report_seconds")) {
		errno = 0;
		pCfg->report_seconds = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& (int)pCfg->report_seconds >= 0
			&& pCfg->report_seconds <= INT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "start_paused")) {
		// ignore this item - tl_host doesn't use it because
		// it pauses before reading any of its streams.
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& tmp >= 0
			&& tmp <= 1) {
			pCfg->start_paused = (tmp == 1);
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "vlan_id")) {
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 0);
		// vlanID is 12 bit field
		if (*pEnd == '\0' && errno == 0
			&& tmp >= 0x0
			&& tmp <= 0xFFF) {
			pCfg->vlan_id = tmp;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "fixed_timestamp")) {
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 0);
		if (*pEnd == '\0' && errno == 0) {
			pCfg->fixed_timestamp = tmp;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "spin_wait")) {
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 0);
		if (*pEnd == '\0' && errno == 0) {
			pCfg->spin_wait = (tmp == 1);
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "tx_blocking_in_intf")) {
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 0);
		if (*pEnd == '\0' && errno == 0) {
			pCfg->tx_blocking_in_intf = tmp;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "thread_rt_priority")) {
		errno = 0;
		long tmp;
		tmp = strtol(value, &pEnd, 0);
		if (*pEnd == '\0' && errno == 0) {
			pCfg->thread_rt_priority = tmp;
			valOK = TRUE;
		}
	}
	else if (MATCH(name, "thread_affinity")) {
		errno = 0;
		unsigned long tmp;
		tmp = strtoul(value, &pEnd, 0);
		if (*pEnd == '\0' && errno == 0) {
			pCfg->thread_affinity = tmp;
			valOK = TRUE;
		}
	}

	else if (MATCH(name, "friendly_name")) {
		strncpy(pCfg->friendly_name, value, FRIENDLY_NAME_SIZE - 1);
		valOK = TRUE;
	}

	else if (MATCH(name, "current_sampling_rate")) {
		errno = 0;
		pCfg->current_sampling_rate = strtol(value, &pEnd, 10);
		if (*pEnd == '\0' && errno == 0
			&& pCfg->current_sampling_rate <= UINT32_MAX)
			valOK = TRUE;
	}
	else if (MATCH(name, "sampling_rates")) {
		errno = 0;
		memset(pCfg->sampling_rates,0,sizeof(pCfg->sampling_rates));
		char *rate, *copy;
		copy = strdup(value);
		rate = strtok(copy,",");
		int i = 0,break_flag = 0;
		while (rate != NULL )
		{
			pCfg->sampling_rates[i] = strtol(rate,&pEnd, 10);
			if (*pEnd != '\0' || errno != 0
				|| pCfg->sampling_rates[i] > UINT32_MAX)
			{
				break_flag = 1;
				break;
			}
			rate = strtok(NULL,",");
			i++;
		}
		if (break_flag != 1)
		{
			valOK = TRUE;
			pCfg->sampling_rates_count = i;
		}
	}
	else if (strcmp(name, "intf_nv_audio_rate") == 0) {
		long int val = strtol(value, &pEnd, 10);
		if (val >= AVB_AUDIO_RATE_8KHZ && val <= AVB_AUDIO_RATE_192KHZ) {
			pCfg->audioRate = val;
			valOK = TRUE;
		}
		else {
			AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
			pCfg->audioRate = AVB_AUDIO_RATE_44_1KHZ;
		}
	}
	else if (strcmp(name, "intf_nv_audio_bit_depth") == 0) {
		long int val = strtol(value, &pEnd, 10);
		if (val >= AVB_AUDIO_BIT_DEPTH_1BIT && val <= AVB_AUDIO_BIT_DEPTH_64BIT) {
			pCfg->audioBitDepth = val;
			valOK = TRUE;
		}
		else {
			AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_bits.");
			pCfg->audioBitDepth = AVB_AUDIO_BIT_DEPTH_24BIT;
		}
	}
	else if (strcmp(name, "intf_nv_audio_channels") == 0) {
		long int val = strtol(value, &pEnd, 10);
		if (val >= AVB_AUDIO_CHANNELS_1 && val <= AVB_AUDIO_CHANNELS_8) {
			pCfg->audioChannels = val;
			valOK = TRUE;
		}
		else {
			AVB_LOG_ERROR("Invalid audio channels configured for intf_nv_audio_channels.");
			pCfg->audioChannels = AVB_AUDIO_CHANNELS_2;
		}
	}
	else if (MATCH(name, "map_fn")) {
		errno = 0;
		memset(pCfg->map_fn,0,sizeof(pCfg->map_fn));
		strncpy(pCfg->map_fn,value,sizeof(pCfg->map_fn)-1);
		valOK = TRUE;
	}

	else {
		// Ignored item.
		AVB_LOGF_DEBUG("Unhandled configuration item: name=%s, value=%s", name, value);

		// Don't abort for this item.
		valOK = TRUE;
	}

	if (!valOK) {
		// bad value
		AVB_LOGF_ERROR("Invalid value: name=%s, value=%s", name, value);
		return 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);

	return 1; // OK
}

bool openavbReadTlDataIniFile(const char *fileName, openavb_tl_data_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	openavbIniCfgInit(pCfg);

	// Use the .INI file name as the default friendly name.
	strncpy(pCfg->friendly_name, fileName, FRIENDLY_NAME_SIZE - 1);
	char * pszComma = strchr(pCfg->friendly_name, ',');
	if (pszComma) {
		// Get rid of anything following the file name.
		*pszComma = '\0';
	}
	char * pszExtension = strrchr(pCfg->friendly_name, '.');
	if (pszExtension && strcasecmp(pszExtension, ".ini") == 0) {
		// Get rid of the .INI file extension.
		*pszExtension = '\0';
	}

	int result = ini_parse(fileName, openavbIniCfgCallback, pCfg);
	if (result < 0) {
		AVB_LOGF_ERROR("Couldn't parse INI file: %s", fileName);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}
	if (result > 0) {
		AVB_LOGF_ERROR("Error in INI file: %s, line %d", fileName, result);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	if (pCfg->current_sampling_rate != 0 && pCfg->audioRate != 0 &&
		pCfg->current_sampling_rate != pCfg->audioRate)
	{
		AVB_LOGF_ERROR("current_sampling_rate(%u) and intf_nv_audio_rate(%u) do not match.", pCfg->current_sampling_rate, pCfg->audioRate);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return FALSE;
	}

	if (pCfg->current_sampling_rate == 0)
	{
		/* Make sure we have a default sampling rate. */
		if (pCfg->audioRate != 0) {
			pCfg->current_sampling_rate = pCfg->audioRate;
		} else if (pCfg->sampling_rates[0] != 0) {
			pCfg->current_sampling_rate = pCfg->sampling_rates[0];
		} else {
			pCfg->current_sampling_rate = AVB_AUDIO_RATE_48KHZ;
		}
		if (pCfg->audioRate == 0) {
			AVB_LOGF_WARNING("current_sampling_rate not specified in %s. Defaulting to %u", fileName, pCfg->current_sampling_rate);
		}
	}
	if (pCfg->sampling_rates_count == 0)
	{
		/* Set the list of sampling rates to the current sampling rate. */
		pCfg->sampling_rates[0] = pCfg->current_sampling_rate;
		pCfg->sampling_rates_count = 1;
		if (pCfg->audioRate == 0) {
			AVB_LOGF_WARNING("sampling_rates not specified in %s. Defaulting to %u", fileName, pCfg->current_sampling_rate);
		}
	} else {
		/* Make sure the current sampling rate is in the list of sampling rates. */
		U16 i;
		for (i = 0; i < pCfg->sampling_rates_count; ++i) {
			if (pCfg->sampling_rates[i] == pCfg->current_sampling_rate) break;
		}
		if (i >= pCfg->sampling_rates_count) {
			AVB_LOGF_ERROR("current_sampling_rate(%u) not in list of sampling_rates.", pCfg->current_sampling_rate);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return FALSE;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return TRUE;
}


// Save the connection to the saved state
//
bool openavbAvdeccSaveState(const openavb_tl_data_cfg_t *pListener, U16 flags, U16 talker_unique_id, const U8 talker_entity_id[8], const U8 controller_entity_id[8])
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	int i;

	// Don't add to the saved state list if fast connect support is not enabled.
	if (!gAvdeccCfg.bFastConnectSupported) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return false;
	}

	// If the supplied saved state matches one of the ones already saved, do nothing and return.
	// If the Talker or Controller has changed, delete the old information.
	for (i = 0; i < 1000; ++i) {
		const openavb_saved_state_t * pTest = openavbAvdeccGetSavedState(i);
		if (!pTest) {
			break;
		}

		if (strcmp(pTest->listener_friendly_name, pListener->friendly_name) == 0) {
			if (pTest->flags == flags &&
					pTest->talker_unique_id == talker_unique_id &&
					memcmp(pTest->talker_entity_id, talker_entity_id, 8) == 0 &&
					memcmp(pTest->controller_entity_id, controller_entity_id, 8) == 0) {
				// The supplied data is a match for the existing item.  Do nothing.
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return true;
			}

			// Delete this item.  We will create a new item with the updated information.
			openavbAvdeccDeleteSavedState(i);
			break;
		}
	}

	// Add the supplied state to the list of states.
	openavbAvdeccAddSavedState(pListener->friendly_name, flags, talker_unique_id, talker_entity_id, controller_entity_id);

	AVB_LOGF_DEBUG("New saved state:  listener_id=%s, flags=0x%04x, talker_unique_id=0x%04x, talker_entity_id=" ENTITYID_FORMAT ", controller_entity_id=" ENTITYID_FORMAT,
		pListener->friendly_name,
		flags,
		talker_unique_id,
		ENTITYID_ARGS(talker_entity_id),
		ENTITYID_ARGS(controller_entity_id));

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return true;
}

// Delete a connection with saved state
//
bool openavbAvdeccClearSavedState(const openavb_tl_data_cfg_t *pListener)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	int i;

	// Delete the saved state matching the supplied one.
	// If the supplied saved state does not match any of the ones already saved, do nothing and return.
	for (i = 0; i < 1000; ++i) {
		const openavb_saved_state_t * pTest = openavbAvdeccGetSavedState(i);
		if (!pTest) {
			break;
		}

		if (strcmp(pTest->listener_friendly_name, pListener->friendly_name) == 0) {
			// We found the index for the item to delete.
			// Delete the item and save the updated file.
			openavbAvdeccDeleteSavedState(i);
			AVB_LOGF_DEBUG("Cleared saved state:  listener_id=\"%s\"", pListener->friendly_name);

			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return true;
		}
	}

	AVB_LOGF_WARNING("Unable to find saved state to clear:  listener_id=\"%s\"", pListener->friendly_name);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return false;
}

// Determine if the connection has a saved state
//
bool openavbAvdeccGetSaveStateInfo(const openavb_tl_data_cfg_t *pListener, U16 *p_flags, U16 *p_talker_unique_id, U8 (*p_talker_entity_id)[8], U8 (*p_controller_entity_id)[8])
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	int i;

	// Don't return anything from the saved state list if fast connect support is not enabled.
	if (!gAvdeccCfg.bFastConnectSupported) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return false;
	}

	// If the loaded saved state information matches the Listener supplied, return the information for it.
	for (i = 0; i < 1000; ++i) {
		const openavb_saved_state_t * pTest = openavbAvdeccGetSavedState(i);
		if (!pTest) {
			break;
		}

		if (strcmp(pTest->listener_friendly_name, pListener->friendly_name) == 0) {
			AVB_LOGF_DEBUG("Saved state available for listener_id=%s, flags=0x%04x, talker_unique_id=0x%04x, talker_entity_id=" ENTITYID_FORMAT ", controller_entity_id=" ENTITYID_FORMAT,
					pListener->friendly_name,
					pTest->flags,
					pTest->talker_unique_id,
					ENTITYID_ARGS(pTest->talker_entity_id),
					ENTITYID_ARGS(pTest->controller_entity_id));
			if (p_flags) {
				*p_flags = pTest->flags;
			}
			if (p_talker_unique_id) {
				*p_talker_unique_id = pTest->talker_unique_id;
			}
			if (p_talker_entity_id) {
				memcpy(*p_talker_entity_id, pTest->talker_entity_id, 8);
			}
			if (p_controller_entity_id) {
				memcpy(*p_controller_entity_id, pTest->controller_entity_id, 8);
			}
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return true;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
	return false;
}
