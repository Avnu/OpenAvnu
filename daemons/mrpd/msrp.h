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

#define MSRP_ETYPE	0x22EA
#define MSRP_PROT_VER	0x00
#define MSRP_SR_PVID_DEFAULT	2

/*
 * PortMediaType = either AccessControlPort (the case for wired) or
 * Non-DMN shared medium port - we are assuming wired-LAN for this daemon
 */

#define MSRP_TALKER_ADV_TYPE	1	/* with AttributeLength = 25 */
#define MSRP_TALKER_FAILED_TYPE	2	/* with AttributeLength = 34 */
#define MSRP_LISTENER_TYPE	3	/* with AttributeLength = 8 */
#define MSRP_DOMAIN_TYPE	4	/* with AttributeLength = 4 */

/* encodings for the Listener related FourPackedEvents field */
#define MSRP_LISTENER_IGNORE	0
#define MSRP_LISTENER_ASKFAILED	1
#define MSRP_LISTENER_READY	2
#define MSRP_LISTENER_READYFAIL	3

/*
 * FirstValue structure definition for MSRP Attributes
 */
typedef struct msrpdu_listen {
	uint8_t StreamID[8];	/* MSB bytes are talker MAC address */
} msrpdu_listen_t;

#if 0
typedef struct msrpdu_talker_advertise {
	uint8_t StreamID[8];
	struct {
		uint8_t Dest_Addr[6];
		uint16_t Vlan_ID;
	} DataFrameParameters;
	struct {
		uint16_t MaxFrameSize;
		uint16_t MaxIntervalFrames;
	} TSpec;
	uint8_t PriorityAndRank;
	/*
	 * PriorityAndRank := (3-bit priority | 1 bit rank | 4-bits reserved)
	 * 'rank=0 means emergency traffic, =1 otherwise
	 */
	unsigned AccumulatedLatency;	/* unsigned 32 bit nsec latency */
} msrpdu_talker_advertise_t;
#endif

typedef struct msrpdu_talker_fail {
	uint8_t StreamID[8];
	struct {
		uint8_t Dest_Addr[6];
		uint16_t Vlan_ID;
	} DataFrameParameters;
	struct {
		uint16_t MaxFrameSize;
		uint16_t MaxIntervalFrames;
	} TSpec;
	uint8_t PriorityAndRank;
	unsigned AccumulatedLatency;
	struct {
		uint8_t BridgeID[8];
		uint8_t FailureCode;
	} FailureInformation;
} msrpdu_talker_fail_t;

/* Failure Code definitions */

#define MSRP_FAIL_BANDWIDTH	1
#define MSRP_FAIL_BRIDGE	2
#define MSRP_FAIL_TC_BANDWIDTH	3
#define MSRP_FAIL_ID_BUSY	4
#define MSRP_FAIL_DSTADDR_BUSY	5
#define MSRP_FAIL_PREEMPTED	6
#define MSRP_FAIL_LATENCY_CHNG	7
#define MSRP_FAIL_PORT_NOT_AVB	8
#define MSRP_FAIL_DSTADDR_FULL	9
#define MSRP_FAIL_MSRP_RESOURCE	10
#define MSRP_FAIL_MMRP_RESOURCE	11
#define MSRP_FAIL_DSTADDR_FAIL	12
#define MSRP_FAIL_PRIO_NOT_SR	13
#define MSRP_FAIL_FRAME_SIZE	14
#define MSRP_FAIL_FANIN_EXCEED	15
#define MSRP_FAIL_STREAM_CHANGE	16
#define MSRP_FAIL_VLAN_BLOCKED	17
#define MSRP_FAIL_VLAN_DISABLED	18
#define MSRP_FAIL_SR_PRIO_ERR	19

/* Domain Discovery FirstValue definition */
typedef struct msrpdu_domain {
	uint8_t SRclassID;
	uint8_t SRclassPriority;
	uint8_t neighborSRclassPriority;
	uint16_t SRclassVID;
} msrpdu_domain_t;

/* Class ID defitions */
#define MSRP_SR_CLASS_A	6
#define MSRP_SR_CLASS_B	5

/* default values for class priorities */
#define MSRP_SR_CLASS_A_PRIO	3
#define MSRP_SR_CLASS_B_PRIO	2

#define MSRP_DIRECTION_TALKER	0
#define MSRP_DIRECTION_LISTENER	1

struct msrp_attribute {
	struct msrp_attribute *prev;
	struct msrp_attribute *next;
	uint32_t type;
	union {
		msrpdu_talker_fail_t talk_listen;
		msrpdu_domain_t domain;
	} attribute;
	uint32_t substate;	/*for listener events */
	uint32_t direction;	/*for listener events */
	mrp_applicant_attribute_t applicant;
	mrp_registrar_attribute_t registrar;
};

struct msrp_database {
	struct mrp_database mrp_db;
	struct msrp_attribute *attrib_list;
	int send_empty_LeaveAll_flag;
};

int msrp_init(int msrp_enable);
void msrp_reset(void);
int msrp_event(int event, struct msrp_attribute *rattrib);
int msrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client);
int msrp_send_notifications(struct msrp_attribute *attrib, int notify);
int msrp_reclaim(void);
void msrp_bye(struct sockaddr_in *client);
int msrp_recv_msg(void);

#ifdef MRP_CPPUTEST
struct msrp_attribute *msrp_lookup(struct msrp_attribute *rattrib);
#endif
