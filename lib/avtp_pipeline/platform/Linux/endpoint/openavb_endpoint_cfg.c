/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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
* MODULE SUMMARY : Reads the .ini file for an enpoint and
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "openavb_endpoint.h"
#include "openavb_endpoint_cfg.h"
#include "openavb_trace.h"
#include "openavb_rawsock.h"
#include "ini.h"

#define	AVB_LOG_COMPONENT	"Endpoint"
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
	
	openavb_endpoint_cfg_t *pCfg = (openavb_endpoint_cfg_t*)user;
	
	AVB_LOGF_DEBUG("name=[%s] value=[%s]", name, value);

	bool valOK = FALSE;
	char *pEnd;

	if (MATCH(section, "network"))
	{
		if (MATCH(name, "ifname"))
		{
			if_info_t ifinfo;
			if (openavbCheckInterface(value, &ifinfo)) {
				strncpy(pCfg->ifname, value, IFNAMSIZ - 1);
				memcpy(pCfg->ifmac, &ifinfo.mac, ETH_ALEN);
				pCfg->ifindex = ifinfo.index;
				pCfg->mtu = ifinfo.mtu;
				valOK = TRUE;
			}
		}
		else if (MATCH(name, "link_kbit")) {
			errno = 0;
			pCfg->link_kbit = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0)
				valOK = TRUE;
		}
		else if (MATCH(name, "nsr_kbit")) {
			errno = 0;
			pCfg->nsr_kbit = strtoul(value, &pEnd, 10);
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
	else if (MATCH(section, "ptp"))
	{
		if (MATCH(name, "start_options")) {
			pCfg->ptp_start_opts = strdup(value);
			valOK = TRUE;
		}
		else {
			// unmatched item, fail
			AVB_LOGF_ERROR("Unrecognized configuration item: section=%s, name=%s", section, name);
			AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
			return 0;
		}
	}
	else if (MATCH(section, "fqtss"))
	{
		if (MATCH(name, "mode")) {
			errno = 0;
			pCfg->fqtss_mode = strtoul(value, &pEnd, 10);
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
	else if (MATCH(section, "srp"))
	{
		if (MATCH(name, "preconfigured")) {
			errno = 0;
			unsigned temp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0) {
				valOK = TRUE;
				if (temp == 1)
					pCfg->noSrp = TRUE;
				else
					pCfg->noSrp = FALSE;
			}
		}
		else if (MATCH(name, "gptp_asCapable_not_required")) {
			errno = 0;
			unsigned temp = strtoul(value, &pEnd, 10);
			if (*pEnd == '\0' && errno == 0) {
				valOK = TRUE;
				if (temp == 1)
					pCfg->bypassAsCapableCheck = TRUE;
				else
					pCfg->bypassAsCapableCheck = FALSE;
			}
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
int openavbReadConfig(const char *ini_file, openavb_endpoint_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	// defaults - most are handled by setting everything to 0
	memset(pCfg, 0, sizeof(openavb_endpoint_cfg_t));
	pCfg->fqtss_mode = -1;

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
void openavbUnconfigure(openavb_endpoint_cfg_t *pCfg)
{
	AVB_TRACE_ENTRY(AVB_TRACE_ENDPOINT);

	if (pCfg) {
		if (pCfg->ptp_start_opts) {
			free(pCfg->ptp_start_opts);
			pCfg->ptp_start_opts = NULL;
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_ENDPOINT);
}
