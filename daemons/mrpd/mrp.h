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

/* control debug logging output on stdout */
#define LOG_MRP 0
#define LOG_MVRP 0
#define LOG_MMRP 0
#define LOG_MSRP 0
#define LOG_POLL_EVENTS 0
#define LOG_TIMERS 0
#define LOG_MSRP_GARBAGE_COLLECTION 0
#define LOG_TXNOW 0
#define LOG_CLIENT_RECV 0
#define LOG_CLIENT_SEND 0
#define LOG_ERRORS	0

#define MRP_DEFAULT_POINT_TO_POINT_MAC	1	/* operPointToPointMAC */
#define MRP_ENCODE_YES		0	/* must send */
#define MRP_ENCODE_OPTIONAL	1	/* send if smaller */

typedef struct mrp_applicant_attribute {
	int mrp_state;
	int tx;			/* tx=1 means transmit on next TX event */
	int sndmsg;		/* sndmsg={NEW,IN,JOININ,JOINMT,MT, or LV} */
	int encode;		/* when tx=1, NO, YES or OPTIONAL */
	int mrp_previous_state; /* for identifying state transitions */
} mrp_applicant_attribute_t;

typedef struct mrp_registrar_attribute {
	int mrp_state;
	int notify;
	short rsvd;
	unsigned char macaddr[6];	/* mac address of last registration */
#ifdef LOG_MRP
	int mrp_previous_state; /* for identifying state transitions for debug */
#endif
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
#define MRP_AA_STATE	4	/* Anxious Active */
#define MRP_QA_STATE	5	/* Quiet Active */
#define MRP_LA_STATE	6	/* Leaving Active */
#define MRP_AO_STATE	7	/* Anxious Observer State */
#define MRP_QO_STATE	8	/* Quiet Observer State */
#define MRP_AP_STATE	9	/* Anxious Passive State */
#define MRP_QP_STATE	10	/* Quiet Passive State */
#define MRP_LO_STATE	11	/* Leaving Observer State */

/* Registrar States */
#define MRP_IN_STATE	16	/* when Registrar state is IN */
#define MRP_LV_STATE	17	/* registrar state - leaving */
#define MRP_MT_STATE	18	/* whe Registrar state is MT or LV */

/* MRP Event Spacing */
#define MRP_EVENT_SPACING	100	/*  Event defines are separated by this */

/* MRP Events */
#define MRP_EVENT_BEGIN            (1 * MRP_EVENT_SPACING)    /*  Initialize state machine (10.7.5.1) */
#define MRP_EVENT_NEW              (2 * MRP_EVENT_SPACING)    /*  A new declaration (10.7.5.4) */
#define MRP_EVENT_JOIN             (3 * MRP_EVENT_SPACING)    /*  Declaration registration (10.7.5.5) */
#define MRP_EVENT_LV               (4 * MRP_EVENT_SPACING)    /*  Withdraw a declaration (10.7.5.6) */
#define MRP_EVENT_TX               (5 * MRP_EVENT_SPACING)    /*  Tx without LVA (10.7.5.7) */
#define MRP_EVENT_TXLA             (6 * MRP_EVENT_SPACING)    /*  Tx with a LVA (10.7.5.8) */
#define MRP_EVENT_TXLAF            (7 * MRP_EVENT_SPACING)    /*  Tx with a LVA, no room (Full) (10.7.5.9) */
#define MRP_EVENT_RNEW             (8 * MRP_EVENT_SPACING)    /*  Rx New message (10.7.5.14) */
#define MRP_EVENT_RJOININ          (9 * MRP_EVENT_SPACING)    /*  Rx JoinIn message (10.7.5.15),  */
#define MRP_EVENT_RIN              (10 * MRP_EVENT_SPACING)   /*  receive In message (10.7.5.18) */
#define MRP_EVENT_RJOINMT          (11 * MRP_EVENT_SPACING)   /*  receive JoinEmpty message (10.7.5.16) */
#define MRP_EVENT_RMT              (12 * MRP_EVENT_SPACING)   /*  receive Empty message (10.7.5.19) */
#define MRP_EVENT_RLV              (13 * MRP_EVENT_SPACING)   /*  receive Leave message (10.7.5.17) */
#define MRP_EVENT_RLA              (14 * MRP_EVENT_SPACING)   /*  receive a LeaveAll message (10.7.5.20) */
#define MRP_EVENT_FLUSH            (15 * MRP_EVENT_SPACING)   /*  Port role change (10.7.5.2) */
#define MRP_EVENT_REDECLARE        (16 * MRP_EVENT_SPACING)   /*  Port role changes (10.7.5.3) */
#define MRP_EVENT_PERIODIC         (17 * MRP_EVENT_SPACING)   /*  periodic timer expire */
#define MRP_EVENT_PERIODIC_ENABLE  (18 * MRP_EVENT_SPACING)   /*  periodic timer enable */
#define MRP_EVENT_PERIODIC_DISABLE (19 * MRP_EVENT_SPACING)   /*  periodic timer disable */
#define MRP_EVENT_LVTIMER          (20 * MRP_EVENT_SPACING)   /*  leave timer expire */
#define MRP_EVENT_LVATIMER         (21 * MRP_EVENT_SPACING)   /*  leaveall timer expire */

#define MRP_SND_NEW	0	/* declare and register a new attribute from a new participant */
#define MRP_SND_JOIN	1	/* declare and register an attribute (generally) */
#define MRP_SND_IN	2
#define MRP_SND_LV	6
#define MRP_SND_LVA	7
#define MRP_SND_NULL	8	/* sent as 'ignore' to improve encoding */
#define MRP_SND_NONE	9

/* timer defaults from 802.1Q-2011, Table 10-7 */

/*
 * Join timer may only "fire" 3 times in 300 msec. Default timeout is 200 msec.
 * Here we use 100 msec, or 300/3 msec.
 */
#define MRP_JOINTIMER_VAL	100	/* join timeout in msec */
#define MRP_LVTIMER_VAL		1000	/* leave timeout in msec */
#define MRP_LVATIMER_VAL	10000	/* leaveall timeout in msec */
#define MRP_PERIODTIMER_VAL	1000	/* periodic timeout in msec */

typedef struct mrp_timer {
	int state;
	int tx;			/* tx=1 means transmit on next TX event */
	int sndmsg;		/* sndmsg={NEW,JOIN,or LV}  */
} mrp_timer_t;

typedef struct mrp_periodictimer_state {
	int state;
} mrp_periodictimer_state_t;

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
	uint16_t VectorHeader;	/* LVA << 13 | NumberOfValues */
	uint8_t FirstValue_VectorEvents[];
} mrpdu_vectorattrib_t;

