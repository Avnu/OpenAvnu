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
  MAAP_CMD_EXIT,    /**< Have the daemon exit */
} Maap_Cmd_Tag;

/** MAAP Command Request Format
 *
 * This is the format of the command request the daemon expects to receive from the client.
 * Not all values are used for all commands.
 */
typedef struct {
  Maap_Cmd_Tag kind; /**< Type of command to perform */
  int32_t  id;       /**< ID to use for #MAAP_CMD_RELEASE or #MAAP_CMD_STATUS */
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
                                                * Try again with a smaller address block size. */
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


/** MAAP Output Type Desired Flags */
typedef enum {
  MAAP_OUTPUT_LOGGING = 0x01,       /**< Send the results to the logging engine */
  MAAP_OUTPUT_USER_FRIENDLY = 0x02, /**< Send the results to stdout in user-friendly format */
} Maap_Output_Type;

#endif
