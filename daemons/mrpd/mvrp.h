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

struct mvrp_attribute {
	struct mvrp_attribute *prev;
	struct mvrp_attribute *next;
	uint16_t attribute;	/* 12-bit VID */
	mrp_applicant_attribute_t applicant;
	mrp_registrar_attribute_t registrar;
};

struct mvrp_database {
	struct mrp_database mrp_db;
	struct mvrp_attribute *attrib_list;
};

#define MVRP_ETYPE	0x88F5
#define MVRP_PROT_VER	0x00
/* one attribute type defined for MVRP */
#define MVRP_VID_TYPE	1

/*
 * MVRP_VID_TYPE FirstValue is a two byte value encoding the 12-bit VID
 * of interest , with attrib_len=2
 */

/* MVRP uses ThreePackedEvents for all vector encodings */
int mvrp_init(int mvrp_enable);
int mvrp_event(int event, struct mvrp_attribute *rattrib);
int mvrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client);
int mvrp_reclaim(void);
void mvrp_bye(struct sockaddr_in *client);
int mvrp_recv_msg(void);
