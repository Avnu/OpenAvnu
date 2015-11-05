/******************************************************************************

  Copyright (c) 2012, Intel Corporation
  Copyright (c) 2014, Parrot SA
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef _TALKER_MRP_CLIENT_H_
#define _TALKER_MRP_CLIENT_H_

/*
 * simple_talker MRP client part
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <pthread.h>
#include <poll.h>

#include "mrpd.h"
#include "mrp.h"
#include "msrp.h" // spurious dep daemons/mrpd/msrp.h:50:#define MSRP_LISTENER_ASKFAILED

/* global variables */

struct mrp_talker_ctx
{
	int control_socket;
	volatile int halt_tx;
	volatile int domain_a_valid;
	int domain_class_a_id;
	int domain_class_a_priority;
	u_int16_t domain_class_a_vid;
	volatile int domain_b_valid;
	int domain_class_b_id;
	int domain_class_b_priority;
	u_int16_t domain_class_b_vid;
	unsigned char monitor_stream_id[8];
	volatile int listeners;
};

struct mrp_domain_attr
{
	int id;
	int priority;
	u_int16_t vid;
};


extern volatile int mrp_error;

/* functions */

int mrp_connect(struct mrp_talker_ctx *ctx);
int mrp_disconnect(struct mrp_talker_ctx *ctx);
int mrp_register_domain(struct mrp_domain_attr *reg_class, struct mrp_talker_ctx *ctx);
int mrp_join_vlan(struct mrp_domain_attr *reg_class, struct mrp_talker_ctx *ctx);
int mrp_advertise_stream(uint8_t * streamid, uint8_t * destaddr, int pktsz, int interval, int latency, struct mrp_talker_ctx *ctx);
int mrp_unadvertise_stream(uint8_t * streamid, uint8_t * destaddr, int pktsz, int interval, int latency, struct mrp_talker_ctx *ctx);
int mrp_await_listener(unsigned char *streamid, struct mrp_talker_ctx *ctx);
int mrp_get_domain(struct mrp_talker_ctx *ctx, struct mrp_domain_attr *class_a, struct mrp_domain_attr *class_b);
int mrp_talker_client_init(struct mrp_talker_ctx *ctx);
#endif /* _TALKER_MRP_CLIENT_H_ */
