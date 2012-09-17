/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
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

struct msrp_attribute {
	struct msrp_attribute		*prev;
	struct msrp_attribute		*next;
	u_int32_t			type;
	union {
		msrpdu_talker_fail_t		talk_listen;
		msrpdu_domain_t			domain;
	} attribute;
	u_int32_t			substate;	/*for listener events*/
	u_int32_t			direction;	/*for listener events*/
	mrp_applicant_attribute_t	applicant;
	mrp_registrar_attribute_t	registrar;
};

struct msrp_database {
	struct mrp_database	mrp_db;
	struct msrp_attribute	*attrib_list;
};

int msrp_init(int msrp_enable);
int msrp_event(int event, struct msrp_attribute *rattrib);
int msrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client);
int msrp_send_notifications(struct msrp_attribute *attrib, int notify);
int msrp_reclaim(void);
void msrp_bye(struct sockaddr_in *client);
int msrp_recv_msg(void);
