#ifndef MAAP_IFACE_H
#define MAAP_IFACE_H

#include <stdint.h>

/* MAAP Request Format
 *
 * This is the format of the request the driver expects to receive from the
 * Host application via a write().
 */

typedef enum {
  MAAP_CMD_INVALID = 0,
  MAAP_CMD_INIT,
  MAAP_CMD_RESERVE,
  MAAP_CMD_RELEASE,
  MAAP_CMD_STATUS,
  MAAP_CMD_EXIT,
} Maap_Cmd_Tag;

typedef struct {
  Maap_Cmd_Tag kind;
  int32_t  id;
  uint64_t start;
  uint32_t count;
} Maap_Cmd;


typedef enum {
  MAAP_NOTIFY_INVALID = 0,
  MAAP_NOTIFY_INITIALIZED,
  MAAP_NOTIFY_ACQUIRED,
  MAAP_NOTIFY_RELEASED,
  MAAP_NOTIFY_STATUS,
  MAAP_NOTIFY_YIELDED,
} Maap_Notify_Tag;

typedef enum {
  MAAP_NOTIFY_ERROR_NONE = 0, /**< Command was successful */
  MAAP_NOTIFY_ERROR_REQUIRES_INITIALIZATION, /**< MAAP is not initialized, so the command cannot be performed */
  MAAP_NOTIFY_ERROR_ALREADY_INITIALIZED, /**< MAAP is already initialized, so the values cannot be changed */
  MAAP_NOTIFY_ERROR_RESERVE_NOT_AVAILABLE, /**< The MAAP reservation is not available */
  MAAP_NOTIFY_ERROR_RELEASE_INVALID_ID, /**< The MAAP reservation ID is not valid, so cannot be released */
  MAAP_NOTIFY_ERROR_OUT_OF_MEMORY, /**< The MAAP application is out of memory */
  MAAP_NOTIFY_ERROR_INTERNAL, /**< The MAAP application experienced an internal error */
} Maap_Notify_Error;

typedef struct {
  Maap_Notify_Tag kind;
  int32_t  id;
  uint64_t start;
  uint32_t count;
  Maap_Notify_Error result;
} Maap_Notify;

#endif
