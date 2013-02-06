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

/******************************************************************************
This module implements code that takes the notification strings that MRPD sends
and parses them into a machine readable structure.
******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>		// for uint8_t etc
#ifdef __linux__
#include <inttypes.h>
#else
#define SCNu64       "I64u"
#define SCNx64       "I64x"
#endif

#include "mrpdhelper.h"

struct app_state_to_enum {
	char *s;
	enum mrpdhelper_applicant_state value;
} mrp_app_state_mapping[12] = {
	{"VO", mrpdhelper_applicant_state_VO},
	{"VP", mrpdhelper_applicant_state_VP},
	{"VN", mrpdhelper_applicant_state_VN},
	{"AN", mrpdhelper_applicant_state_AN},
	{"AA", mrpdhelper_applicant_state_AA},
	{"QA", mrpdhelper_applicant_state_QA},
	{"LA", mrpdhelper_applicant_state_LA},
	{"AO", mrpdhelper_applicant_state_AO},
	{"QO", mrpdhelper_applicant_state_QO},
	{"AP", mrpdhelper_applicant_state_AP},
	{"QP", mrpdhelper_applicant_state_QP},
	{"LO", mrpdhelper_applicant_state_LO}
};


static int parse_app_state(char *sz, struct mrpdhelper_notify *n)
{
	int i;

	/* loop over mrp_app_state_mapping struct */
	for (i=0; i < 12; i++) {
		if (strncmp(sz, mrp_app_state_mapping[i].s, 2) == 0) {
			n->app_state = mrp_app_state_mapping[i].value;
			break;
		}
	}
	if (n->app_state == mrpdhelper_applicant_state_null)
		return -1;
	else
		return 0;
}

static int parse_state(char *sz, struct mrpdhelper_notify *n)
{
	if (strncmp(sz, "IN", 2) == 0) {
		n->state = mrpdhelper_state_in;
	} else if (strncmp(sz, "LV", 3) == 0) {
		n->state = mrpdhelper_state_leave;
	} else if (strncmp(sz, "MT", 3) == 0) {
		n->state = mrpdhelper_state_empty;
	} else {
		return -1;
	}
	return 0;
}

static int parse_notification(char *sz, struct mrpdhelper_notify *n)
{
	if (strncmp(sz, "NE", 2) == 0) {
		n->notify = mrpdhelper_notification_new;
	} else if (strncmp(sz, "JO", 2) == 0) {
		n->notify = mrpdhelper_notification_join;
	} else if (strncmp(sz, "LE", 2) == 0) {
		n->notify = mrpdhelper_notification_leave;
	} else {
		return -1;
	}
	return 0;
}

static int parse_registrar(char *sz, struct mrpdhelper_notify *n, char **rpos)
{
	char *r;

	r = strstr(sz, "R=");
	if (rpos)
		*rpos = r;
	if (!r)
		return -1;

	if (sscanf(r, "R=%" SCNx64, &n->registrar) == 1)
		return 0;
	else
		return -1;
}

static int parse_mvrp(char *sz, size_t len, struct mrpdhelper_notify *n)
{
	/* format
	   VIN VJO 1234 R=112233445566
	   len = 28
	 */
	if (len < 28)
		return -1;

	if (parse_notification(&sz[1], n) < 0)
		return -1;

	if (parse_state(&sz[5], n) < 0)
		return -1;

	n->attrib = mrpdhelper_attribtype_mvrp;
	if (sscanf(&sz[8], "%04x ", &n->u.v.vid) != 1)
		return -1;
	return parse_registrar(sz, n, NULL);
}

