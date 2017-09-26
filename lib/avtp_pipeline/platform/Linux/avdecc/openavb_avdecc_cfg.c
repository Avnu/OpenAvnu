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
* MODULE SUMMARY : Reads the AVDECC .ini file for an endpoint
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "openavb_avdecc_cfg.h"
#include "openavb_trace.h"
#include "openavb_rawsock.h"
#include "ini.h"

#define	AVB_LOG_COMPONENT	"AVDECC Cfg"
#include "openavb_log.h"

// macro to make matching names easier
#define MATCH(A, B)(strcasecmp((A), (B)) == 0)
#define MATCH_FIRST(A, B)(strncasecmp((A), (B), strlen(B)) == 0)

static void cfgValErr(const char *section, const char *name, const char *value)
{
	AVB_LOGF_ERROR("Invalid value: section=%s, name=%s, value=%s",
				   section, name, value);
}

static int cfgCallback(void *user, const char *section, const char *name, const char *value)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (!user || !section || !name || !value) {
		AVB_LOG_ERROR("Config: invalid arguments passed to callback");
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return 0;
	}

	openavb_avdecc_cfg_t *pCfg = (openavb_avdecc_cfg_t*)user;

	AVB_LOGF_DEBUG("name=[%s] value=[%s]", name, value);

	bool valOK = FALSE;
	char *pEnd;

	if (MATCH(section, "network"))
	{
		if (MATCH(name, "ifname"))
		{
			if_info_t ifinfo;
			if (openavbCheckInterface(value, &ifinfo)) {
				strncpy(pCfg->ifname, value, sizeof(pCfg->ifname) - 1);
				memcpy(pCfg->ifmac, &ifinfo.mac, ETH_ALEN);
				valOK = TRUE;
			}
		}
	}
	else if (MATCH(section, "vlan"))
	{
		if (MATCH(name, "vlanID")) {
			errno = 0;
			pCfg->vlanID = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0)
				valOK = TRUE;
		}
		else if (MATCH(name, "vlanPCP")) {
			errno = 0;
			pCfg->vlanPCP = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0)
				valOK = TRUE;
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else if (MATCH(section, "fast_connect"))
	{
		if (MATCH(name, "fast_connect")) {
			errno = 0;
			pCfg->bFastConnectSupported = (strtoul(value, &pEnd, 10) != 0);
			if (*pEnd == '\0' && errno == 0)
				valOK = TRUE;
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else if (MATCH(section, "discovery"))
	{
		if (MATCH(name, "valid_time")) {
			errno = 0;
			pCfg->valid_time = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0) {
				if (pCfg->valid_time < 2 || pCfg->valid_time > 62) {
					AVB_LOG_ERROR("valid_time must be between 2 and 62 (inclusive)");
					AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
					return 0;
				}
				if ((pCfg->valid_time & 1) != 0) {
					AVB_LOGF_WARNING("valid_time converted to an even number (%d to %d)", pCfg->valid_time, pCfg->valid_time - 1);
				}
				pCfg->valid_time /= 2; // Convert from seconds to 2-second units.
				valOK = TRUE;
			}
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else if (MATCH(section, "descriptor_entity"))
	{
		if (MATCH(name, "avdeccId")) {
			errno = 0;
			pCfg->avdeccId = strtoul(value, &pEnd, 0);
			if (*pEnd == '\0' && errno == 0)
				valOK = TRUE;
		}
		else if (MATCH(name, "entity_model_id")) {
			errno = 0;
			U64 nTemp = strtoull(value, &pEnd, 0);
			if (*pEnd == '\0' && errno == 0) {
				valOK = TRUE;
				int i;
				for (i = 7; i >= 0; --i) {
					pCfg->entity_model_id[i] = (U8) (nTemp & 0xFF);
					nTemp = (nTemp >> 8);
				}
				AVB_LOGF_DEBUG("entity_model_id = " ENTITYID_FORMAT, ENTITYID_ARGS(pCfg->entity_model_id));
			}
		}
		else if (MATCH(name, "entity_name"))
		{
			strncpy(pCfg->entity_name, value, sizeof(pCfg->entity_name));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else if (MATCH(name, "firmware_version"))
		{
			strncpy(pCfg->firmware_version, value, sizeof(pCfg->firmware_version));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else if (MATCH(name, "group_name"))
		{
			strncpy(pCfg->group_name, value, sizeof(pCfg->group_name));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else if (MATCH(name, "serial_number"))
		{
			strncpy(pCfg->serial_number, value, sizeof(pCfg->serial_number));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else if (MATCH(section, "localization"))
	{
		if (MATCH(name, "locale_identifier"))
		{
			strncpy(pCfg->locale_identifier, value, sizeof(pCfg->locale_identifier));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else if (MATCH(name, "vendor_name"))
		{
			strncpy(pCfg->vendor_name, value, sizeof(pCfg->vendor_name));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else if (MATCH(name, "model_name"))
		{
			strncpy(pCfg->model_name, value, sizeof(pCfg->model_name));
				// String does not need to be 0-terminated.
			valOK = TRUE;
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else {
		// unmatched item, fail
		AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return 0;
	}

	if (!valOK) {
		cfgValErr(section, name, value);
		AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
		return 0;
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
	return 1; // OK
}

// Parse ini file, and create config data
//
int openavbReadAvdeccConfig(const char *ini_file, openavb_avdecc_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	// defaults - most are handled by setting everything to 0
	memset(pCfg, 0, sizeof(openavb_avdecc_cfg_t));
	pCfg->valid_time = 31; // See IEEE Std 1722.1-2013 clause 6.2.1.6
	pCfg->avdeccId = 0xfffe;

	int result = ini_parse(ini_file, cfgCallback, pCfg);
	if (result < 0) {
		AVB_LOGF_ERROR("Couldn't parse INI file: %s", ini_file);
		return -1;
    }
	if (result > 0) {
		AVB_LOGF_ERROR("Error in INI file: %s, line %d", ini_file, result);
		return -1;
    }

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);

	// Yay, we did it.
	return 0;
}

// Clean up any configuration-related stuff
//
void openavbAvdeccUnconfigure(openavb_avdecc_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}
