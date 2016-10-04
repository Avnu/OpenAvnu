#ifndef MAAP_IFACE_H
#define MAAP_IFACE_H

#include <stdint.h>

/* MAAP Request Format
 *
 * This is the format of the request the driver expects to receive from the
 * Host application via a write().
 */

typedef enum {
  MAAP_INVALID = 0,
  MAAP_INIT,
  MAAP_RESERVE,
  MAAP_RELEASE,
  MAAP_EXIT,
} Maap_Cmd_Tag;

typedef struct {
  Maap_Cmd_Tag kind;
  union {
    uint64_t range_info;
    uint16_t length;
    int id;
  } param;
} Maap_Cmd;

typedef enum {
  MAAP_ACQUIRED,
  MAAP_YIELDED,
} Maap_Notify_Tag;

typedef struct {
  Maap_Notify_Tag kind;
  int id;
  uint64_t start;
  uint16_t count;
} Maap_Notify;

#endif
