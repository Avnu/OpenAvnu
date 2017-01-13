/*************************************************************************************************************
Copyright (c) 2012-2015, Harman International Industries, Incorporated
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
#include "openavb_avdecc_read_ini_pub.h"

#define	AVB_LOG_COMPONENT	"TL INI"
#include "openavb_log.h"

#define MATCH(A, B)(strcasecmp((A), (B)) == 0)
#define MATCH_LEFT(A, B, C)(strncasecmp((A), (B), (C)) == 0)

static bool parse_mac(const char *str, cfg_mac_t *mac)
{
	memset(&mac->buffer, 0, sizeof(struct ether_addr));

	mac->mac = ether_aton_r(str, &mac->buffer);
	if (mac->mac)
		return TRUE;

	AVB_LOGF_ERROR("Failed to parse addr: %s", str);
	return FALSE;
}

// callback function - called for each name/value pair by ini parsing library
static int openavbIniCfgCallback(void *user, const char *tlSection, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

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

	if (!valOK) {
		// bad value
		AVB_LOGF_DEBUG("Unhandled configuration item: name=%s, value=%s", name, value);
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);

	return 1; // OK
}

bool openavbReadTlDataIniFile(const char *fileName, openavb_tl_data_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_TL);

	int result = ini_parse(fileName, openavbIniCfgCallback, pCfg);
	if (result < 0) {
		AVB_LOGF_ERROR("Couldn't parse INI file: %s", fileName);
		return FALSE;
	}
	if (result > 0) {
		AVB_LOGF_ERROR("Error in INI file: %s, line %d", fileName, result);
		return FALSE;
	}

	AVB_TRACE_EXIT(AVB_TRACE_TL);
	return TRUE;
}
