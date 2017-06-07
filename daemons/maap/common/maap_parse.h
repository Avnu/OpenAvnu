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
 * @param input_is_text Optional pointer for variable set to 1 if the input is text, 0 if the input is binary.
 *
 * @return 1 if the exit command was received, -1 for an unrecognized command, or 0 otherwise.
 */
int parse_write(Maap_Client *mc, const void *sender, char *buf, int *input_is_text);

/**
 * Prints the usage expected for parsing.
 *
 * @param print_callback Function of type #print_notify_callback_t that will handle printable results.
 * @param callback_data Data to return with the callback.
 */
void parse_usage(print_notify_callback_t print_callback, void *callback_data);

#endif /* MAAP_PARSE_H_ */
