#ifndef MAAP_IFACE_H
#define MAAP_IFACE_H

#include <stdint.h>

#define MAAP_SOCKET_NAME "/var/tmp/maap.socket"
#define MAAP_CMD_BUF_SZ 500

/* MAAP Reqeust Format
 *
 * This is the format of the request the driver expects to receive from the
 * Host application via a write().
 */

typedef enum {
  MAAP_INIT,
  MAAP_RESERVE,
  MAAP_RELEASE,
} Cmd_Tag;

typedef struct {
  Cmd_Tag kind;
  union {
    uint64_t range_info;
    uint16_t length;
    int id;
  } param;
} Cmd;

typedef enum {
  MAAP_ACQUIRED,
  MAAP_YIELDED,
} Notify_Tag;

typedef struct {
  Notify_Tag kind;
  int id;
  uint64_t start;
  uint16_t count;
} Notify;

#endif
