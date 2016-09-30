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

  if (argc == 2)
  {
    if (strncmp(argv[0], "init", 4) == 0) {
      cmd->kind = MAAP_INIT;
      cmd->param.range_info = strtoull(argv[1], NULL, 16);
      set_cmd = 1;
    } else if (strncmp(argv[0], "reserve", 7) == 0) {
      cmd->kind = MAAP_RESERVE;
      cmd->param.length = (uint16_t)strtoul(argv[1], NULL, 0);
      set_cmd = 1;
    } else if (strncmp(argv[0], "release", 7) == 0) {
      cmd->kind = MAAP_RELEASE;
      cmd->param.id = (int)strtoul(argv[1], NULL, 0);
      set_cmd = 1;
    } else {
      printf("Invalid command type\n");
    }
  }

  if (!set_cmd)
  {
    printf("input usage:  init|reserve|release <value>\n");
    return 0;
  }

  return 1;
}

void parse_write(Maap_Client *mc, char *buf) {
  Maap_Cmd *bufcmd, cmd;
  int rv = 0;

  bufcmd = (Maap_Cmd *)buf;

  switch (bufcmd->kind) {
  case MAAP_INIT:
  case MAAP_RESERVE:
  case MAAP_RELEASE:
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
    case MAAP_INIT:
      printf("Got cmd maap_init_client, range_info: %016llx\n", (unsigned long long)cmd.param.range_info);
      rv = maap_init_client(mc, cmd.param.range_info);
      break;
    case MAAP_RESERVE:
      printf("Got cmd maap_reserve_range, length: %u\n", (unsigned)cmd.param.length);
      rv = maap_reserve_range(mc, cmd.param.length);
      break;
    case MAAP_RELEASE:
      printf("Got cmd maap_release_range, id: %d\n", cmd.param.id);
      rv = maap_release_range(mc, cmd.param.id);
      break;
    default:
      printf("Error parsing in parse_write\n");
      rv = 0;
      break;
    }
  }
}
