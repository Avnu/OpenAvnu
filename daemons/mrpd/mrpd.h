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
/*
 * an endpoint implementation of 802.1Q-2011 MRP (MMRP, MVRP, MSRP)
 */

#define MRP_ENCODE_YES		0	/* must send */
#define MRP_ENCODE_OPTIONAL	1	/* send if smaller */

typedef struct mrp_applicant_attribute {
	int	mrp_state;
	int	tx;		/* tx=1 means transmit on next TX event */
	int	sndmsg;		/* sndmsg={NEW,IN,JOININ,JOINMT,MT, or LV} */
	int	encode; 	/* when tx=1, NO, YES or OPTIONAL */
} mrp_applicant_attribute_t;

typedef struct mrp_registrar_attribute {
	int	mrp_state;
	int	notify;
	short	rsvd;
	unsigned char	macaddr[6];	/* mac address of last registration */
} mrp_registrar_attribute_t;

/* MRP Application Notifications */
#define MRP_NOTIFY_NONE		0
#define MRP_NOTIFY_NEW		1
#define MRP_NOTIFY_JOIN		2
#define MRP_NOTIFY_LV		3

/* Applicant counts number of NEW, JOIN_IN and JOIN_EMPTY states sent,
 * as well as number of JOIN_IN messages received by peers. 
 * Upon receipt of LEAVE or LEAVEALL, applicant ensures at least 2 NEW, JOIN_IN
 * or JOIN_EMPTY (or JOIN_IN from peers) have been sent since the last LEAVEALL.
 */

/* Applicant FSM states */
#define MRP_VO_STATE	0	/* Very Anxious Observer */
#define MRP_VP_STATE	1	/* Very Anxious Passive */
#define MRP_VN_STATE	2	/* Very Anxious New */
#define MRP_AN_STATE	3	/* Anxious New */
#define MRP_AA_STATE	4	/* Anxious New */
#define MRP_QA_STATE	5	/* Quiet Active */
#define MRP_LA_STATE	6	/* Leaving Active */
#define MRP_AO_STATE	7	/* Anxious Observer State */
#define MRP_QO_STATE	8	/* Quiet Observer State */
#define MRP_AP_STATE	9	/* Anxious Passive State */
#define MRP_QP_STATE	10	/* Quiet Passive State */
#define MRP_LO_STATE	11	/* Leaving Observer State */

/* Registrar States */
#define MRP_IN_STATE	16	/* when Registrar state is IN */
#define MRP_LV_STATE	17 	/* registrar state - leaving */
#define MRP_MT_STATE	18 	/* whe Registrar state is MT or LV */

/* MRP Events */
#define MRP_EVENT_BEGIN	100	/*  Initialize state machine (10.7.5.1) */
#define MRP_EVENT_NEW	200	/*  A new declaration (10.7.5.4) */
#define MRP_EVENT_JOIN	300	/*  Declaration registration (10.7.5.5) */
#define MRP_EVENT_LV	400	/*  Withdraw a declaration (10.7.5.6) */
#define MRP_EVENT_TX	500	/*  Tx without LVA (10.7.5.7) */
#define MRP_EVENT_TXLA	600	/*  Tx with a LVA (10.7.5.8) */
#define MRP_EVENT_TXLAF	700	/*  Tx with a LVA, no room (Full) (10.7.5.9) */
#define MRP_EVENT_RNEW	800	/*  Rx New message (10.7.5.14) */
#define MRP_EVENT_RJOININ 900	/*  Rx JoinIn message (10.7.5.15),  */
#define MRP_EVENT_RIN	1000	/*  receive In message (10.7.5.18) */
#define MRP_EVENT_RJOINMT 1100	/*  receive JoinEmpty message (10.7.5.16) */
#define MRP_EVENT_RMT	1200	/*  receive Empty message (10.7.5.19) */
#define MRP_EVENT_RLV	1300	/*  receive Leave message (10.7.5.17) */
#define MRP_EVENT_RLA	1400	/*  receive a LeaveAll message (10.7.5.20) */ 
#define MRP_EVENT_FLUSH	1500	/*  Port role change (10.7.5.2) */
#define MRP_EVENT_REDECLARE 1600	/*  Port role changes (10.7.5.3) */
#define MRP_EVENT_PERIODIC 1700	/*  periodic timer expire */
#define MRP_EVENT_PERIODIC_ENABLE 1800	/*  periodic timer enable */
#define MRP_EVENT_PERIODIC_DISABLE 1900	/*  periodic timer disable */
#define MRP_EVENT_LVTIMER  2000	/*  leave timer expire */
#define MRP_EVENT_LVATIMER 2100	/*  leaveall timer expire */

