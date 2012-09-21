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

#define MMRP_ETYPE	0x88F6
#define MMRP_PROT_VER	0x00

/* two attribute types defined for MMRP */
#define MMRP_SVCREQ_TYPE	1
#define MMRP_MACVEC_TYPE	2

/* MMRP_MACVEC_TYPE FirstValue is the 6-byte MAC address, with
 * corresponding attrib_length=6
 */

/* MMRP_SVCREQ_TYPE FirstValue is a single byte - values follow,
 * attrib_length=1
 */

#define MMRP_SVCREQ_FOWARD_ALL			0
#define MMRP_SVCREQ_FOWARD_UNREGISTERED	1
/* MMRP uses ThreePackedEvents for all vector encodings */

struct mmrp_attribute {
	struct mmrp_attribute *prev;
	struct mmrp_attribute *next;
	uint32_t type;
	union {
		unsigned char macaddr[6];
		uint8_t svcreq;
	} attribute;
	mrp_applicant_attribute_t applicant;
	mrp_registrar_attribute_t registrar;
};

struct mmrp_database {
	struct mrp_database mrp_db;
	struct mmrp_attribute *attrib_list;
};

int mmrp_init(int mmrp_enable);
int mmrp_event(int event, struct mmrp_attribute *rattrib);
int mmrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client);
int mmrp_reclaim(void);
void mmrp_bye(struct sockaddr_in *client);
int mmrp_recv_msg(void);
void mmrp_increment_macaddr(uint8_t * macaddr);
int mmrp_send_notifications(struct mmrp_attribute *attrib, int notify);
