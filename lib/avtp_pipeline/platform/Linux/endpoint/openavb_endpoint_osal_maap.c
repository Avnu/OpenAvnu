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

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "openavb_platform.h"
#include "openavb_trace.h"
#include "openavb_maap.h"
//#include "openavb_list.h"
//#include "openavb_rawsock.h"

#define	AVB_LOG_COMPONENT	"Endpoint MAAP"
//#define AVB_LOG_LEVEL AVB_LOG_LEVEL_DEBUG
#include "openavb_pub.h"
#include "openavb_log.h"


/*******************************************************************************
 * MAAP proxies
 ******************************************************************************/

// OPENAVB_TODO: Real implementation should be added once OpenAVB's MAAP daemon is finished

typedef struct {
	U8 destAddr[ETH_ALEN];
	bool taken;
} maapAlloc_t;

maapAlloc_t maapAllocList[MAX_AVB_STREAMS];

bool openavbMaapInitialize(const char *ifname, openavbMaapRestartCb_t* cbfn)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);
	int i = 0;
	U8 destAddr[ETH_ALEN] = {0x91, 0xe0, 0xf0, 0x00, 0x0e, 0x80};
	for (i = 0; i < MAX_AVB_STREAMS; i++) {
		memcpy(maapAllocList[i].destAddr, destAddr, ETH_ALEN);
		maapAllocList[i].taken = false;
		destAddr[5] += 1;
	}
	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
	return true;
}

void openavbMaapFinalize()
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);
	AVB_TRACE_EXIT(AVB_TRACE_MAAP);

}

void* openavbMaapAllocate(int count, /* out */ struct ether_addr *addr)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);
	assert(count == 1);

	int i = 0;
	while (i < MAX_AVB_STREAMS && maapAllocList[i].taken) {
		i++;
	}

	if (i < MAX_AVB_STREAMS) {
		maapAllocList[i].taken = true;
		memcpy(addr, maapAllocList[i].destAddr, ETH_ALEN);

		AVB_TRACE_EXIT(AVB_TRACE_MAAP);
		return &maapAllocList[i];
	}

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
	return NULL;
}

void openavbMaapRelease(void* handle)
{
	AVB_TRACE_ENTRY(AVB_TRACE_MAAP);

	maapAlloc_t *elem = handle;
	elem->taken = false;

	AVB_TRACE_EXIT(AVB_TRACE_MAAP);
}
