/*
 * maap_parse.c
 *
 *  Created on: Sep 27, 2016
 *      Author: bthomsen
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "maap.h"
#include "maap_parse.h"

/* Uncomment the DEBUG_CMD_MSG define to display command debug messages. */
/* #define DEBUG_CMD_MSG */


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
    } else if (strncmp(argv[0], "reserve", 7) == 0 && argc == 2) {
      cmd->kind = MAAP_CMD_RESERVE;
      cmd->count = strtoul(argv[1], NULL, 0);
      set_cmd = 1;
    } else if (strncmp(argv[0], "release", 7) == 0 && argc == 2) {
      cmd->kind = MAAP_CMD_RELEASE;
      cmd->id = (int)strtoul(argv[1], NULL, 0);
      set_cmd = 1;
    } else if (strncmp(argv[0], "status", 7) == 0 && argc == 2) {
      cmd->kind = MAAP_CMD_STATUS;
      cmd->id = (int)strtoul(argv[1], NULL, 0);
      set_cmd = 1;
    } else if (strncmp(argv[0], "exit", 4) == 0 && argc == 1) {
      cmd->kind = MAAP_CMD_EXIT;
      set_cmd = 1;
    } else {
      printf("Invalid command type\n");
    }
  }

  if (!set_cmd)
  {
    printf("input usage:\n");
    printf("    init [<range_base> <range_size>] - Initialize the MAAP daemon to recognize the specified range of addresses\n");
    printf("        If not specified, range_base=0x%llx, range_size=0x%04x\n", MAAP_DYNAMIC_POOL_BASE, MAAP_DYNAMIC_POOL_SIZE);
    printf("    reserve <addr_size> - Reserve a range of addresses of size <addr_size> in the initialized range\n");
    printf("    release <id> - Release the range of addresses with identifier ID\n");
    printf("    status <id> - Get the range of addresses associated with identifier ID\n");
    printf("    exit - Shutdown the MAAP daemon\n\n");
    return 0;
  }

  return 1;
}

int parse_write(Maap_Client *mc, const void *sender, char *buf) {
  Maap_Cmd *bufcmd, cmd;
  int rv = 0;
  int retVal = 0;

  bufcmd = (Maap_Cmd *)buf;

  switch (bufcmd->kind) {
  case MAAP_CMD_INIT:
  case MAAP_CMD_RESERVE:
  case MAAP_CMD_RELEASE:
  case MAAP_CMD_STATUS:
  case MAAP_CMD_EXIT:
    memcpy(&cmd, bufcmd, sizeof (Maap_Cmd));
    rv = 1;
    break;
  default:
    bzero(&cmd, sizeof (Maap_Cmd));
    rv = parse_text_cmd(buf, &cmd);
    break;
  }

  if (rv) {
    switch(cmd.kind) {
    case MAAP_CMD_INIT:
#ifdef DEBUG_CMD_MSG
      printf("Got cmd MAAP_CMD_INIT, range_base: 0x%016llx, range_size: 0x%04x\n",
             (unsigned long long)cmd.start, cmd.count);
#endif
      rv = maap_init_client(mc, sender, cmd.start, cmd.count);
      break;
    case MAAP_CMD_RESERVE:
#ifdef DEBUG_CMD_MSG
      printf("Got cmd MAAP_CMD_RESERVE, length: %u\n", (unsigned) cmd.count);
#endif
      rv = maap_reserve_range(mc, sender, cmd.count);
      break;
    case MAAP_CMD_RELEASE:
#ifdef DEBUG_CMD_MSG
      printf("Got cmd MAAP_CMD_RELEASE, id: %d\n", (int) cmd.id);
#endif
      rv = maap_release_range(mc, sender, cmd.id);
      break;
    case MAAP_CMD_STATUS:
#ifdef DEBUG_CMD_MSG
      printf("Got cmd MAAP_CMD_STATUS, id: %d\n", (int) cmd.id);
#endif
      maap_range_status(mc, sender, cmd.id);
      break;
    case MAAP_CMD_EXIT:
#ifdef DEBUG_CMD_MSG
      printf("Got cmd MAAP_CMD_EXIT\n");
#endif
      retVal = 1; /* Indicate that we should exit. */
      break;
    default:
      printf("Error parsing in parse_write\n");
      rv = 0;
      break;
    }
  }

  return retVal;
}