#define MRPDU_VECT_NUMVALUES(x)	((x) & ((1 << 13) - 1))
#define MRPDU_VECT_LVA(x)	(((x) & (7 << 13)) == (1 << 13))
#define MRPDU_VECT_LVA_FLAG	(1 << 13)

typedef struct client_s {
	struct client_s *next;
	struct sockaddr_in client;
} client_t;

struct mrp_database {
	mrp_timer_t lva;
	HTIMER join_timer;
	int join_timer_running;
	HTIMER lv_timer;
	int lv_timer_running;
	HTIMER lva_timer;
	int lva_timer_running;
	client_t *clients;
	int registration;
	int participant;
};

int mrp_client_add(client_t ** list, struct sockaddr_in *newclient);
int mrp_client_delete(client_t ** list, struct sockaddr_in *newclient);

int mrp_init(void);
char *mrp_event_string(int e);
int mrp_periodictimer_start();
int mrp_periodictimer_stop();
int mrp_periodictimer_fsm(struct mrp_periodictimer_state *periodic_state, int event);
int mrp_jointimer_stop(struct mrp_database *mrp_db);
int mrp_jointimer_start(struct mrp_database *mrp_db);
int mrp_lvtimer_start(struct mrp_database *mrp_db);
int mrp_lvtimer_stop(struct mrp_database *mrp_db);
int mrp_lvatimer_start(struct mrp_database *mrp_db);
int mrp_lvatimer_stop(struct mrp_database *mrp_db);
int mrp_lvatimer_fsm(struct mrp_database *mrp_db, int event);
int mrp_applicant_fsm(struct mrp_database *mrp_db,
		      mrp_applicant_attribute_t * attrib, int event, int in_flag);
int mrp_registrar_fsm(mrp_registrar_attribute_t * attrib,
		      struct mrp_database *mrp_db, int event);
int mrp_registrar_in(mrp_registrar_attribute_t * attrib);
int mrp_decode_state(mrp_registrar_attribute_t * rattrib,
		     mrp_applicant_attribute_t * aattrib, char *str,
		     int strlen);
int mrp_applicant_state_transition_implies_tx(mrp_applicant_attribute_t * attrib);
void mrp_schedule_tx_event(struct mrp_database *mrp_db);

#if LOG_MVRP || LOG_MSRP || LOG_MMRP || LOG_MRP
char *mrp_event_string(int e);
char *mrp_send_string(int s);
char *mrp_pdu_string(int s);
char *mrp_print_status(const mrp_applicant_attribute_t * app,
		       const mrp_registrar_attribute_t * reg);
#endif
