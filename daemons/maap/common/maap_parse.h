/*
 * maap_parse.h
 *
 *  Created on: Sep 27, 2016
 *      Author: bthomsen
 */

#ifndef COMMON_MAAP_PARSE_H_
#define COMMON_MAAP_PARSE_H_

#include "maap_iface.h"

/**
 * Parse the incoming text data for a command.
 *
 * @param buf Binary or text data to parse
 * @param mc Pointer to the Maap_Cmd to fill with the binary equivalent of the command
 *
 * @return 1 if a valid command was received, 0 otherwise.
 */
int parse_text_cmd(char *buf, Maap_Cmd *cmd);

/**
 * Parse the incoming binary or text data, and perform the specified command, if any.
 *
 * @param mc Pointer to the Maap_Client to use
 * @param buf Binary or text data to parse
 *
 * @return 1 if the exit command was received, 0 otherwise.
 */
int parse_write(Maap_Client *mc, char *buf);

#endif /* COMMON_MAAP_PARSE_H_ */
