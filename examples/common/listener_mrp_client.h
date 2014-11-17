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

#include "mrpd.h"

/* global variables */

// TODO move these in a talker_context struct + init func

extern int control_socket;
extern volatile int talker;
extern unsigned char stream_id[8];

/* functions */

int create_socket();
int report_domain_status();
int join_vlan();
int await_talker();
int send_ready();
int send_leave();
int mrp_disconnect();

#endif /* _LISTENER_MRP_CLIENT_H_ */
