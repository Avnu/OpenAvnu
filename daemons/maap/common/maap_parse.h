/**
 * @file
 *
 * @brief Support for parsing and handling commands
 *
 * These functions convert a text string into a Maap_Cmd structure,
 * and perform the supplied commands.
 */

#ifndef MAAP_PARSE_H_
#define MAAP_PARSE_H_

#include "maap_iface.h"

/**
 * Parse the incoming text data for a command.
 *
 * @param buf Binary or text data to parse
 * @param cmd Pointer to the Maap_Cmd to fill with the binary equivalent of the command
 *
 * @return 1 if a valid command was received, 0 otherwise.
 */
int parse_text_cmd(char *buf, Maap_Cmd *cmd);

/**
 * Parse the incoming binary or text data, and perform the specified command, if any.
 *
 * @param mc Pointer to the Maap_Client structure to use
 * @param sender Sender information pointer used to track the entity requesting the command
 * @param buf Binary or text data to parse
 *
 * @return 1 if the exit command was received, 0 otherwise.
 */
int parse_write(Maap_Client *mc, const void *sender, char *buf);

#endif /* MAAP_PARSE_H_ */
