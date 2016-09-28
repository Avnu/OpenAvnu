/*
 * maap_parse.h
 *
 *  Created on: Sep 27, 2016
 *      Author: bthomsen
 */

#ifndef COMMON_MAAP_PARSE_H_
#define COMMON_MAAP_PARSE_H_

#include "maap_iface.h"

int parse_text_cmd(char *buf, Maap_Cmd *cmd);

void parse_write(Maap_Client *mc, char *buf);

#endif /* COMMON_MAAP_PARSE_H_ */
