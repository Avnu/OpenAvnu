/*************************************************************************************
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
*************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) && (_MSC_VER < 1800)
/* Visual Studio 2012 and earlier */
#define strtoull(x,y,z) _strtoui64(x,y,z)
#endif

#include "maap.h"
#include "maap_parse.h"

#define MAAP_LOG_COMPONENT "Parse"
#include "maap_log.h"

/* Uncomment the DEBUG_CMD_MSG define to display command debug messages. */
#define DEBUG_CMD_MSG


int parse_text_cmd(char *buf, Maap_Cmd *cmd) {
	char *argv[5];
	int argc = 0;
	char *p;
	int set_cmd = 0;

	p = strtok(buf, " \r\n");
	while ((p != NULL) && (argc < sizeof(argv) / sizeof(*argv))) {
		argv[argc++] = p;
		p = strtok(NULL, " \r\n");
	}

	if (argc >= 1 && argc <= 3)
	{
		/* Give all parameters default values. */
		cmd->kind = MAAP_CMD_INVALID;
		cmd->id = -1; /* N/A */
		cmd->start = 0; /* N/A */
		cmd->count = 0; /* N/A */

		if (strncmp(argv[0], "init", 4) == 0) {
			if (argc == 1) {
				cmd->kind = MAAP_CMD_INIT;
				cmd->start = MAAP_DYNAMIC_POOL_BASE;
				cmd->count = MAAP_DYNAMIC_POOL_SIZE;
				set_cmd = 1;
			} else if (argc == 3) {
				cmd->kind = MAAP_CMD_INIT;
				cmd->start = strtoull(argv[1], NULL, 16);
				cmd->count = strtoul(argv[2], NULL, 0);
				set_cmd = 1;
			}
		} else if (strncmp(argv[0], "reserve", 7) == 0) {
			if (argc == 2) {
				cmd->kind = MAAP_CMD_RESERVE;
				cmd->count = strtoul(argv[1], NULL, 0);
				set_cmd = 1;
			} else if (argc == 3) {
				cmd->kind = MAAP_CMD_RESERVE;
				cmd->start = strtoull(argv[1], NULL, 16);
				cmd->count = strtoul(argv[2], NULL, 0);
				set_cmd = 1;
			}
		} else if (strncmp(argv[0], "release", 7) == 0 && argc == 2) {
			cmd->kind = MAAP_CMD_RELEASE;
			cmd->id = (int)strtoul(argv[1], NULL, 0);
			set_cmd = 1;
		} else if (strncmp(argv[0], "status", 7) == 0 && argc == 2) {
			cmd->kind = MAAP_CMD_STATUS;
			cmd->id = (int)strtoul(argv[1], NULL, 0);
			set_cmd = 1;
		} else if (strncmp(argv[0], "yield", 7) == 0 && argc == 2) {
			cmd->kind = MAAP_CMD_YIELD;
			cmd->id = (int)strtoul(argv[1], NULL, 0);
			set_cmd = 1;
		} else if (strncmp(argv[0], "exit", 4) == 0 && argc == 1) {
			cmd->kind = MAAP_CMD_EXIT;
			set_cmd = 1;
		}
	}

	return set_cmd;
 }

int parse_write(Maap_Client *mc, const void *sender, char *buf, int *input_is_text) {
	Maap_Cmd *bufcmd, cmd;
	int rv = 0;
	int retVal = 0;

	bufcmd = (Maap_Cmd *)buf;

	switch (bufcmd->kind) {
	case MAAP_CMD_INIT:
	case MAAP_CMD_RESERVE:
	case MAAP_CMD_RELEASE:
	case MAAP_CMD_STATUS:
	case MAAP_CMD_YIELD:
	case MAAP_CMD_EXIT:
		if (input_is_text) { *input_is_text = 0; }
		memcpy(&cmd, bufcmd, sizeof (Maap_Cmd));
		rv = 1;
		break;
	default:
		if (input_is_text) { *input_is_text = 1; }
		memset(&cmd, 0, sizeof (Maap_Cmd));
		rv = parse_text_cmd(buf, &cmd);
		if (!rv) {
			/* Unrecognized command. */
			retVal = -1;
		}
		break;
	}

	if (rv) {
		switch(cmd.kind) {
		case MAAP_CMD_INIT:
#ifdef DEBUG_CMD_MSG
			MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_INIT, range_base: 0x%016llx, range_size: 0x%04x",
							(unsigned long long)cmd.start, cmd.count);
#endif
			rv = maap_init_client(mc, sender, cmd.start, cmd.count);
			break;
		case MAAP_CMD_RESERVE:
#ifdef DEBUG_CMD_MSG
			if (cmd.start != 0) {
				MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_RESERVE, start: 0x%016llx, length: %u",
					(unsigned long long)cmd.start, (unsigned) cmd.count);
			} else {
				MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_RESERVE, length: %u", (unsigned) cmd.count);
			}
#endif
			rv = maap_reserve_range(mc, sender, cmd.start, cmd.count);
			break;
		case MAAP_CMD_RELEASE:
#ifdef DEBUG_CMD_MSG
			MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_RELEASE, id: %d", (int) cmd.id);
#endif
			rv = maap_release_range(mc, sender, cmd.id);
			break;
		case MAAP_CMD_STATUS:
#ifdef DEBUG_CMD_MSG
			MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_STATUS, id: %d", (int) cmd.id);
#endif
			maap_range_status(mc, sender, cmd.id);
			break;
		case MAAP_CMD_YIELD:
#ifdef DEBUG_CMD_MSG
			MAAP_LOGF_DEBUG("Got cmd MAAP_CMD_YIELD, id: %d", (int) cmd.id);
#endif
			rv = maap_yield_range(mc, sender, cmd.id);
			break;
		case MAAP_CMD_EXIT:
#ifdef DEBUG_CMD_MSG
			MAAP_LOG_DEBUG("Got cmd MAAP_CMD_EXIT");
#endif
			retVal = 1; /* Indicate that we should exit. */
			break;
		default:
			MAAP_LOG_ERROR("Unrecognized input to parse_write");
			retVal = -1;
			break;
		}
	}

	return retVal;
}

void parse_usage(print_notify_callback_t print_callback, void *callback_data)
{
	char szOutput[100];
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"Input usage:");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    init [<range_base> <range_size>] - Initialize the MAAP daemon to recognize");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"        the specified range of addresses.  If not specified, it uses");
	sprintf(szOutput,
		"        range_base=0x%llx, range_size=0x%04x.", MAAP_DYNAMIC_POOL_BASE, MAAP_DYNAMIC_POOL_SIZE);
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO, szOutput);
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    reserve [<addr_base>] <addr_size> - Reserve a range of addresses of size");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"        <addr_size> in the initialized range.  If <addr_base> is specified,");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"        that address base will be attempted first.");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    release <id> - Release the range of addresses with identifier ID");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    status <id> - Get the range of addresses associated with identifier ID");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    yield <id> - Yield the range of addresses associated with identifier ID.");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"        This is only useful for testing.");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		"    exit - Shutdown the MAAP daemon");
	print_callback(callback_data, MAAP_LOG_LEVEL_INFO,
		" "); // Blank line
}
