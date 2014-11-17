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

// TODO move these in a talker_context struct + init func

extern volatile int halt_tx;
extern volatile int listeners;
extern volatile int mrp_error;

extern volatile int domain_a_valid;
extern int domain_class_a_id;
extern int domain_class_a_priority;
extern u_int16_t domain_class_a_vid;

extern volatile int domain_b_valid;
extern int domain_class_b_id;
extern int domain_class_b_priority;
extern u_int16_t domain_class_b_vid;

/* functions */

int mrp_connect(void);
int mrp_disconnect(void);
int mrp_monitor(void);
int mrp_register_domain(int *class_id, int *priority, u_int16_t *vid);
int mrp_join_vlan(void);
int mrp_advertise_stream(uint8_t * streamid, uint8_t * destaddr, u_int16_t vlan, int pktsz, int interval, int priority, int latency);
int mrp_unadvertise_stream(uint8_t * streamid, uint8_t * destaddr, u_int16_t vlan, int pktsz, int interval, int priority, int latency);
int mrp_await_listener(unsigned char *streamid);

#endif /* _TALKER_MRP_CLIENT_H_ */
