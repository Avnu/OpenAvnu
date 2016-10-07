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
  MAAP_CMD_EXIT,
} Maap_Cmd_Tag;

typedef struct {
  Maap_Cmd_Tag kind;
  int32_t  id;
  uint64_t start;
  uint32_t count;
} Maap_Cmd;


typedef enum {
  MAAP_NOTIFY_INIT,
  MAAP_NOTIFY_ACQUIRED,
  MAAP_NOTIFY_YIELDED,
} Maap_Notify_Tag;

typedef struct {
  Maap_Notify_Tag kind;
  int32_t  id;
  uint64_t start;
  uint32_t count;
} Maap_Notify;

#endif