#define MRP_SND_NEW	0	/* declare and register a new attribute from a new participant */
#define MRP_SND_JOIN	1	/* declare and register an attribute (generally) */
#define MRP_SND_IN	2
#define MRP_SND_LV	6
#define MRP_SND_LVA	7
#define MRP_SND_NULL	8	/* sent as 'ignore' to improve encoding */
#define MRP_SND_NONE	9

/* timer defaults from 802.1Q-2011, Table 10-7 */

#define MRP_JOINTIMER_VAL	200	/* join timeout in msec */
#define MRP_LVTIMER_VAL		1000	/* leave timeout in msec */
#define MRP_LVATIMER_VAL	10000	/* leaveall timeout in msec */
#define MRP_PERIODTIMER_VAL	1000	/* periodic timeout in msec */

typedef struct mrp_timer {
	int	state;
	int	tx;		/* tx=1 means transmit on next TX event */
	int	sndmsg;		/* sndmsg={NEW,JOIN,or LV}  */
} mrp_timer_t;

#define MRP_TIMER_PASSIVE	0
#define MRP_TIMER_ACTIVE	1

#define MRP_REGISTRAR_CTL_NORMAL	0
#define MRP_REGISTRAR_CTL_FIXED		1
#define MRP_REGISTRAR_CTL_FORBIDDEN	2

#define MRP_APPLICANT_CTL_NORMAL	0
#define MRP_APPLICANT_CTL_SILENT	1

#define MRPDU_ENDMARK	0x0000
#define MRPDU_ENDMARK_SZ	2

#define MRPDU_NULL_LVA	0
#define MRPDU_LVA	1
#define MRPDU_NEW	0
#define MRPDU_JOININ	1
#define MRPDU_IN	2
#define MRPDU_JOINMT	3
#define MRPDU_MT	4
#define MRPDU_LV	5

#define MRPDU_3PACK_ENCODE(x, y, z)	(((((x) * 6) + (y)) * 6) + (z))
#define MRPDU_4PACK_ENCODE(w, x, y, z)	(((w) * 64) + ((x) * 16) + \
						((y) * 4) + (z))

typedef struct mrpdu_vectorattrib {
	u_int16_t	VectorHeader;	/* LVA << 13 | NumberOfValues */
	uint8_t	FirstValue_VectorEvents[];
} mrpdu_vectorattrib_t;

#define MRPDU_VECT_NUMVALUES(x)	((x) & ((1 << 13) - 1))	
#define MRPDU_VECT_LVA(x)	((x) & (1 << 13))	

#define MAX_MRPD_CMDSZ	(1500)

typedef struct eth_hdr {
	uint8_t	destaddr[6];
	uint8_t	srcaddr[6];
	u_int16_t	typelen;
} eth_hdr_t;

/*
 * NumberOfValues is the number of EVENTS encoded in the various 
 * 3/4 PACK Bytes.  Non-multiples of (3) [ for 3-packed ] or (4) 
 * [ for 4-packed ] indicates the trailing events are transmitted 
 * as -0- and should be ignored (and NOT interpreted as an ENDMARK)
 * A value of -0- indicates there are no events to report (a NULL).
 *
 * furthermore, the events are 'vector' operations - the first encoded event
 * operates on the (FirstEvent) attribute, the second encoded event
 * operates on the (FirstEvent+1) attribute, and so forth.
 */
