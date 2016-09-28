/*
 * maap_parse.c
 *
 *  Created on: Sep 27, 2016
 *      Author: bthomsen
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "maap.h"
#include "maap_parse.h"

int parse_text_cmd(char *buf, Maap_Cmd *cmd) {
  char *argv[40];
  int argc = 0;
  char *p, opt;
  int set_cmd = 0, set_data = 0;

  argv[argc++] = "maap-app";
  p = strtok(buf, " \r\n");
  while ((p != NULL) && (argc < 40)) {
    argv[argc++] = p;
    p = strtok(NULL, " \r\n");
  }

  optind = 1;

  while (!set_cmd || !set_data) {
    opt = getopt(argc, argv, "c:m:l:i:");
    switch (opt) {
    case 'c':
      if (set_cmd) {
        printf("Only one command type at a time\n");
        return 0;
      }
      if (strncmp(optarg, "init", 4) == 0) {
        cmd->kind = MAAP_INIT;
        set_cmd = 1;
      } else if (strncmp(optarg, "reserve", 7) == 0) {
        cmd->kind = MAAP_RESERVE;
        set_cmd = 1;
      } else if (strncmp(optarg, "release", 7) == 0) {
        cmd->kind = MAAP_RELEASE;
        set_cmd = 1;
      } else {
        printf("Invalid command type\n");
        return 0;
      }
      break;
    case 'm':
      if (set_data) {
        printf("Only one data argument can be used at a time\n");
        return 0;
      }
      cmd->param.range_info = strtoull(optarg, NULL, 16);
      set_data = 1;
      break;
    case 'l':
      if (set_data) {
        printf("Only one data argument can be used at a time\n");
        return 0;
      }
      cmd->param.length = (uint16_t)strtoul(optarg, NULL, 0);
      set_data = 1;
      break;
    case 'i':
      if (set_data) {
        printf("Only one data argument can be used at a time\n");
        return 0;
      }
      cmd->param.id = (int)strtoul(optarg, NULL, 0);
      set_data = 1;
      break;
    }
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
