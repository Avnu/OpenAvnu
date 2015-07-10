/*
  Copyright (c) 2013 Katja Rohloff <Katja.Rohloff@uni-jena.de>
  Copyright (c) 2014, Parrot SA

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _LISTENER_MRP_CLIENT_H_
#define _LISTENER_MRP_CLIENT_H_

/*
 * simple_listener MRP client part
 */

#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h> // TODO fprintf, to be removed
#include <unistd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>

#include "mrpd.h"
#include "mrp.h"
#include "msrp.h"

/* global variables */

// TODO move these in a talker_context struct + init func

extern int control_socket;
extern volatile int talker;
extern unsigned char stream_id[8];
extern volatile int halt_tx;

extern volatile int domain_a_valid;
extern int domain_class_a_id;
extern int domain_class_a_priority;
extern u_int16_t domain_class_a_vid;

extern volatile int domain_b_valid;
extern int domain_class_b_id;
extern int domain_class_b_priority;
extern u_int16_t domain_class_b_vid;


/* functions */

int create_socket();
int mrp_monitor(void);
int report_domain_status(int *class_id, int *priority, u_int16_t *vid);
int join_vlan(u_int16_t vid);
int await_talker();
int send_ready();
int send_leave();
int mrp_disconnect();
int mrp_get_domain(int *class_a_id, int *a_priority, u_int16_t * a_vid, int *class_b_id, int *b_priority, u_int16_t * b_vid);

#endif /* _LISTENER_MRP_CLIENT_H_ */