typedef struct mrpdu_message {
	uint8_t	AttributeType;
	uint8_t	AttributeLength;	/* length of FirstValue */
	uint8_t	Data[];
	/* parsing of the data field is application specific - either
	 * a ushort with an attribute list length followed by vector
	 * attributes, or just a list of vector attributes ...
	 */

	/* table should have a trailing NULL (0x0000) indicating the ENDMARK */
} mrpdu_message_t;

typedef struct mrpdu {
	uint8_t	ProtocolVersion;
	mrpdu_message_t	MessageList[];	
	/* mrpdu should have trailing NULL (0x0000) indicating the ENDMARK */
} mrpdu_t;

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

#define MVRP_ETYPE	0x88F5
#define MVRP_PROT_VER	0x00
/* one attribute type defined for MVRP */
#define MVRP_VID_TYPE	1

/* 
 * MVRP_VID_TYPE FirstValue is a two byte value encoding the 12-bit VID 
 * of interest , with attrib_len=2 
 */

/* MVRP uses ThreePackedEvents for all vector encodings */

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
	uint8_t	StreamID[8];	/* MSB bytes are talker MAC address */
} msrpdu_listen_t;

typedef struct msrpdu_talker_advertise {
	uint8_t	StreamID[8];
	struct {
		uint8_t	Dest_Addr[6];
		u_int16_t	Vlan_ID;
	} DataFrameParameters;
	struct {
		u_int16_t	MaxFrameSize;
		u_int16_t	MaxIntervalFrames;
	} TSpec;
	uint8_t	PriorityAndRank; 
	/* 
	 * PriorityAndRank := (3-bit priority | 1 bit rank | 4-bits reserved)
	 * 'rank=0 means emergency traffic, =1 otherwise 
	 */
	unsigned	AccumulatedLatency; /* unsigned 32 bit nsec latency */
} msrpdu_talker_advertise_t;

typedef struct msrpdu_talker_fail {
	uint8_t	StreamID[8];
	struct {
		uint8_t	Dest_Addr[6];
		u_int16_t	Vlan_ID;
	} DataFrameParameters;
	struct {
		u_int16_t	MaxFrameSize;
		u_int16_t	MaxIntervalFrames;
	} TSpec;
	uint8_t	PriorityAndRank;
	unsigned	AccumulatedLatency; 
	struct {
		uint8_t	BridgeID[8];
		uint8_t	FailureCode;
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
	uint8_t	SRclassID;
	uint8_t	SRclassPriority;
	u_int16_t	SRclassVID;
} msrpdu_domain_t;

/* Class ID defitions */
#define MSRP_SR_CLASS_A	6
#define MSRP_SR_CLASS_B	5

/* default values for class priorities */
#define MSRP_SR_CLASS_A_PRIO	3
#define MSRP_SR_CLASS_B_PRIO	2

#define MSRP_DIRECTION_TALKER	0
#define MSRP_DIRECTION_LISTENER	1

#define MAX_FRAME_SIZE		2000

#define MRPD_PORT_DEFAULT	7500

typedef struct client {
	struct client		*next;
	struct sockaddr_in	client;
} client_t;

struct mrp_database {
	mrp_timer_t	lva;
	mrp_timer_t	periodic;
	int		join_timer;
	int		lv_timer;
	int		lva_timer;
	client_t	*clients;
	int		registration;
	int		participant;
};

struct mmrp_attribute {
	struct mmrp_attribute		*prev;
	struct mmrp_attribute		*next;
	u_int32_t			type;	
	union { 
		unsigned char	macaddr[6];	
		uint8_t	svcreq;
	} attribute;
	mrp_applicant_attribute_t	applicant;
	mrp_registrar_attribute_t	registrar;
};

struct mmrp_database {
	struct mrp_database	mrp_db;
	struct mmrp_attribute	*attrib_list;
};


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


int send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data,
		int notify_len);
int client_add(client_t **list, struct sockaddr_in *newclient);
int init_protocol_socket(u_int16_t etype, int *sock,
		unsigned char *multicast_addr);
int init_mrp_timers(struct mrp_database *mrp_db);
int client_delete(client_t **list, struct sockaddr_in *newclient);
