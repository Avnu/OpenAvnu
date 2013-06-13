/******************************************************************************

  Copyright (c) 2012, AudioScience, Inc
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

#ifndef _MRPDHELPER_H_
#define _MRPDHELPER_H_

/**************************************************************************
This module provides helper functions for communicating with the MRPD daemon.
***************************************************************************/

enum mrpdhelper_attribtype {
	mrpdhelper_attribtype_null,
	mrpdhelper_attribtype_mmrp,
	mrpdhelper_attribtype_mvrp,
	mrpdhelper_attribtype_msrp_domain,
	mrpdhelper_attribtype_msrp_talker,
	mrpdhelper_attribtype_msrp_listener,
	mrpdhelper_attribtype_msrp_talker_fail,
	mrpdhelper_attribtype_msrp_listener_fail
};

enum mrpdhelper_notification {
	mrpdhelper_notification_null,
	mrpdhelper_notification_new,
	mrpdhelper_notification_join,
	mrpdhelper_notification_leave
};

enum mrpdhelper_state {
	mrpdhelper_state_null,
	mrpdhelper_state_in,
	mrpdhelper_state_leave,
	mrpdhelper_state_empty
};

enum mrpdhelper_applicant_state {
	mrpdhelper_applicant_state_null,
	mrpdhelper_applicant_state_VO,
	mrpdhelper_applicant_state_VP,
	mrpdhelper_applicant_state_VN,
	mrpdhelper_applicant_state_AN,
	mrpdhelper_applicant_state_AA,
	mrpdhelper_applicant_state_QA,
	mrpdhelper_applicant_state_LA,
	mrpdhelper_applicant_state_AO,
	mrpdhelper_applicant_state_QO,
	mrpdhelper_applicant_state_AP,
	mrpdhelper_applicant_state_QP,
	mrpdhelper_applicant_state_LO
};

struct mrpdhelper_mmrp_notify {
	uint64_t mac;
};

struct mrpdhelper_mvrp_notify {
	uint32_t vid;
};

struct mrpdhelper_msrp_domain {
	uint32_t id;
	uint32_t priority;
	uint32_t vid;
};

struct mrpdhelper_msrp_listener {
	uint32_t substate;
	uint64_t id;
};

struct mrpdhelper_msrp_talker {
	uint64_t id;
	uint64_t dest_mac;
	uint32_t vid;
	uint32_t max_frame_size;
	uint32_t max_interval_frames;
	uint32_t priority_and_rank;
	uint32_t accum_latency;
	uint64_t bridge_id;
	uint32_t failure_code;
};

struct mrpdhelper_notify {
	enum mrpdhelper_attribtype attrib;
	enum mrpdhelper_notification notify;
	enum mrpdhelper_state state;
	enum mrpdhelper_applicant_state app_state;
	uint64_t registrar;
	union {
		struct mrpdhelper_mmrp_notify m;
		struct mrpdhelper_mvrp_notify v;
		struct mrpdhelper_msrp_domain sd;
		struct mrpdhelper_msrp_listener sl;
		struct mrpdhelper_msrp_talker st;
	} u;
};

int mrpdhelper_parse_notification(char *sz,
				  size_t len, struct mrpdhelper_notify *n);

int mrpdhelper_notify_equal(struct mrpdhelper_notify *n1,
			    struct mrpdhelper_notify *n2);

int mrpdhelper_to_string(struct mrpdhelper_notify *mrp_data,
				char *sz,  size_t len);

#endif
