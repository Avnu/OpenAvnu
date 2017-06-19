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
 * @brief Interface for inter-process commands and notifications
 *
 * The commands and notifications are used to allow a client to tell the daemon
 * which command to perform, and provide notifications when the status relevant
 * to a client has changed.  The commands and notifications are asynchronous.
 */

#ifndef MAAP_IFACE_H
#define MAAP_IFACE_H

#include <stdint.h>

/** MAAP Commands */
typedef enum {
	MAAP_CMD_INVALID = 0,
	MAAP_CMD_INIT,    /**< Initialize the daemon to support a range of values */
	MAAP_CMD_RESERVE, /**< Preserve a block of addresses within the initialized range */
	MAAP_CMD_RELEASE, /**< Release a previously-reserved block of addresses */
	MAAP_CMD_STATUS,  /**< Return the block of reserved addresses associated with the supplied ID */
	MAAP_CMD_YIELD,   /**< Yield a previously-reserved block of addresses.  This is only useful for testing. */
	MAAP_CMD_EXIT,    /**< Have the daemon exit */
 } Maap_Cmd_Tag;

/** MAAP Command Request Format
 *
 * This is the format of the command request the daemon expects to receive from the client.
 * Not all values are used for all commands.
 */
typedef struct {
	Maap_Cmd_Tag kind; /**< Type of command to perform */
	int32_t  id;       /**< ID to use for #MAAP_CMD_RELEASE, #MAAP_CMD_STATUS, or #MAAP_CMD_YIELD */
	uint64_t start;    /**< Address range start for #MAAP_CMD_INIT */
	uint32_t count;    /**< Address range size for #MAAP_CMD_INIT, or address block size for #MAAP_CMD_RESERVE */
} Maap_Cmd;


/** MAAP Notifications */
typedef enum {
	MAAP_NOTIFY_INVALID = 0,
	MAAP_NOTIFY_INITIALIZED, /**< Notification sent in response to a #MAAP_CMD_INIT command */
	MAAP_NOTIFY_ACQUIRING,   /**< Notification sent in response to a #MAAP_CMD_RESERVE command indicating that the reservation is in progress */
	MAAP_NOTIFY_ACQUIRED,    /**< Notification sent in response to a #MAAP_CMD_RESERVE command indicating that the reservation is complete (either succeeded or failed) */
	MAAP_NOTIFY_RELEASED,    /**< Notification sent in response to a #MAAP_CMD_RELEASE command */
	MAAP_NOTIFY_STATUS,      /**< Notification sent in response to a #MAAP_CMD_STATUS command */
	MAAP_NOTIFY_YIELDED,     /**< Notification that an address block was yielded to another device on the network */
} Maap_Notify_Tag;

/** MAAP Notification Errors */
typedef enum {
	MAAP_NOTIFY_ERROR_NONE = 0,                /**< Command was successful */
	MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION, /**< MAAP is not initialized, so the command cannot be performed */
	MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED,     /**< MAAP is already initialized, so the values cannot be changed */
	MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE,   /**< The MAAP reservation is not available, or yield cannot allocate a replacement block.
                                                *   Try again with a smaller address block size. */
	MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID,      /**< The MAAP reservation ID is not valid, so cannot be released or report its status */
	MAAP_NOTIFY_ERROR_OUT_OF_MEMORY,           /**< The MAAP application is out of memory */
	MAAP_NOTIFY_ERROR_INTERNAL,                /**< The MAAP application experienced an internal error */
} Maap_Notify_Error;

/** MAAP Notification Response Format
 *
 * This is the format of the notification response the client expects to receive from the daemon.
 * Not all values are used for all notifications.
 */
typedef struct {
	Maap_Notify_Tag kind;     /**< Type of notification being returned */
	int32_t  id;              /**< ID assigned for #MAAP_NOTIFY_ACQUIRING and #MAAP_NOTIFY_ACQUIRED, or used for #MAAP_NOTIFY_RELEASED,
                               * #MAAP_NOTIFY_STATUS, or #MAAP_NOTIFY_YIELDED */
	uint64_t start;           /**< Address range start for #MAAP_NOTIFY_INITIALIZED, or address block start for #MAAP_NOTIFY_ACQUIRING,
                               * #MAAP_NOTIFY_ACQUIRED, #MAAP_NOTIFY_RELEASED, #MAAP_NOTIFY_STATUS, or #MAAP_NOTIFY_YIELDED */
	uint32_t count;           /**< Address range size for #MAAP_NOTIFY_INITIALIZED, or address block size for #MAAP_NOTIFY_ACQUIRING,
                               * #MAAP_NOTIFY_ACQUIRED, #MAAP_NOTIFY_RELEASED, #MAAP_NOTIFY_STATUS, or #MAAP_NOTIFY_YIELDED */
	Maap_Notify_Error result; /**< #MAAP_NOTIFY_ERROR_NONE if the command succeeded, or another value if an error occurred */
} Maap_Notify;


/** Callback function used by #print_notify and #print_cmd_usage */
typedef void (*print_notify_callback_t)(void *callback_data, int logLevel, const char *notifyText);

#endif