static int parse_msrp_string(char *sz, size_t len, struct mrpdhelper_notify *n)
{
	int result;

	sz[len] = 0;
	switch (sz[0]) {
	case 'D':
		result = sscanf(sz, "D:C=%d,P=%d,V=%04x",
				&n->u.sd.id, &n->u.sd.priority, &n->u.sd.vid);
		if (result < 3)
			return -1;
		n->attrib = mrpdhelper_attribtype_msrp_domain;
		break;
	case 'L':
		result = sscanf(sz, "L:D=%d,S=%" SCNx64,
				&n->u.sl.substate, &n->u.sl.id);
		if (result < 2)
			return -1;
		n->attrib = mrpdhelper_attribtype_msrp_listener;
		break;
	case 'T':
		result = sscanf(sz,
				"T:S=%" SCNx64
				",A=%" SCNx64
				",V=%04x"
				",Z=%d"
				",I=%d"
				",P=%d"
				",L=%d"
				",B=%" SCNx64
				",C=%d",
				&n->u.st.id,
				&n->u.st.dest_mac,
				&n->u.st.vid,
				&n->u.st.max_frame_size,
				&n->u.st.max_interval_frames,
				&n->u.st.priority_and_rank,
				&n->u.st.accum_latency,
				&n->u.st.bridge_id, &n->u.st.failure_code);
		if (result < 9)
			return -1;
		n->attrib = mrpdhelper_attribtype_msrp_talker;
		break;
	default:
		return -1;

	}
	return 0;
}

static int parse_msrp(char *sz, size_t len, struct mrpdhelper_notify *n)
{
	if (parse_notification(&sz[1], n) < 0)
		return -1;

	if (parse_msrp_string(&sz[4], len - 4, n) < 0)
		return -1;

	return parse_registrar(sz, n, NULL);
}


static int parse_msrp_query(char *sz, size_t len, struct mrpdhelper_notify *n)
{
	char *r;

	if (parse_msrp_string(sz, len, n) < 0)
		return -1;

	if (parse_registrar(sz, n, &r) < 0)
		return -1;

	if (parse_app_state(&r[15], n) < 0)
		return -1;

	return parse_state(&r[18], n);
}

static int parse_mmrp(char *sz, size_t len, struct mrpdhelper_notify *n)
{
	/* format
	   MIN MJO M=112233445566 R=112233445566
	   len = 38
	 */
	if (len < 38)
		return -1;

	if (parse_notification(&sz[1], n) < 0)
		return -1;

	if (parse_state(&sz[5], n) < 0)
		return -1;

	n->attrib = mrpdhelper_attribtype_mvrp;
	if (sscanf(&sz[8], "M=%" SCNx64, &n->u.m.mac) != 1)
		return -1;
	return parse_registrar(sz, n, NULL);
}

int mrpdhelper_parse_notification(char *sz, size_t len,
				  struct mrpdhelper_notify *n)
{
	memset(n, 0, sizeof(*n));
	switch (sz[0]) {
	case 'V':
		return parse_mvrp(sz, len, n);
	case 'S':
		return parse_msrp(sz, len, n);
	case 'M':
		return parse_mmrp(sz, len, n);
	case 'L':
	case 'D':
	case 'T':
		return parse_msrp_query(sz, len, n);
	default:
		return -1;
	}
}

int mrpdhelper_notify_equal(struct mrpdhelper_notify *n1,
			    struct mrpdhelper_notify *n2)
{
	if (n1->attrib != n2->attrib)
		return 0;

	switch (n1->attrib) {
	case mrpdhelper_attribtype_mmrp:
		if (n1->u.m.mac != n2->u.m.mac)
			return 0;
		break;
	case mrpdhelper_attribtype_mvrp:
		if (n1->u.v.vid != n2->u.v.vid)
			return 0;
		break;
	case mrpdhelper_attribtype_msrp_domain:
		if ((n1->u.sd.id != n1->u.sd.id) ||
		    (n1->u.sd.priority != n1->u.sd.priority) ||
		    (n1->u.sd.vid != n1->u.sd.vid))
			return 0;
		break;
	case mrpdhelper_attribtype_msrp_talker:
	case mrpdhelper_attribtype_msrp_talker_fail:
		if (n1->u.st.id != n2->u.st.id)
			return 0;
		break;
	case mrpdhelper_attribtype_msrp_listener:
	case mrpdhelper_attribtype_msrp_listener_fail:
		if (n1->u.sl.id != n2->u.sl.id)
			return 0;
		break;
	default:
		return 0;
	}
	return 1;
}
