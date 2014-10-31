/****************************************************************************
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
 * MSRP protocol (part of 802.1Q-2011)
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "parse.h"
#include "mrpd.h"
#include "mrp.h"
#include "msrp.h"
#include "mmrp.h"

int msrp_txpdu(void);
static struct msrp_attribute *msrp_alloc(void);
int msrp_send_notifications(struct msrp_attribute *attrib, int notify);
static struct msrp_attribute *msrp_conditional_reclaim(struct msrp_attribute *sattrib);

int msrp_event(int event, struct msrp_attribute *rattrib);

unsigned char MSRP_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E };

extern unsigned char STATION_ADDR[];

/* global variables */
SOCKET msrp_socket;
struct msrp_database *MSRP_db;

char *msrp_attrib_type_string(int t)
{
	switch (t) {
	case MSRP_TALKER_ADV_TYPE:
		return "Talker Advertise";
	case MSRP_TALKER_FAILED_TYPE:
		return "Talker Failed";
	case MSRP_LISTENER_TYPE:
		return "Listener";
	case MSRP_DOMAIN_TYPE:
		return "Domain";
	default:
		return "TYPE ???";
	}
}

#if LOG_MSRP || LOG_MRP
void msrp_print_debug_info(int evt, const struct msrp_attribute *attrib)
{
	char * state_mc_states = NULL;

	if (attrib) {
#if 0
		if (MSRP_DOMAIN_TYPE == attrib->type) {
			state_mc_states = mrp_print_status(
						&(attrib->applicant),
						&(attrib->registrar));
			mrpd_log_printf("MSRP Domain event %s, %s\n",
				     mrp_event_string(evt),
				     state_mc_states);		

		}
#endif
		if (MSRP_TALKER_ADV_TYPE == attrib->type) {
			state_mc_states = mrp_print_status(
						&(attrib->applicant),
						&(attrib->registrar));
			mrpd_log_printf("MSRP Talker event %s, %s\n",
				     mrp_event_string(evt),
				     state_mc_states);		

		}
	}
}
#endif

struct msrp_attribute *msrp_lookup(struct msrp_attribute *rattrib)
{
	struct msrp_attribute *attrib;
	int mac_eq;

	attrib = MSRP_db->attrib_list;
	while (NULL != attrib) {
		if (	(rattrib->type == attrib->type) ||
			((MSRP_TALKER_ADV_TYPE == rattrib->type) && (MSRP_TALKER_FAILED_TYPE == attrib->type)) ||
			((MSRP_TALKER_FAILED_TYPE == rattrib->type) && (MSRP_TALKER_ADV_TYPE == attrib->type)))
			{
			if (MSRP_DOMAIN_TYPE == attrib->type) {
				if (attrib->attribute.domain.SRclassID ==
				   rattrib->attribute.domain.SRclassID)
					return attrib;
			} else {
				/* compare on the stream ID */
				mac_eq =
				    memcmp(attrib->attribute.talk_listen.
					   StreamID,
					   rattrib->attribute.talk_listen.
					   StreamID, 8);
				if (0 == mac_eq)
					return attrib;
			}
		}
		attrib = attrib->next;
	}
	return NULL;
}

int msrp_add(struct msrp_attribute *rattrib)
{
	struct msrp_attribute *attrib;
	struct msrp_attribute *attrib_tail;
	int mac_eq;

	/* XXX do a lookup first to guarantee uniqueness? */

	attrib_tail = attrib = MSRP_db->attrib_list;

	while (NULL != attrib) {
		/* sort list into types, then sorted in order within types */
		if (rattrib->type == attrib->type) {
			if (MSRP_DOMAIN_TYPE == attrib->type) {
				mac_eq = memcmp(&(attrib->attribute.domain),
						&(rattrib->attribute.domain),
						sizeof(msrpdu_domain_t));

				if (mac_eq < 0) {
					/* possible tail insertion ... */

					/*
					 * see if there is a another of the same following
					 * not likely since valid values are only 0 and 1
					 */
					if (NULL != attrib->next) {
						if (attrib->type ==
						    attrib->next->type) {
							attrib = attrib->next;
							continue;
						}
					}

					rattrib->next = attrib->next;
					rattrib->prev = attrib;
					attrib->next = rattrib;
					if (rattrib->next)
						rattrib->next->prev = rattrib;
					return 0;

				} else {
					/* head insertion ... */
					rattrib->next = attrib;
					rattrib->prev = attrib->prev;
					attrib->prev = rattrib;
					if (NULL != rattrib->prev)
						rattrib->prev->next = rattrib;
					else
						MSRP_db->attrib_list = rattrib;

					return 0;
				}
			} else {
				mac_eq =
				    memcmp(attrib->attribute.talk_listen.
					   StreamID,
					   rattrib->attribute.talk_listen.
					   StreamID, 8);

				if (mac_eq < 0) {
					/* possible tail insertion ... */

					if (NULL != attrib->next) {
						if (attrib->type ==
						    attrib->next->type) {
							attrib = attrib->next;
							continue;
						}
					}

					rattrib->next = attrib->next;
					rattrib->prev = attrib;
					attrib->next = rattrib;
					if (rattrib->next)
						rattrib->next->prev = rattrib;
					return 0;

				} else {
					/* head insertion ... */
					rattrib->next = attrib;
					rattrib->prev = attrib->prev;
					attrib->prev = rattrib;
					if (NULL != rattrib->prev)
						rattrib->prev->next = rattrib;
					else
						MSRP_db->attrib_list = rattrib;

					return 0;
				}
			}
		}
		attrib_tail = attrib;
		attrib = attrib->next;
	}

	/* if we are here we didn't need to stitch in a a sorted entry
	 * so append it onto the tail (if it exists)
	 */

	if (NULL == attrib_tail) {
		rattrib->next = NULL;
		rattrib->prev = NULL;
		MSRP_db->attrib_list = rattrib;
	} else {
		rattrib->next = NULL;
		rattrib->prev = attrib_tail;
		attrib_tail->next = rattrib;
	}

	return 0;
}

int msrp_merge(struct msrp_attribute *rattrib)
{
	struct msrp_attribute *talker_attrib;
	struct msrp_attribute *attrib;
	int talker_missing;

	attrib = msrp_lookup(rattrib);

	if (NULL == attrib)
		return -1;	/* shouldn't happen */

	/* we always update the last mac address state for diagnostics */
	memcpy(attrib->registrar.macaddr, rattrib->registrar.macaddr, 6);

	/* some attribute types carry updated fault condition information
	 * from the bridges indicating either insufficient bandwidth or
	 * excessive latency between the talker and listener. We merge
	 * these status conditions with the existing attribute without
	 * necessarily changing the MRP state (we let the FSM functions
	 * handle these)
	 */
	switch (attrib->type) {
	case MSRP_TALKER_ADV_TYPE:
	case MSRP_TALKER_FAILED_TYPE:
		/* support merging
		 * TalkerFailed <- TalkerAdvertise and
		 * TalkerAdvertise <- TalkerFailed
		 */
		attrib->attribute.talk_listen.FailureInformation.FailureCode =
		    rattrib->attribute.talk_listen.FailureInformation.
		    FailureCode;
		memcpy(attrib->attribute.talk_listen.FailureInformation.
		       BridgeID,
		       rattrib->attribute.talk_listen.FailureInformation.
		       BridgeID, 8);
#ifdef ENABLE_MERGED_LATENCY
		attrib->attribute.talk_listen.AccumulatedLatency =
		    rattrib->attribute.talk_listen.AccumulatedLatency;
#endif 
		if (attrib->type != rattrib->type) {
			attrib->type = rattrib->type;
			attrib->registrar.mrp_state = MRP_MT_STATE;	/* ugly - force a notify */
		}
		break;
	case MSRP_LISTENER_TYPE:
		if (attrib->substate != rattrib->substate) {
			/* If transitioning from AskingFailed to Ready or ReadyFailed, need to 
			 * check a matching talkerID has been declared.
			 */
			if (MSRP_LISTENER_ASKFAILED == attrib->substate) {
				talker_attrib = msrp_alloc();
				if (NULL == talker_attrib)
					break;
				talker_attrib->type =
				    MSRP_TALKER_ADV_TYPE;
				memcpy(talker_attrib->attribute.
				       talk_listen.StreamID,
				       attrib->attribute.
				       talk_listen.StreamID, 8);
				talker_missing = (NULL == msrp_lookup(talker_attrib));
				free(talker_attrib);
				if (talker_missing)
					break;
			}
			attrib->substate = rattrib->substate;
			attrib->registrar.mrp_state = MRP_MT_STATE;	/* ugly - force a notify */
		}
		break;
	case MSRP_DOMAIN_TYPE:
		/*
		 * If the device has the capability of updating the AVTP packet priority,
		 * then ENABLE_MERGED_DOMAIN_PRIORITY can be defined.
		 */
		if (
		   (attrib->attribute.domain.SRclassPriority !=
		    rattrib->attribute.domain.SRclassPriority) ||
		    (attrib->attribute.domain.SRclassVID !=
		    rattrib->attribute.domain.SRclassVID)) {
#ifdef ENABLE_MERGED_DOMAIN_PRIORITY
			attrib->attribute.domain.SRclassPriority =
			    rattrib->attribute.domain.SRclassPriority;
#endif
			attrib->attribute.domain.neighborSRclassPriority =
			    rattrib->attribute.domain.SRclassPriority;
 			attrib->attribute.domain.SRclassVID =
			    rattrib->attribute.domain.SRclassVID;
			attrib->registrar.mrp_state = MRP_MT_STATE;	/* ugly - force a notify */
		}
		break;
	default:
		break;
	}
	return 0;
}

#ifdef MRP_CPPUTEST /* MSRP_PDU_TEST */
int msrp_event_orig(int event, struct msrp_attribute *rattrib)
#else
int msrp_event(int event, struct msrp_attribute *rattrib)
#endif
{
	struct msrp_attribute *attrib;
	int count = 0;
	int rc;

	switch (event) {
	case MRP_EVENT_LVATIMER:
		mrp_lvatimer_stop(&(MSRP_db->mrp_db));
		mrp_jointimer_stop(&(MSRP_db->mrp_db));
		/* update state */
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MSRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_TXLA,
					  mrp_registrar_in(&(attrib->registrar)));
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_TXLA);
#if LOG_MSRP
			msrp_print_debug_info(event, attrib);
#endif
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_LVATIMER);

		MSRP_db->send_empty_LeaveAll_flag = 1;
		/*
		 * We are going to emit a PDU here, so call lva FSM with
		 * TX event so as to set the lva.tx=1 flag prior to sending
		 * the PDU. After the pdu is sent, lva.tx=0 and the lva
		 * LSM is back to the passive state.
		 */
		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_TX);
		msrp_txpdu();
		MSRP_db->send_empty_LeaveAll_flag = 0;
		break;
	case MRP_EVENT_RLA:
		mrp_jointimer_start(&(MSRP_db->mrp_db));
		if (NULL == rattrib)
			return -1;	/* XXX internal fault */

		/* update state */
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			if (attrib->type == rattrib->type) {
				mrp_applicant_fsm(&(MSRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_RLA,
					  mrp_registrar_in(&(attrib->registrar)));
				mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_RLA);
#if LOG_MSRP
				msrp_print_debug_info(event, attrib);
#endif
			}
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_RLA);

		break;
	case MRP_EVENT_TX:
		mrp_jointimer_stop(&(MSRP_db->mrp_db));
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MSRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_TX,
					  mrp_registrar_in(&(attrib->registrar)));
#if LOG_MSRP
			msrp_print_debug_info(event, attrib);
#endif
			count += mrp_applicant_state_transition_implies_tx(&(attrib->applicant));
			attrib = attrib->next;
		}

		/*
		 * Don't need to call mrp_lvatimer_fsm(..., MRP_EVENT_TX) from
		 * here because the TX event only ever does anything in the
		 * lva FSM when the start is active, and it is only active
		 * momentarily after a LVATIMER event.
		 */
		msrp_txpdu();

		/*
		 * Certain state transitions imply we need to request another tx
		 * opportunity.
		 */
		if (count) {
			mrp_jointimer_start(&(MSRP_db->mrp_db));
		}

		break;
	case MRP_EVENT_LVTIMER:
		mrp_lvtimer_stop(&(MSRP_db->mrp_db));
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db),
					  MRP_EVENT_LVTIMER);

#if LOG_MSRP
			msrp_print_debug_info(event, attrib);
#endif
			attrib = msrp_conditional_reclaim(attrib);
		}
		break;
	case MRP_EVENT_PERIODIC:
		/*
		 * Periodic timer is ignored in MSRP application.
		 */
		break;
	case MRP_EVENT_NEW:
	case MRP_EVENT_JOIN:
	case MRP_EVENT_RNEW:
	case MRP_EVENT_RJOININ:
	case MRP_EVENT_RJOINMT:
	case MRP_EVENT_LV:
	case MRP_EVENT_RIN:
	case MRP_EVENT_RMT:
	case MRP_EVENT_RLV:
		mrp_jointimer_start(&(MSRP_db->mrp_db));
		if (NULL == rattrib)
			return -1;	/* XXX internal fault */

		/* update state */
		attrib = msrp_lookup(rattrib);

		if (NULL == attrib) {
			msrp_add(rattrib);
			attrib = rattrib;
		} else {
			msrp_merge(rattrib);
			free(rattrib);
		}

		mrp_applicant_fsm(&(MSRP_db->mrp_db), &(attrib->applicant),
				  event,
				  mrp_registrar_in(&(attrib->registrar)));

		/* remap local requests into registrar events */
		switch (event) {
		case MRP_EVENT_NEW:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_BEGIN);
			attrib->registrar.notify = MRP_NOTIFY_NEW;
			break;
		case MRP_EVENT_JOIN:
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MSRP_db->mrp_db),
						  MRP_EVENT_RJOININ);
			else
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MSRP_db->mrp_db),
						  MRP_EVENT_RJOINMT);
			break;
		case MRP_EVENT_LV:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_RLV);
			break;
		default:
			rc = mrp_registrar_fsm(&(attrib->registrar),
					       &(MSRP_db->mrp_db), event);
			if (-1 == rc) {
				printf
				    ("MSRP registrar error on attrib->type = %s (%d)\n",
				     msrp_attrib_type_string(attrib->type),
				     attrib->type);
			}
			break;
		}
		msrp_conditional_reclaim(attrib);
#if LOG_MSRP
		msrp_print_debug_info(event, attrib);
#endif

		break;
	default:
		break;
	}

	/*
	 * XXX should honor the MSRP_db->mrp_db.registration and
	 * MSRP_db->mrp_db.participant controls
	 */

	/* generate local notifications */
	attrib = MSRP_db->attrib_list;

	while (NULL != attrib) {
		if (MRP_NOTIFY_NONE != attrib->registrar.notify) {
			msrp_send_notifications(attrib,
						attrib->registrar.notify);
			attrib->registrar.notify = MRP_NOTIFY_NONE;
		}
		attrib = attrib->next;
	}

	return 0;
}

static struct msrp_attribute *msrp_alloc()
{
	struct msrp_attribute *attrib;

	attrib = (struct msrp_attribute *) malloc(sizeof(struct msrp_attribute));
	if (NULL == attrib)
		return NULL;

	memset(attrib, 0, sizeof(struct msrp_attribute));

	attrib->applicant.mrp_state = MRP_VO_STATE;
	attrib->applicant.tx = 0;
	attrib->applicant.sndmsg = MRP_SND_NULL;
	attrib->applicant.encode = MRP_ENCODE_OPTIONAL;
#ifdef LOG_MRP
	attrib->applicant.mrp_previous_state = -1;
#endif

	attrib->registrar.mrp_state = MRP_MT_STATE;
	attrib->registrar.notify = MRP_NOTIFY_NONE;
#ifdef LOG_MRP
	attrib->registrar.mrp_previous_state = -1;
#endif
	return attrib;
}

void msrp_increment_streamid(uint8_t * streamid)
{

	int i;

	i = 7;
	while (i > 0) {
		if (255 != streamid[i]) {
			streamid[i]++;
			return;
		}
		streamid[i] = 0;
		i--;
	}
	return;
}

static int msrp_recv_msg_check_attrib_list_len(
	int attrib_type,
	uint8_t *pkt_attrib_list_len,
	uint8_t *pkt_eof)
{
	uint32_t list_len = (uint32_t)pkt_attrib_list_len[0] << 8 |
			(uint32_t)pkt_attrib_list_len[1];
	uint32_t attrib_len = (uint32_t)pkt_attrib_list_len[-1];
#if !LOG_MSRP
	(void)(attrib_type);
#endif

	/*
	 * Check for list length < attribute len + header_size (2) + event (1)
	 * Note: this test is more of a sanity test than anything else. It does
	 * not attempt to use number of values to compute 3packed and 4packed byte
	 * count.
	 */
	if (list_len < (attrib_len + 2 + 1)) {
#if LOG_MSRP
		mrpd_log_printf
		    ("MSRP msrp_recv_msg_check_attrib_list_len(%s) FAILED SIZE list len %d attrib len %d\n",
		     msrp_attrib_type_string(attrib_type),
		     list_len,
		     attrib_len);
#endif
		return 0;
	}

	/* check the eof */
	if ((pkt_attrib_list_len + list_len) >= pkt_eof) {
#if LOG_MSRP
		mrpd_log_printf
		    ("MSRP msrp_recv_msg_check_attrib_list_len(%s) FAILED EOF list len %d pkt %p eof %p\n",
		     msrp_attrib_type_string(attrib_type),
		     list_len,
		     pkt_attrib_list_len,
		     pkt_eof);
#endif
		return 0;
	}

	/* check for the endmark */
	if ((0 != pkt_attrib_list_len[list_len]) || (0 != pkt_attrib_list_len[list_len + 1])) {
#if LOG_MSRP
		mrpd_log_printf
		    ("MSRP msrp_recv_msg_check_attrib_list_len(%s) FAILED ENDMARK list len %d pkt %p\n",
		     msrp_attrib_type_string(attrib_type),
		     list_len,
		     pkt_attrib_list_len,
		     pkt_eof);
#endif
		return 0;
	}

	return 1;
}

int msrp_recv_msg()
{
	char *msgbuf;
	int bytes = 0;
	eth_hdr_t *eth;
	mrpdu_t *mrpdu;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr;
	unsigned char *mrpdu_msg_eof;
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	uint16_t numvalues;
	uint16_t numvalues_processed;
	int numvectorbytes;
	uint8_t vect_3pack;
	uint8_t vect_4pack;
	int vectidx;
	int vectevt[4];
	int vectevt_idx;
	uint8_t srclassID_firstval;
	uint8_t srclassprio_firstval;
	uint16_t srclassvid_firstval;
	uint8_t streamid_firstval[8];
	uint8_t destmac_firstval[6];
	struct msrp_attribute *attrib;
	int endmarks;
	int *listener_vectevt = NULL;
	int listener_vectevt_sz = 0;
	int listener_vectevt_idx;
	int listener_endbyte;
	int saw_domain_lva = 0;
	int saw_talker_lva = 0;
	int saw_talkerfailed_lva = 0;
	int saw_listener_lva = 0;

	bytes = mrpd_recvmsgbuf(msrp_socket, &msgbuf);
	if (bytes <= 0)
		goto out;
	if ((unsigned int)bytes < (sizeof(eth_hdr_t) + sizeof(mrpdu_t) +
				   sizeof(mrpdu_message_t)))
		goto out;

	eth = (eth_hdr_t *) msgbuf;

	/* note that MSRP frames should always arrive untagged (no vlan) */
	if (MSRP_ETYPE != ntohs(eth->typelen)) {
		goto out;
	}
	/* check dest mac address too */
	if (memcmp(eth->destaddr, MSRP_ADDR, sizeof(eth->destaddr)))
		goto out;

	mrpdu = (mrpdu_t *) (msgbuf + sizeof(struct eth_hdr));

	/*
	 * ProtocolVersion handling - a receiver must process received frames with a lesser
	 * protocol version consistent with the older protocol processing requirements (e.g. a V2
	 * agent receives a V1 message, the V1 message should be parsed with V1 rules).
	 *
	 * However - if an agent receives a NEWER protocol, the agent should still attempt
	 * to parse the frame. If the agent finds an AttributeType not recognized
	 * the agent discards the current message including any associated trailing vectors
	 * up to the end-mark, and resumes with the next message or until the end of the PDU
	 * is reached.
	 *
	 * If a VectorAttribute is found with an unknown Event for the Type, the specific
	 * VectorAttrute is discarded and processing continues with the next VectorAttribute.
	 */

	/*
	if (MSRP_PROT_VER != mrpdu->ProtocolVersion) {
		goto out;
	}
	*/

	mrpdu_msg_ptr = MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu);

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= MRPD_OFFSETOF_MRPD_GET_MRPDU_MESSAGE_LIST;

	endmarks = 0;

	while (mrpdu_msg_ptr < (mrpdu_msg_eof - MRPDU_ENDMARK_SZ)) {
		mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
		if ((mrpdu_msg->AttributeType == 0) &&
		    (mrpdu_msg->AttributeLength == 0)) {
			mrpdu_msg_ptr += 2;
			endmarks++;
			if (endmarks < 2)
				continue;	/* end-mark of message-list */
			else {
				break;	/* two endmarks - end of message list */
			}
		}

		endmarks = 0;

		switch (mrpdu_msg->AttributeType) {
		case MSRP_DOMAIN_TYPE:
			if (mrpdu_msg->AttributeLength != 4) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}

			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			/* check attribute list length is reasonable */
			if (!msrp_recv_msg_check_attrib_list_len(mrpdu_msg->AttributeType, &mrpdu_msg->Data[0], mrpdu_msg_eof)) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				mrpdu_msg_ptr =
				    (uint8_t *) mrpdu_vectorptr;
				/* skip this null attribute ... some switches generate these ... */
				/* 2 byte numvalues + 4 byte FirstValue + vector bytes */
				mrpdu_msg_ptr += 2 + 4 + (numvalues + 2) / 3;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *)
				    mrpdu_msg_ptr;
				continue;
			}

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));
#if LOG_MSRP
				mrpd_log_printf
				    ("MSRP msrp_recv_msg() DOMAIN vector addr 0x%X, n = %d, VectorHeader %d\n",
				     ((int)mrpdu_vectorptr) & 0x3, numvalues,
				     ntohs(mrpdu_vectorptr->VectorHeader));
#endif

				if (MRPDU_VECT_LVA(ntohs(mrpdu_vectorptr->VectorHeader)) && !saw_domain_lva) {
					saw_domain_lva = 1;
					attrib = msrp_alloc();
					if (NULL == attrib)
						goto out;	/* oops - internal error */

					attrib->type = 	MSRP_DOMAIN_TYPE;				
					msrp_event(MRP_EVENT_RLA, attrib);
					free(attrib);
				}


				if (0 == numvalues) {
					/* skip this null attribute ... some switches generate these ... */
					/* 2 byte numvalues + 4 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (uint8_t *) mrpdu_vectorptr;
					mrpdu_msg_ptr += 6;

					mrpdu_vectorptr =
					    (mrpdu_vectorattrib_t *)
					    mrpdu_msg_ptr;
					continue;
				}
				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof) {
					/* Malformed - runs off the end of the pdu */
					goto out;
				}
				srclassID_firstval =
				    mrpdu_vectorptr->FirstValue_VectorEvents[0];
				srclassprio_firstval =
				    mrpdu_vectorptr->FirstValue_VectorEvents[1];
				srclassvid_firstval = ((uint16_t)
						       mrpdu_vectorptr->
						       FirstValue_VectorEvents
						       [2]) << 8 |
				    mrpdu_vectorptr->FirstValue_VectorEvents[3];

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 4;
				     vectidx <= (numvectorbytes + 4);
				     vectidx++) {
					vect_3pack =
					    mrpdu_vectorptr->
					    FirstValue_VectorEvents[vectidx];
					vectevt[0] = vect_3pack / 36;
					vectevt[1] =
					    (vect_3pack - vectevt[0] * 36) / 6;
					vectevt[2] =
					    vect_3pack - (36 * vectevt[0]) -
					    (6 * vectevt[1]);

					numvalues_processed =
					    (numvalues > 3 ? 3 : numvalues);

					/* check for out of range encoding */
					if (vectevt[0] > MRPDU_LV) {
#if LOG_MSRP
						mrpd_log_printf("MSRP msrp_recv_msg() DOMAIN vector discard\n");
#endif
						numvalues -= numvalues_processed;
						continue;
					}

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						attrib = msrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type = MSRP_DOMAIN_TYPE;
						attrib->attribute.domain.
						    SRclassID =
						    srclassID_firstval;
						attrib->attribute.domain.
						    SRclassPriority =
						    srclassprio_firstval;
						attrib->attribute.domain.
						    neighborSRclassPriority =
						    srclassprio_firstval;
						attrib->attribute.domain.
						    SRclassVID =
						    srclassvid_firstval;

						srclassID_firstval++;
						srclassprio_firstval++;

						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							msrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							msrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							msrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							msrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							msrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							msrp_event
							    (MRP_EVENT_RLV,
							     attrib);
							break;
						default:
							free(attrib);
							break;
						}
					}
					numvalues -= numvalues_processed;
				}

				/* 2 byte numvalues + 4 byte FirstValue + (n) vector bytes */
				mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 6 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		case MSRP_LISTENER_TYPE:
			if (mrpdu_msg->AttributeLength != 8) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}

			/* MSRP uses AttributeListLength ...  */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			/* check attribute list length is reasonable */
			if (!msrp_recv_msg_check_attrib_list_len(mrpdu_msg->AttributeType, &mrpdu_msg->Data[0], mrpdu_msg_eof)) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				mrpdu_msg_ptr =
				    (uint8_t *) mrpdu_vectorptr;
				/* skip this null attribute ... some switches generate these ... */
				/* 2 byte numvalues + 8 byte FirstValue + (0) vector bytes */
				mrpdu_msg_ptr += 2 + 8 + (numvalues + 2) / 3 + (numvalues + 3) / 4;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *)
				    mrpdu_msg_ptr;
				continue;
			}


			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

#if LOG_MSRP
				mrpd_log_printf
				    ("MSRP msrp_recv_msg() LISTENER vector addr 0x%X, n = %d, VectorHeader  %d\n",
				     ((int)mrpdu_vectorptr) & 0x3, numvalues,
				     ntohs(mrpdu_vectorptr->VectorHeader));
#endif

				if (MRPDU_VECT_LVA(ntohs(mrpdu_vectorptr->VectorHeader)) && !saw_listener_lva) {
					saw_listener_lva = 1;
					attrib = msrp_alloc();
					if (NULL == attrib)
						goto out;	/* oops - internal error */

					attrib->type = 	MSRP_LISTENER_TYPE;				
					msrp_event(MRP_EVENT_RLA, attrib);
					free(attrib);
				}


				if (0 == numvalues) {
					/* 2 byte numvalues + 8 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (uint8_t *) mrpdu_vectorptr;
					mrpdu_msg_ptr += 10;

					mrpdu_vectorptr =
					    (mrpdu_vectorattrib_t *)
					    mrpdu_msg_ptr;
					/* Malformed - cant tell how long the trailing vectors are */
					continue;
				}
				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3 + numvalues / 4) >=
				    mrpdu_msg_eof) {
					/* Malformed - runs off the end of the pdu */
					goto out;
				}
				memcpy(streamid_firstval,
				       &
				       (mrpdu_vectorptr->FirstValue_VectorEvents
					[0]), 8);

				/*
				 * parse the 4-packed events first to figure out the substates, then
				 * insert them as we parse the 3-packed events for the MRP state machines
				 */

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 4) * 4))
					numvectorbytes = (numvalues / 4) + 1;
				else
					numvectorbytes = (numvalues / 4);

				if (numvalues != ((numvalues / 3) * 3))
					vectidx = 8 + (numvalues / 3) + 1;
				else
					vectidx = 8 + (numvalues / 3);

				listener_endbyte = vectidx + numvectorbytes;

				if (&
				    (mrpdu_vectorptr->FirstValue_VectorEvents
				     [listener_endbyte]) >= mrpdu_msg_eof)
					goto out;

				if (NULL == listener_vectevt) {
					listener_vectevt_sz = 64;
					listener_vectevt =
					    (int *)malloc(listener_vectevt_sz *
							  sizeof(int));
					if (NULL == listener_vectevt)
						goto out;
				}

				listener_vectevt_idx = 0;

				for (; vectidx < listener_endbyte; vectidx++) {
					vect_4pack =
					    mrpdu_vectorptr->
					    FirstValue_VectorEvents[vectidx];

					vectevt[3] =
					    vect_4pack -
					    ((vect_4pack >> 2) << 2);
					vect_4pack = vect_4pack >> 2;
					vectevt[2] =
					    vect_4pack -
					    ((vect_4pack >> 2) << 2);
					vect_4pack = vect_4pack >> 2;
					vectevt[1] =
					    vect_4pack -
					    ((vect_4pack >> 2) << 2);
					vect_4pack = vect_4pack >> 2;
					vectevt[0] =
					    vect_4pack -
					    ((vect_4pack >> 2) << 2);
					vect_4pack = vect_4pack >> 2;

					numvalues_processed =
					    (numvalues > 4 ? 4 : numvalues);

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {
						if (listener_vectevt_idx ==
						    listener_vectevt_sz) {
							listener_vectevt =
							    (int*)realloc
							    (listener_vectevt,
							     (listener_vectevt_sz
							      +
							      64) *
							     sizeof(int));
							if (NULL ==
							    listener_vectevt)
								goto out;
							listener_vectevt_sz +=
							    64;
						}
						listener_vectevt
						    [listener_vectevt_idx] =
						    vectevt[vectevt_idx];
						listener_vectevt_idx++;
					}
					numvalues -= numvalues_processed;
				}

				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				listener_vectevt_idx = 0;

				for (vectidx = 8;
				     vectidx <= (numvectorbytes + 8);
				     vectidx++) {
					vect_3pack =
					    mrpdu_vectorptr->
					    FirstValue_VectorEvents[vectidx];
					vectevt[0] = vect_3pack / 36;
					vectevt[1] =
					    (vect_3pack - vectevt[0] * 36) / 6;
					vectevt[2] =
					    vect_3pack - (36 * vectevt[0]) -
					    (6 * vectevt[1]);

					numvalues_processed =
					    (numvalues > 3 ? 3 : numvalues);

					/* check for out of range encoding */
					if (vectevt[0] > MRPDU_LV) {
#if LOG_MSRP
						mrpd_log_printf("MSRP msrp_recv_msg() LISTENER vector discard\n");
#endif
						numvalues -= numvalues_processed;
						continue;
					}

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						if (MSRP_LISTENER_IGNORE ==
						    listener_vectevt
						    [listener_vectevt_idx]) {
							msrp_increment_streamid
							    (streamid_firstval);
							listener_vectevt_idx++;
							continue;
						}

						attrib = msrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type =
						    MSRP_LISTENER_TYPE;

						memcpy(attrib->attribute.
						       talk_listen.StreamID,
						       streamid_firstval, 8);
						attrib->substate =
						    listener_vectevt
						    [listener_vectevt_idx];
						msrp_increment_streamid
						    (streamid_firstval);
						listener_vectevt_idx++;

						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

#if LOG_MSRP
						mrpd_log_printf
						    ("MSRP msrp_recv_msg() Listener[%d][%d] attrib %d\n",
						     vectidx, vectevt_idx,
						     vectevt[vectevt_idx]);
#endif

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							msrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							msrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							msrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							msrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							msrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							msrp_event
							    (MRP_EVENT_RLV,
							     attrib);
							break;
						default:
							free(attrib);
							break;
						}
					}
					numvalues -= numvalues_processed;
				}

				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));
				if (numvalues != ((numvalues / 4) * 4))
					numvectorbytes += (numvalues / 4) + 1;
				else
					numvectorbytes += (numvalues / 4);

				/* 2 byte numvalues + 8 byte FirstValue + (n) vector bytes */
				mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 10 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}

			break;
		case MSRP_TALKER_ADV_TYPE:
			/* another place where the spec munges with reality ...
			 * we can start out as a talker advertise type, but can get back
			 * a talker failed from the bridge in response ... in which
			 * by the spec we are supposed to merge the talker failed into
			 * the talker advertise and change the attribute type ...
			 *
			 * but also note the spec says that a talker cant change the
			 * firstvalues definition of a stream without tearing down the
			 * existing stream, wait two leave-all periods, then re-advertise
			 * the stream.
			 */
			if (mrpdu_msg->AttributeLength != 25) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}

			/* MSRP uses AttributeListLength ...  */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			/* check attribute list length is reasonable */
			if (!msrp_recv_msg_check_attrib_list_len(mrpdu_msg->AttributeType, &mrpdu_msg->Data[0], mrpdu_msg_eof)) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				mrpdu_msg_ptr =
				    (uint8_t *) mrpdu_vectorptr;
				/* skip this null attribute ... some switches generate these ... */
				/* 2 byte numvalues + 25 byte FirstValue + (0) vector bytes */
				mrpdu_msg_ptr += 2 + 25 + (numvalues + 2) / 3;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *)
				    mrpdu_msg_ptr;
				continue;
			}

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

#if LOG_MSRP
				mrpd_log_printf
				     ("MSRP msrp_recv_msg() TALKER vector addr 0x%X, n = %d, VectorHeader %d\n",
				     ((int)mrpdu_vectorptr) & 0x3, numvalues,
				     ntohs(mrpdu_vectorptr->VectorHeader));
#endif

				if (MRPDU_VECT_LVA(ntohs(mrpdu_vectorptr->VectorHeader)) && !saw_talker_lva) {
					saw_talker_lva = 1;
					attrib = msrp_alloc();
					if (NULL == attrib)
						goto out;	/* oops - internal error */

					attrib->type = 	MSRP_TALKER_ADV_TYPE;				
					msrp_event(MRP_EVENT_RLA, attrib);
					free(attrib);
				}

				if (0 == numvalues) {
					/* 2 byte numvalues + 25 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (uint8_t *) mrpdu_vectorptr;
					mrpdu_msg_ptr += 27;

					mrpdu_vectorptr =
					    (mrpdu_vectorattrib_t *)
					    mrpdu_msg_ptr;
					continue;
				}
				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof) {
					/* Malformed - runs off the end of the pdu */
					printf
					    ("bad talker adv PDU too long \n");
					goto out;
				}
				memcpy(streamid_firstval,
				       mrpdu_vectorptr->FirstValue_VectorEvents,
				       8);
				memcpy(destmac_firstval,
				       &
				       (mrpdu_vectorptr->FirstValue_VectorEvents
					[8]), 6);

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 25;
				     vectidx <= (numvectorbytes + 25);
				     vectidx++) {
					vect_3pack =
					    mrpdu_vectorptr->
					    FirstValue_VectorEvents[vectidx];
					vectevt[0] = vect_3pack / 36;
					vectevt[1] =
					    (vect_3pack - vectevt[0] * 36) / 6;
					vectevt[2] =
					    vect_3pack - (36 * vectevt[0]) -
					    (6 * vectevt[1]);

					numvalues_processed =
					    (numvalues > 3 ? 3 : numvalues);

					/* check for out of range encoding */
					if (vectevt[0] > MRPDU_LV) {
#if LOG_MSRP
						mrpd_log_printf("MSRP msrp_recv_msg() TALKER_ADV vector discard\n");
#endif
						numvalues -= numvalues_processed;
						continue;
					}

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						attrib = msrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type =
						    MSRP_TALKER_ADV_TYPE;

						/* note this ISNT from us ... */
						attrib->direction =
						    MSRP_DIRECTION_LISTENER;

						memcpy(attrib->attribute.
						       talk_listen.StreamID,
						       streamid_firstval, 8);
						memcpy(attrib->attribute.
						       talk_listen.
						       DataFrameParameters.
						       Dest_Addr,
						       destmac_firstval, 6);

						msrp_increment_streamid
						    (streamid_firstval);
						mmrp_increment_macaddr
						    (destmac_firstval);

						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[14];
						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID <<= 8;
						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[15];

						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[16];
						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize <<= 8;
						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[17];

						attrib->attribute.talk_listen.
						    TSpec.MaxIntervalFrames =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[18];
						attrib->attribute.talk_listen.
						    TSpec.
						    MaxIntervalFrames <<= 8;
						attrib->attribute.talk_listen.
						    TSpec.MaxIntervalFrames |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[19];

						attrib->attribute.talk_listen.
						    PriorityAndRank =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[20];

						attrib->attribute.talk_listen.
						    AccumulatedLatency =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[21];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[22];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[23];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[24];

						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

#if LOG_MSRP
						mrpd_log_printf
						    ("MSRP msrp_recv_msg() TalkerAdv[%d][%d] attrib %d\n",
						     vectidx, vectevt_idx,
						     vectevt[vectevt_idx]);
#endif

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							msrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							msrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							msrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							msrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							msrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							msrp_event
							    (MRP_EVENT_RLV,
							     attrib);
							break;
						default:
							free(attrib);
							break;
						}
					}
					numvalues -= numvalues_processed;
				}

				/* 2 byte numvalues + 25 byte FirstValue + (n) vector bytes */
				mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 27 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		case MSRP_TALKER_FAILED_TYPE:
			if (mrpdu_msg->AttributeLength != 34) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}

			/* MSRP uses AttributeListLength ...  */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			/* check attribute list length is reasonable */
			if (!msrp_recv_msg_check_attrib_list_len(mrpdu_msg->AttributeType, &mrpdu_msg->Data[0], mrpdu_msg_eof)) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				mrpdu_msg_ptr =
				    (uint8_t *) mrpdu_vectorptr;
				/* skip this null attribute ... some switches generate these ... */
				/* 2 byte numvalues + 34 byte FirstValue + (0) vector bytes */
				mrpdu_msg_ptr += 2 + 34 + (numvalues + 2) / 3;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *)
				    mrpdu_msg_ptr;
				continue;
			}

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));
#if LOG_MSRP
				mrpd_log_printf
				    ("MSRP msrp_recv_msg() TALKER FAILED vector addr 0x%X, n = %d, VectorHeader %d\n",
				     ((int)mrpdu_vectorptr) & 0x3, numvalues,
				     ntohs(mrpdu_vectorptr->VectorHeader));
#endif

				if (MRPDU_VECT_LVA(ntohs(mrpdu_vectorptr->VectorHeader)) && !saw_talkerfailed_lva) {
					saw_talkerfailed_lva = 1;
					attrib = msrp_alloc();
					if (NULL == attrib)
						goto out;	/* oops - internal error */

					attrib->type = 	MSRP_TALKER_FAILED_TYPE;				
					msrp_event(MRP_EVENT_RLA, attrib);
					free(attrib);
				}

				if (0 == numvalues) {
					/* 2 byte numvalues + 34 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (uint8_t *) mrpdu_vectorptr;
					mrpdu_msg_ptr += 36;
					mrpdu_vectorptr =
					    (mrpdu_vectorattrib_t *)
					    mrpdu_msg_ptr;
					continue;
				}
				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof)
					/* Malformed - runs off the end of the pdu */
					goto out;

				memcpy(streamid_firstval,
				       mrpdu_vectorptr->FirstValue_VectorEvents,
				       8);
				memcpy(destmac_firstval,
				       &
				       (mrpdu_vectorptr->FirstValue_VectorEvents
					[8]), 6);

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 34;
				     vectidx <= (numvectorbytes + 34);
				     vectidx++) {
					vect_3pack =
					    mrpdu_vectorptr->
					    FirstValue_VectorEvents[vectidx];
					vectevt[0] = vect_3pack / 36;
					vectevt[1] =
					    (vect_3pack - vectevt[0] * 36) / 6;
					vectevt[2] =
					    vect_3pack - (36 * vectevt[0]) -
					    (6 * vectevt[1]);

					numvalues_processed =
					    (numvalues > 3 ? 3 : numvalues);

					/* check for out of range encoding */
					if (vectevt[0] > MRPDU_LV) {
#if LOG_MSRP
						mrpd_log_printf("MSRP msrp_recv_msg() TALKER_FAIL vector discard\n");
#endif
						numvalues -= numvalues_processed;
						continue;
					}

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						attrib = msrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type =
						    MSRP_TALKER_FAILED_TYPE;

						/* NOTE this isn't from us */
						attrib->direction =
						    MSRP_DIRECTION_LISTENER;

						memcpy(attrib->attribute.
						       talk_listen.StreamID,
						       streamid_firstval, 8);
						memcpy(attrib->attribute.
						       talk_listen.
						       DataFrameParameters.
						       Dest_Addr,
						       destmac_firstval, 6);

						msrp_increment_streamid
						    (streamid_firstval);
						mmrp_increment_macaddr
						    (destmac_firstval);

						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[14];
						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID <<= 8;
						attrib->attribute.talk_listen.
						    DataFrameParameters.
						    Vlan_ID |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[15];

						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[16];
						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize <<= 8;
						attrib->attribute.talk_listen.
						    TSpec.MaxFrameSize |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[17];

						attrib->attribute.talk_listen.
						    TSpec.MaxIntervalFrames =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[18];
						attrib->attribute.talk_listen.
						    TSpec.
						    MaxIntervalFrames <<= 8;
						attrib->attribute.talk_listen.
						    TSpec.MaxIntervalFrames |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[19];

						attrib->attribute.talk_listen.
						    PriorityAndRank =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[20];

						attrib->attribute.talk_listen.
						    AccumulatedLatency =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[21];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[22];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[23];
						attrib->attribute.talk_listen.
						    AccumulatedLatency <<= 8;
						attrib->attribute.talk_listen.
						    AccumulatedLatency |=
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[24];

						memcpy(attrib->attribute.
						       talk_listen.
						       FailureInformation.
						       BridgeID,
						       &(mrpdu_vectorptr->
							 FirstValue_VectorEvents
							 [25]), 8);

						attrib->attribute.talk_listen.
						    FailureInformation.
						    FailureCode =
						    mrpdu_vectorptr->
						    FirstValue_VectorEvents[33];

						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							msrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							msrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							msrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							msrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							msrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							msrp_event
							    (MRP_EVENT_RLV,
							     attrib);
							break;
						default:
							free(attrib);
							break;
						}
					}
					numvalues -= numvalues_processed;
				}

				/* 2 byte numvalues + 34 byte FirstValue + (n) vector bytes */
				mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 36 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		default:
			/* unrecognized attribute type
			 * we can seek for an endmark to recover .. but this version
			 * dumps the entire packet as malformed
			 */
#if LOG_MSRP
			printf
			    ("################## unrecognized attribute type (%d)\n",
			     mrpdu_msg->AttributeType);
#endif
			goto out;
		}
	}

	if (listener_vectevt)
		free(listener_vectevt);

	free(msgbuf);
	return 0;
 out:
	if (listener_vectevt)
		free(listener_vectevt);

	free(msgbuf);

	return -1;
}

int
msrp_emit_domainvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
			int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	uint16_t numvalues;
	uint8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	uint8_t srclassID_firstval;
	uint8_t srclassprio_firstval;
	uint16_t srclassvid_firstval;
	int attriblistlen;
	struct msrp_attribute *attrib;
/* pending review and deletion */
#ifdef MSRP_AGGREGATE_DOMAINS_VECTORS
	struct msrp_attribute *vattrib;
#endif
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;
	unsigned int attrib_found_flag = 0;
	unsigned int vector_size = 5;

	/* need at least 5 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - vector_size))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MSRP_DOMAIN_TYPE;
	mrpdu_msg->AttributeLength = 4;

	attrib = MSRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - vector_size - MRPDU_ENDMARK_SZ)) && (NULL != attrib)) {

		if (MSRP_DOMAIN_TYPE != attrib->type) {
			attrib = attrib->next;
			continue;
		}

		if (0 == attrib->applicant.tx) {
			attrib = attrib->next;
			continue;
		}
		attrib->applicant.tx = 0;
		if (MRP_ENCODE_OPTIONAL == attrib->applicant.encode) {
			attrib = attrib->next;
			continue;
		}

		attrib_found_flag = 1;
		/* pointing to at least one attribute which needs to be transmitted */
		srclassID_firstval = attrib->attribute.domain.SRclassID;
		srclassprio_firstval = attrib->attribute.domain.SRclassPriority;
		srclassvid_firstval = attrib->attribute.domain.SRclassVID;

		mrpdu_vectorptr->FirstValue_VectorEvents[0] =
		    srclassID_firstval;
		mrpdu_vectorptr->FirstValue_VectorEvents[1] =
		    srclassprio_firstval;
		mrpdu_vectorptr->FirstValue_VectorEvents[2] =
		    (uint8_t) (srclassvid_firstval >> 8);
		mrpdu_vectorptr->FirstValue_VectorEvents[3] =
		    (uint8_t) srclassvid_firstval;

		switch (attrib->applicant.sndmsg) {
		case MRP_SND_IN:
			/*
			 * If 'In' in indicated by the applicant attribute, the
			 * look at the registrar state to determine whether to
			 * send an In (if registrar is also In) or an Mt if the
			 * registrar is either Mt or Lv.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_IN;
			else
				vectevt[0] = MRPDU_MT;
			break;
		case MRP_SND_NEW:
			vectevt[0] = MRPDU_NEW;
			break;
		case MRP_SND_LV:
			vectevt[0] = MRPDU_LV;
			break;
		case MRP_SND_JOIN:
			/* IF 'Join' in indicated by the applicant, look at
			 * the corresponding registrar state to determine whether
			 * to send a JoinIn (if the registar state is 'In') or
			 * a JoinMt if the registrar state is MT or LV.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_JOININ;
			else
				vectevt[0] = MRPDU_JOINMT;
			break;
		default:
			/* huh? */
			goto oops;
			break;
		}

		vectevt_idx = 1;
		numvalues = 1;
		vectevt[1] = 0;
		vectevt[2] = 0;

		/* now attempt to vectorize contiguous other attributes
		 * which also need to be transmitted
		 */

		vectidx = 4;

/* pending review and deletion */
#ifdef MSRP_AGGREGATE_DOMAINS_VECTORS
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (MSRP_DOMAIN_TYPE != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			srclassID_firstval++;
			srclassprio_firstval++;

			if (srclassID_firstval !=
			    vattrib->attribute.domain.SRclassID)
				break;

			vattrib->applicant.tx = 0;

			switch (vattrib->applicant.sndmsg) {
			case MRP_SND_IN:
				/*
				 * If 'In' in indicated by the applicant attribute, the
				 * look at the registrar state to determine whether to
				 * send an In (if registrar is also In) or an Mt if the
				 * registrar is either Mt or Lv.
				 */
				if (MRP_IN_STATE ==
				    vattrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_IN;
				else
					vectevt[vectevt_idx] = MRPDU_MT;
				break;
			case MRP_SND_NEW:
				vectevt[vectevt_idx] = MRPDU_NEW;
				break;
			case MRP_SND_LV:
				vectevt[vectevt_idx] = MRPDU_LV;
				break;
			case MRP_SND_JOIN:
				/* IF 'Join' in indicated by the applicant, look at
				 * the corresponding registrar state to determine whether
				 * to send a JoinIn (if the registar state is 'In') or
				 * a JoinMt if the registrar state is MT or LV.
				 */
				if (MRP_IN_STATE == attrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_JOININ;
				else
					vectevt[vectevt_idx] = MRPDU_JOINMT;
				break;
			default:
				/* huh? */
				goto oops;
				break;
			}

			vectevt_idx++;
			numvalues++;

			if (vectevt_idx > 2) {
				vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
								vectevt[1],
								vectevt[2]);

				mrpdu_vectorptr->FirstValue_VectorEvents
				    [vectidx] = vect_3pack;
				vectidx++;
				vectevt[0] = 0;
				vectevt[1] = 0;
				vectevt[2] = 0;
				vectevt_idx = 0;
			}

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
				goto oops;

			vattrib = vattrib->next;
		}
#endif

		/* handle any trailers */
		if (vectevt_idx > 0) {
			vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
							vectevt[1], vectevt[2]);

			mrpdu_vectorptr->FirstValue_VectorEvents[vectidx] =
			    vect_3pack;
			vectidx++;
		}

		if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]) >
		    (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
			goto oops;

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(numvalues);

		if (lva) {
			mrpdu_vectorptr->VectorHeader |= MRPDU_VECT_LVA_FLAG;
			lva = 0;
		}

		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;

		attrib = attrib->next;

	}

	/*
	 * If no attributes are declared, send a LeaveAll with an all 0
	 * FirstValue, Number of Values set to 0 and not attribute event.
	 */
	if ((0 == attrib_found_flag) && MSRP_db->send_empty_LeaveAll_flag) {

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(0) |
						MRPDU_VECT_LVA_FLAG;
		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[mrpdu_msg->AttributeLength]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}

	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2])) {
		*bytes_used = 0;
		return 0;
	}

	/* endmark */
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;

	*bytes_used = (mrpdu_msg_ptr - msgbuf);

	attriblistlen = mrpdu_msg_ptr - &(mrpdu_msg->Data[2]);
	mrpdu_msg->Data[0] = (uint8_t) (attriblistlen >> 8);
	mrpdu_msg->Data[1] = (uint8_t) attriblistlen;

#if LOG_MSRP
	mrpd_log_printf("MSRP msrp_emit_domainvectors() DONE (OK)\n");
#endif
	return 0;
 oops:
#if LOG_MSRP
	mrpd_log_printf("MSRP msrp_emit_domainvectors() DONE (OOPS)\n");
#endif
	/* an internal error - caller should assume TXLAF */
	*bytes_used = 0;
	return -1;
}

/*
 * Below works for both talker and talkerFailed because talker has
 * FailureInformation explicitly set to zero.
 */
static int vectorize_talker(struct msrp_attribute *first_attrib,
			uint8_t streamid_firstval[8],
			uint8_t mac_firstval[6],
			struct msrp_attribute *candidate_attrib)
{
	struct msrpdu_talker_fail  first;

	/*
	 * Make a copy of the "first" attrib and update ID and MAC with new
	 * values before comparing
	 */
	first = first_attrib->attribute.talk_listen;
	memcpy(first.StreamID, streamid_firstval, 8);
	memcpy(first.DataFrameParameters.Dest_Addr, mac_firstval, 6);
	return (memcmp(&first, &candidate_attrib->attribute.talk_listen,
			sizeof(first)) == 0);
}

int
msrp_emit_talkervectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
		      int *bytes_used, int lva, unsigned int type)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	uint16_t numvalues;
	uint8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	uint8_t streamid_firstval[8];
	uint8_t destmac_firstval[6];
	uint16_t tspec_maxframesize_firstval;
	int attriblistlen;
	struct msrp_attribute *attrib, *vattrib;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;
	unsigned int attrib_found_flag = 0;
	unsigned int vector_size = 0;
	unsigned int attrib_len = 0;

	/* MSRP_TALKER_ADV_TYPE */
	vector_size = 28;
	attrib_len = 25;
	if (MSRP_TALKER_FAILED_TYPE == type) {
		/* talker failed */
		vector_size += 9;
		attrib_len += 9;		
	}

	/* need at least 28 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - vector_size))
		goto oops;

	/*
	 * as an endpoint, we don't advertise talker failed attributes (which
	 * only a listener should receive) - but since this daemon could
	 * function dual-mode - listening to streams as well as being a talker
	 * station - we only advertise the talker vectors for declared streams,
	 * not those we know about as a listener ...
	 */
	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = type;
	mrpdu_msg->AttributeLength = attrib_len;

	attrib = MSRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - vector_size - MRPDU_ENDMARK_SZ)) && (NULL != attrib)) {

		if (type != attrib->type) {
			attrib = attrib->next;
			continue;
		}
#ifdef CHECK
		if (MSRP_DIRECTION_LISTENER == attrib->direction) {
			attrib = attrib->next;
			continue;
		}
#endif
		if (0 == attrib->applicant.tx) {
			attrib = attrib->next;
			continue;
		}
		attrib->applicant.tx = 0;
		if (MRP_ENCODE_OPTIONAL == attrib->applicant.encode) {
			attrib = attrib->next;
			continue;
		}

		attrib_found_flag = 1;
		/* pointing to at least one attribute which needs to be transmitted */
		memcpy(streamid_firstval,
		       attrib->attribute.talk_listen.StreamID, 8);
		memcpy(destmac_firstval,
		       attrib->attribute.talk_listen.DataFrameParameters.
		       Dest_Addr, 6);
		memcpy(mrpdu_vectorptr->FirstValue_VectorEvents,
		       streamid_firstval, 8);
		memcpy(&(mrpdu_vectorptr->FirstValue_VectorEvents[8]),
		       destmac_firstval, 6);

		mrpdu_vectorptr->FirstValue_VectorEvents[14] =
		    attrib->attribute.talk_listen.DataFrameParameters.
		    Vlan_ID >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[15] =
		    (uint8_t) attrib->attribute.talk_listen.
		    DataFrameParameters.Vlan_ID;

		tspec_maxframesize_firstval = attrib->attribute.talk_listen.TSpec.MaxFrameSize;
		mrpdu_vectorptr->FirstValue_VectorEvents[16] =
		    tspec_maxframesize_firstval >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[17] =
		    (uint8_t)tspec_maxframesize_firstval;

		mrpdu_vectorptr->FirstValue_VectorEvents[18] =
		    attrib->attribute.talk_listen.TSpec.MaxIntervalFrames >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[19] =
		    (uint8_t) attrib->attribute.talk_listen.
		    TSpec.MaxIntervalFrames;

		mrpdu_vectorptr->FirstValue_VectorEvents[20] =
		    attrib->attribute.talk_listen.PriorityAndRank;

		mrpdu_vectorptr->FirstValue_VectorEvents[21] =
		    attrib->attribute.talk_listen.AccumulatedLatency >> 24;
		mrpdu_vectorptr->FirstValue_VectorEvents[22] =
		    attrib->attribute.talk_listen.AccumulatedLatency >> 16;
		mrpdu_vectorptr->FirstValue_VectorEvents[23] =
		    attrib->attribute.talk_listen.AccumulatedLatency >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[24] =
		    attrib->attribute.talk_listen.AccumulatedLatency;

		if (MSRP_TALKER_FAILED_TYPE == type) {
			memcpy(&mrpdu_vectorptr->FirstValue_VectorEvents[25],
			       attrib->attribute.talk_listen.FailureInformation.BridgeID, 8);
			mrpdu_vectorptr->FirstValue_VectorEvents[33] =
			       attrib->attribute.talk_listen.FailureInformation.FailureCode;
		}

		switch (attrib->applicant.sndmsg) {
		case MRP_SND_IN:
			/*
			 * If 'In' in indicated by the applicant attribute, the
			 * look at the registrar state to determine whether to
			 * send an In (if registrar is also In) or an Mt if the
			 * registrar is either Mt or Lv.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_IN;
			else
				vectevt[0] = MRPDU_MT;
			break;
		case MRP_SND_NEW:
			vectevt[0] = MRPDU_NEW;
			break;
		case MRP_SND_LV:
			vectevt[0] = MRPDU_LV;
			break;
		case MRP_SND_JOIN:
			/* IF 'Join' in indicated by the applicant, look at
			 * the corresponding registrar state to determine whether
			 * to send a JoinIn (if the registar state is 'In') or
			 * a JoinMt if the registrar state is MT or LV.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_JOININ;
			else
				vectevt[0] = MRPDU_JOINMT;
			break;
		default:
			/* huh? */
			goto oops;
			break;
		}

		vectevt_idx = 1;
		numvalues = 1;
		vectevt[1] = 0;
		vectevt[2] = 0;

		/* now attempt to vectorize contiguous other attributes
		 * which also need to be transmitted
		 */

		vectidx = attrib_len;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (type != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			msrp_increment_streamid(streamid_firstval);
			mmrp_increment_macaddr(destmac_firstval);

			if (!vectorize_talker(attrib,
					streamid_firstval,
					destmac_firstval,
					vattrib))
				break;

			vattrib->applicant.tx = 0;

			switch (vattrib->applicant.sndmsg) {
			case MRP_SND_IN:
				/*
				 * If 'In' in indicated by the applicant attribute, the
				 * look at the registrar state to determine whether to
				 * send an In (if registrar is also In) or an Mt if the
				 * registrar is either Mt or Lv.
				 */
				if (MRP_IN_STATE ==
				    vattrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_IN;
				else
					vectevt[vectevt_idx] = MRPDU_MT;
				break;
			case MRP_SND_NEW:
				vectevt[vectevt_idx] = MRPDU_NEW;
				break;
			case MRP_SND_LV:
				vectevt[vectevt_idx] = MRPDU_LV;
				break;
			case MRP_SND_JOIN:
				/* If 'Join' in indicated by the applicant, look at
				 * the corresponding registrar state to determine whether
				 * to send a JoinIn (if the registar state is 'In') or
				 * a JoinMt if the registrar state is MT or LV.
				 */
				if (MRP_IN_STATE == vattrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_JOININ;
				else
					vectevt[vectevt_idx] = MRPDU_JOINMT;
				break;
			default:
				/* huh? */
				goto oops;
				break;
			}

			vectevt_idx++;
			numvalues++;

			if (vectevt_idx > 2) {
				vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
								vectevt[1],
								vectevt[2]);

				mrpdu_vectorptr->FirstValue_VectorEvents
				    [vectidx] = vect_3pack;
				vectidx++;
				vectevt[0] = 0;
				vectevt[1] = 0;
				vectevt[2] = 0;
				vectevt_idx = 0;
			}

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
				goto oops;

			vattrib = vattrib->next;
		}

		/* handle any trailers */
		if (vectevt_idx > 0) {
			vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
							vectevt[1], vectevt[2]);

			mrpdu_vectorptr->FirstValue_VectorEvents[vectidx] =
			    vect_3pack;
			vectidx++;
		}

		if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]) >
		    (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
			goto oops;

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(numvalues);

		if (lva) {
			mrpdu_vectorptr->VectorHeader |= MRPDU_VECT_LVA_FLAG;
			lva = 0;
		}

		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;

		attrib = attrib->next;

	}

	/*
	 * If no attributes are declared, send a LeaveAll with an all 0
	 * FirstValue, Number of Values set to 0 and not attribute event.
	 */
	if ((0 == attrib_found_flag) && MSRP_db->send_empty_LeaveAll_flag) {

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(0) |
						MRPDU_VECT_LVA_FLAG;
		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[mrpdu_msg->AttributeLength]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}

	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2])) {
		*bytes_used = 0;
		return 0;
	}

	/* endmark */
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;

	*bytes_used = (mrpdu_msg_ptr - msgbuf);

	attriblistlen = mrpdu_msg_ptr - &(mrpdu_msg->Data[2]);
	mrpdu_msg->Data[0] = (uint8_t) (attriblistlen >> 8);
	mrpdu_msg->Data[1] = (uint8_t) attriblistlen;
	return 0;
 oops:
	/* an internal error - caller should assume TXLAF */
	*bytes_used = 0;
	return -1;
}

int
msrp_emit_listenvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
			int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;
	uint16_t numvalues;
	uint8_t vect_3pack;
	uint8_t vect_4pack;
	int vectidx;
	int attriblistlen;
	int vectevt[4];
	int vectevt_idx;
	int *listen_declare = NULL;
	int listen_declare_sz = 64;
	int listen_declare_idx = 0;
	int listen_declare_end = 0;
	uint8_t streamid_firstval[8];
	struct msrp_attribute *attrib, *vattrib;
	unsigned int vector_size = 13;
	int mac_eq;
	unsigned int attrib_found_flag = 0;

	/* need at least 13 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - vector_size))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MSRP_LISTENER_TYPE;
	mrpdu_msg->AttributeLength = 8;

	listen_declare = (int *)malloc(listen_declare_sz * sizeof(int));
	if (NULL == listen_declare)
		goto oops;

	/* if we have a listener type registered, always send out an update,
	 * so mark all as ready.
	 */
	attrib = MSRP_db->attrib_list;
	while (attrib) {
		if (MSRP_LISTENER_TYPE == attrib->type) {
			attrib->applicant.tx = 1;
		}
		attrib = attrib->next;
	}

	attrib = MSRP_db->attrib_list;
	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2]);

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - vector_size -MRPDU_ENDMARK_SZ)) && (NULL != attrib)) {

		if (MSRP_LISTENER_TYPE != attrib->type) {
			attrib = attrib->next;
			continue;
		}

		if (0 == attrib->applicant.tx) {
			attrib = attrib->next;
			continue;
		}

		attrib->applicant.tx = 0;
		if (MRP_ENCODE_OPTIONAL == attrib->applicant.encode) {
			attrib = attrib->next;
			continue;
		}

		listen_declare_idx = 0;

		/* if we have a listener type registered, we always will
		 * send out an update. We also don't bother trying to
		 * insert IGNORES between stream IDs - we only send out
		 * contiguous active streams in messages
		 */

		attrib_found_flag = 1;
		/* pointing to at least one attribute which needs to be transmitted */
		memcpy(streamid_firstval,
		       attrib->attribute.talk_listen.StreamID, 8);
		memcpy(&(mrpdu_vectorptr->FirstValue_VectorEvents),
		       attrib->attribute.talk_listen.StreamID, 8);

		switch (attrib->applicant.sndmsg) {
		case MRP_SND_IN:
			/*
			 * If 'In' in indicated by the applicant attribute, the
			 * look at the registrar state to determine whether to
			 * send an In (if registrar is also In) or an Mt if the
			 * registrar is either Mt or Lv.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_IN;
			else
				vectevt[0] = MRPDU_MT;
			break;
		case MRP_SND_NEW:
			vectevt[0] = MRPDU_NEW;
			break;
		case MRP_SND_LV:
			vectevt[0] = MRPDU_LV;
			break;
		case MRP_SND_JOIN:
			/* IF 'Join' in indicated by the applicant, look at
			 * the corresponding registrar state to determine whether
			 * to send a JoinIn (if the registar state is 'In') or
			 * a JoinMt if the registrar state is MT or LV.
			 */
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				vectevt[0] = MRPDU_JOININ;
			else
				vectevt[0] = MRPDU_JOINMT;
			break;
		default:
			/* huh? */
			goto oops;
			break;
		}

		vectevt_idx = 1;
		numvalues = 1;
		vectevt[1] = 0;
		vectevt[2] = 0;

		if (listen_declare_sz == (listen_declare_idx + 1)) {
			listen_declare =
			    (int *)realloc(listen_declare,
					   (64 +
					    listen_declare_sz) * sizeof(int));
			if (NULL == listen_declare)
				goto oops;

			listen_declare_sz += 64;
		}

		listen_declare[listen_declare_idx] = attrib->substate;
		listen_declare_idx++;

		/* now attempt to vectorize contiguous other attributes
		 * which also need to be transmitted
		 */

		vectidx = 8;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (MSRP_LISTENER_TYPE != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			msrp_increment_streamid(streamid_firstval);

			mac_eq = memcmp(vattrib->attribute.talk_listen.StreamID,
					streamid_firstval, 8);

			if (0 != mac_eq)
				break;

			vattrib->applicant.tx = 0;

			switch (vattrib->applicant.sndmsg) {
			case MRP_SND_IN:
				/*
				 * If 'In' in indicated by the applicant attribute, the
				 * look at the registrar state to determine whether to
				 * send an In (if registrar is also In) or an Mt if the
				 * registrar is either Mt or Lv.
				 */
				if (MRP_IN_STATE ==
				    vattrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_IN;
				else
					vectevt[vectevt_idx] = MRPDU_MT;
				break;
			case MRP_SND_NEW:
				vectevt[vectevt_idx] = MRPDU_NEW;
				break;
			case MRP_SND_LV:
				vectevt[vectevt_idx] = MRPDU_LV;
				break;
			case MRP_SND_JOIN:
				/* IF 'Join' in indicated by the applicant, look at
				 * the corresponding registrar state to determine whether
				 * to send a JoinIn (if the registar state is 'In') or
				 * a JoinMt if the registrar state is MT or LV.
				 */
				if (MRP_IN_STATE == vattrib->registrar.mrp_state)
					vectevt[vectevt_idx] = MRPDU_JOININ;
				else
					vectevt[vectevt_idx] = MRPDU_JOINMT;
				break;
			default:
				/* huh? */
				goto oops;
				break;
			}

			vectevt_idx++;
			numvalues++;

			if (listen_declare_sz == (listen_declare_idx + 1)) {
				listen_declare =
				    (int *)realloc(listen_declare,
						   (64 +
						    listen_declare_sz) *
						   sizeof(int));
				listen_declare_sz += 64;
			}

			listen_declare[listen_declare_idx] = vattrib->substate;
			listen_declare_idx++;

			if (vectevt_idx > 2) {
				vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
								vectevt[1],
								vectevt[2]);

				mrpdu_vectorptr->FirstValue_VectorEvents
				    [vectidx] = vect_3pack;
				vectidx++;
				vectevt[0] = 0;
				vectevt[1] = 0;
				vectevt[2] = 0;
				vectevt_idx = 0;
			}

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
				goto oops;

			vattrib = vattrib->next;
		}

		/* handle any trailers */
		if (vectevt_idx > 0) {
			vect_3pack = MRPDU_3PACK_ENCODE(vectevt[0],
							vectevt[1], vectevt[2]);

			mrpdu_vectorptr->FirstValue_VectorEvents[vectidx] =
			    vect_3pack;
			vectidx++;
		}

		/* now emit the 4-packed events */

		listen_declare_end = listen_declare_idx;

		for (listen_declare_idx = 0;
		     listen_declare_idx < ((listen_declare_end / 4) * 4);
		     listen_declare_idx += 4) {

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
				goto oops;

			vect_4pack =
			    MRPDU_4PACK_ENCODE(listen_declare
					       [listen_declare_idx],
					       listen_declare
					       [listen_declare_idx + 1],
					       listen_declare
					       [listen_declare_idx + 2],
					       listen_declare
					       [listen_declare_idx + 3]);

			mrpdu_vectorptr->FirstValue_VectorEvents
			    [vectidx] = vect_4pack;
			vectidx++;

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - 3))
				goto oops;
		}

		/* handle any trailers */
		vectevt_idx = 0;
		vectevt[0] = 0;
		vectevt[1] = 0;
		vectevt[2] = 0;
		vectevt[3] = 0;
		while (listen_declare_idx < listen_declare_end) {
			vectevt[vectevt_idx] =
			    listen_declare[listen_declare_idx];
			listen_declare_idx++;
			vectevt_idx++;
		}
		if (vectevt_idx) {
			vect_4pack = MRPDU_4PACK_ENCODE(vectevt[0],
							vectevt[1],
							vectevt[2], vectevt[3]);

			mrpdu_vectorptr->FirstValue_VectorEvents[vectidx] =
			    vect_4pack;
			vectidx++;
		}

		if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]) >
		    (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
			goto oops;

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(numvalues);

		if (lva) {
			mrpdu_vectorptr->VectorHeader |= MRPDU_VECT_LVA_FLAG;
			lva = 0;
		}

		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}

	/*
	 * If no attributes are declared, send a LeaveAll with an all 0
	 * FirstValue, Number of Values set to 0 and not attribute event.
	 */
	if ((0 == attrib_found_flag) && MSRP_db->send_empty_LeaveAll_flag) {

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(0) |
						MRPDU_VECT_LVA_FLAG;
		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[mrpdu_msg->AttributeLength]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}


	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *) & (mrpdu_msg->Data[2])) {
		if (listen_declare)
			free(listen_declare);
		*bytes_used = 0;
		return 0;
	}

	/* endmark */
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;

	*bytes_used = (mrpdu_msg_ptr - msgbuf);

	attriblistlen = mrpdu_msg_ptr - &(mrpdu_msg->Data[2]);
	mrpdu_msg->Data[0] = (uint8_t) (attriblistlen >> 8);
	mrpdu_msg->Data[1] = (uint8_t) attriblistlen;

	free(listen_declare);
	return 0;
 oops:
	/* an internal error - caller should assume TXLAF */
	if (listen_declare)
		free(listen_declare);
	*bytes_used = 0;
	return -1;
}

int msrp_txpdu(void)
{
	unsigned char *msgbuf, *msgbuf_wrptr;
	int msgbuf_len;
	int bytes = 0;
	eth_hdr_t *eth;
	mrpdu_t *mrpdu;
	unsigned char *mrpdu_msg_ptr;
	unsigned char *mrpdu_msg_eof;
	int rc;
	int lva = 0;

	msgbuf = (unsigned char *)malloc(MAX_FRAME_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, MAX_FRAME_SIZE);
	msgbuf_len = 0;

	msgbuf_wrptr = msgbuf;

	eth = (eth_hdr_t *) msgbuf_wrptr;

	/* note that MSRP frames should always be untagged (no vlan) */
	eth->typelen = htons(MSRP_ETYPE);
	memcpy(eth->destaddr, MSRP_ADDR, sizeof(eth->destaddr));
	memcpy(eth->srcaddr, STATION_ADDR, sizeof(eth->srcaddr));

	msgbuf_wrptr += sizeof(eth_hdr_t);

	mrpdu = (mrpdu_t *) msgbuf_wrptr;

	mrpdu->ProtocolVersion = MSRP_PROT_VER;
	mrpdu_msg_ptr = MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu);
	mrpdu_msg_eof = (unsigned char *)msgbuf + MAX_FRAME_SIZE;

	if (MSRP_db->mrp_db.lva.tx) {
		lva = 1;
		MSRP_db->mrp_db.lva.tx = 0;
	}

	rc = msrp_emit_talkervectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva, MSRP_TALKER_ADV_TYPE);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	rc = msrp_emit_talkervectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva, MSRP_TALKER_FAILED_TYPE);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	if (mrpdu_msg_ptr >= (mrpdu_msg_eof - MRPDU_ENDMARK_SZ))
		goto out;

	rc = msrp_emit_listenvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	rc = msrp_emit_domainvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	if (mrpdu_msg_ptr == MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu)) {
		goto out;	/* nothing to send */
	}

	/* endmark */
	if (mrpdu_msg_ptr < (mrpdu_msg_eof - MRPDU_ENDMARK_SZ)) {
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
	} else
		goto out;

	msgbuf_len = mrpdu_msg_ptr - msgbuf;

	bytes = mrpd_send(msrp_socket, msgbuf, msgbuf_len, 0);
#if LOG_MSRP
	mrpd_log_printf("MSRP send PDU\n");
#endif
	if (bytes <= 0) {
#if LOG_ERRORS
		fprintf(stderr, "%s - Error on send %s", __FUNCTION__, strerror(errno));
#endif
		goto out;
	}

	free(msgbuf);
	return 0;
 out:
	free(msgbuf);
	/* caller should assume TXLAF */
	return -1;
}

int msrp_send_notifications(struct msrp_attribute *attrib, int notify)
{
	char *msgbuf;
	char *variant;
	char *regsrc;
	char mrp_state[8];
	client_t *client;
	size_t sub_str_len = 128;

	if (NULL == attrib)
		return -1;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return -1;

	variant = regsrc = NULL;

	variant = (char *)malloc(sub_str_len);
	regsrc = (char *)malloc(sub_str_len);

	if ((NULL == variant) || (NULL == regsrc))
		goto free_msgbuf;

	memset(msgbuf, 0, MAX_MRPD_CMDSZ);

	if (MSRP_LISTENER_TYPE == attrib->type) {
		snprintf(variant, sub_str_len - 1, "L:D=%d,S=%02x%02x%02x%02x%02x%02x%02x%02x",
			attrib->substate,
			attrib->attribute.talk_listen.StreamID[0],
			attrib->attribute.talk_listen.StreamID[1],
			attrib->attribute.talk_listen.StreamID[2],
			attrib->attribute.talk_listen.StreamID[3],
			attrib->attribute.talk_listen.StreamID[4],
			attrib->attribute.talk_listen.StreamID[5],
			attrib->attribute.talk_listen.StreamID[6],
			attrib->attribute.talk_listen.StreamID[7]);
	} else if (MSRP_DOMAIN_TYPE == attrib->type) {
		snprintf(variant, sub_str_len - 1, "D:C=%d,P=%d,V=%04x,N=%d",
			attrib->attribute.domain.SRclassID,
			attrib->attribute.domain.SRclassPriority,
			attrib->attribute.domain.SRclassVID,
			attrib->attribute.domain.neighborSRclassPriority
			);
	} else if (MSRP_TALKER_ADV_TYPE == attrib->type) {
		snprintf(variant, sub_str_len - 1, "T:S=%02x%02x%02x%02x%02x%02x%02x%02x"
			",A=%02x%02x%02x%02x%02x%02x"
			",V=%04x"
			",Z=%d"
			",I=%d"
			",P=%d"
			",L=%d",
			attrib->attribute.talk_listen.StreamID[0],
			attrib->attribute.talk_listen.StreamID[1],
			attrib->attribute.talk_listen.StreamID[2],
			attrib->attribute.talk_listen.StreamID[3],
			attrib->attribute.talk_listen.StreamID[4],
			attrib->attribute.talk_listen.StreamID[5],
			attrib->attribute.talk_listen.StreamID[6],
			attrib->attribute.talk_listen.StreamID[7],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[0],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[1],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[2],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[3],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[4],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[5],
			attrib->attribute.talk_listen.DataFrameParameters.
			Vlan_ID,
			attrib->attribute.talk_listen.TSpec.MaxFrameSize,
			attrib->attribute.talk_listen.TSpec.MaxIntervalFrames,
			attrib->attribute.talk_listen.PriorityAndRank,
			attrib->attribute.talk_listen.AccumulatedLatency);
	} else {
		snprintf(variant, sub_str_len - 1, "T:S=%02x%02x%02x%02x%02x%02x%02x%02x"
			",A=%02x%02x%02x%02x%02x%02x"
			",V=%04x"
			",Z=%d"
			",I=%d"
			",P=%d"
			",L=%d"
			",B=%02x%02x%02x%02x%02x%02x%02x%02x"
			",C=%d",
			attrib->attribute.talk_listen.StreamID[0],
			attrib->attribute.talk_listen.StreamID[1],
			attrib->attribute.talk_listen.StreamID[2],
			attrib->attribute.talk_listen.StreamID[3],
			attrib->attribute.talk_listen.StreamID[4],
			attrib->attribute.talk_listen.StreamID[5],
			attrib->attribute.talk_listen.StreamID[6],
			attrib->attribute.talk_listen.StreamID[7],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[0],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[1],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[2],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[3],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[4],
			attrib->attribute.talk_listen.DataFrameParameters.
			Dest_Addr[5],
			attrib->attribute.talk_listen.DataFrameParameters.
			Vlan_ID,
			attrib->attribute.talk_listen.TSpec.MaxFrameSize,
			attrib->attribute.talk_listen.TSpec.MaxIntervalFrames,
			attrib->attribute.talk_listen.PriorityAndRank,
			attrib->attribute.talk_listen.AccumulatedLatency,
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[0],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[1],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[2],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[3],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[4],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[5],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[6],
			attrib->attribute.talk_listen.FailureInformation.
			BridgeID[7],
			attrib->attribute.talk_listen.FailureInformation.
			FailureCode);
	}

	snprintf(regsrc, sub_str_len - 1, "R=%02x%02x%02x%02x%02x%02x",
		attrib->registrar.macaddr[0],
		attrib->registrar.macaddr[1],
		attrib->registrar.macaddr[2],
		attrib->registrar.macaddr[3],
		attrib->registrar.macaddr[4], attrib->registrar.macaddr[5]);

	mrp_decode_state(&attrib->registrar, &attrib->applicant,
				 mrp_state, sizeof(mrp_state));

	switch (notify) {
	case MRP_NOTIFY_NEW:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "SNE %s %s %s\n", variant, regsrc, mrp_state);
		break;
	case MRP_NOTIFY_JOIN:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "SJO %s %s %s\n", variant, regsrc, mrp_state);
		break;
	case MRP_NOTIFY_LV:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "SLE %s %s %s\n", variant, regsrc, mrp_state);
		break;
	default:
		goto free_msgbuf;
		break;
	}

	client = MSRP_db->mrp_db.clients;
	while (NULL != client) {
		mrpd_send_ctl_msg(&(client->client), msgbuf, MAX_MRPD_CMDSZ);
		client = client->next;
	}

 free_msgbuf:
	if (regsrc)
		free(regsrc);
	if (variant)
		free(variant);
	free(msgbuf);
	return 0;
}

int msrp_dumptable(struct sockaddr_in *client)
{
	char *msgbuf;
	char *msgbuf_wrptr;
	char *stage;
	char *variant;
	char *regsrc;
	struct msrp_attribute *attrib;
	char mrp_state[8];

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return -1;

	stage = variant = regsrc = NULL;

	stage = (char *)malloc(128);
	variant = (char *)malloc(128);
	regsrc = (char *)malloc(128);

	if ((NULL == stage) || (NULL == variant) || (NULL == regsrc))
		goto free_msgbuf;

	memset(msgbuf, 0, MAX_MRPD_CMDSZ);

	msgbuf_wrptr = msgbuf;

	attrib = MSRP_db->attrib_list;
	if (attrib == NULL) {
		sprintf(msgbuf, "MSRP:Empty\n");
	}

	while (NULL != attrib) {
		if (MSRP_LISTENER_TYPE == attrib->type) {
			sprintf(variant,
				"L:D=%d,S=%02x%02x%02x%02x%02x%02x%02x%02x",
				attrib->substate,
				attrib->attribute.talk_listen.StreamID[0],
				attrib->attribute.talk_listen.StreamID[1],
				attrib->attribute.talk_listen.StreamID[2],
				attrib->attribute.talk_listen.StreamID[3],
				attrib->attribute.talk_listen.StreamID[4],
				attrib->attribute.talk_listen.StreamID[5],
				attrib->attribute.talk_listen.StreamID[6],
				attrib->attribute.talk_listen.StreamID[7]);
		} else if (MSRP_DOMAIN_TYPE == attrib->type) {
			sprintf(variant, "D:C=%d,P=%d,V=%04x,N=%d",
				attrib->attribute.domain.SRclassID,
				attrib->attribute.domain.SRclassPriority,
				attrib->attribute.domain.SRclassVID,
				attrib->attribute.domain.neighborSRclassPriority);
		} else {
			sprintf(variant, "T:S=%02x%02x%02x%02x%02x%02x%02x%02x"
				",A=%02x%02x%02x%02x%02x%02x"
				",V=%04x"
				",Z=%d"
				",I=%d"
				",P=%d"
				",L=%d"
				",B=%02x%02x%02x%02x%02x%02x%02x%02x"
				",C=%d",
				attrib->attribute.talk_listen.StreamID[0],
				attrib->attribute.talk_listen.StreamID[1],
				attrib->attribute.talk_listen.StreamID[2],
				attrib->attribute.talk_listen.StreamID[3],
				attrib->attribute.talk_listen.StreamID[4],
				attrib->attribute.talk_listen.StreamID[5],
				attrib->attribute.talk_listen.StreamID[6],
				attrib->attribute.talk_listen.StreamID[7],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[0],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[1],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[2],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[3],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[4],
				attrib->attribute.talk_listen.
				DataFrameParameters.Dest_Addr[5],
				attrib->attribute.talk_listen.
				DataFrameParameters.Vlan_ID,
				attrib->attribute.talk_listen.TSpec.
				MaxFrameSize,
				attrib->attribute.talk_listen.TSpec.
				MaxIntervalFrames,
				attrib->attribute.talk_listen.PriorityAndRank,
				attrib->attribute.talk_listen.
				AccumulatedLatency,
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[0],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[1],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[2],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[3],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[4],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[5],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[6],
				attrib->attribute.talk_listen.
				FailureInformation.BridgeID[7],
				attrib->attribute.talk_listen.
				FailureInformation.FailureCode);
		}

		mrp_decode_state(&attrib->registrar, &attrib->applicant,
				 mrp_state, sizeof(mrp_state));

		sprintf(regsrc, "R=%02x%02x%02x%02x%02x%02x %s",
			attrib->registrar.macaddr[0],
			attrib->registrar.macaddr[1],
			attrib->registrar.macaddr[2],
			attrib->registrar.macaddr[3],
			attrib->registrar.macaddr[4],
			attrib->registrar.macaddr[5], mrp_state);

		sprintf(stage, "%s %s\n", variant, regsrc);

		sprintf(msgbuf_wrptr, "%s", stage);
		msgbuf_wrptr += strlen(stage);
		attrib = attrib->next;
	}

	mrpd_send_ctl_msg(client, msgbuf, MAX_MRPD_CMDSZ);

 free_msgbuf:
	if (regsrc)
		free(regsrc);
	if (variant)
		free(variant);
	if (stage)
		free(stage);
	free(msgbuf);
	return 0;

}

/* S+? - (re)JOIN a stream */
/* S++ - NEW a stream      */
static int msrp_cmd_parse_join_or_new_stream(char *buf, int buflen,
					     struct msrpdu_talker_fail
					     *talker_ad, int *err_index)
{
	struct parse_param specs[] = {
		{"S" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{"A" PARSE_ASSIGN, parse_mac,
		 talker_ad->DataFrameParameters.Dest_Addr},
		{"V" PARSE_ASSIGN, parse_u16_04x,
		 &talker_ad->DataFrameParameters.Vlan_ID},
		{"Z" PARSE_ASSIGN, parse_u16, &talker_ad->TSpec.MaxFrameSize},
		{"I" PARSE_ASSIGN, parse_u16,
		 &talker_ad->TSpec.MaxIntervalFrames},
		{"P" PARSE_ASSIGN, parse_u8, &talker_ad->PriorityAndRank},
		{"L" PARSE_ASSIGN, parse_u32, &talker_ad->AccumulatedLatency},
		{0, parse_null, 0}
	};
	if (buflen < 22)
		return -1;
	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

static int msrp_cmd_parse_join_or_new_stream_failure(char *buf, int buflen,
					     struct msrpdu_talker_fail
					     *talker_ad, int *err_index)
{
	struct parse_param specs[] = {
		{"S" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{"A" PARSE_ASSIGN, parse_mac,
		 talker_ad->DataFrameParameters.Dest_Addr},
		{"V" PARSE_ASSIGN, parse_u16_04x,
		 &talker_ad->DataFrameParameters.Vlan_ID},
		{"Z" PARSE_ASSIGN, parse_u16, &talker_ad->TSpec.MaxFrameSize},
		{"I" PARSE_ASSIGN, parse_u16,
		 &talker_ad->TSpec.MaxIntervalFrames},
		{"P" PARSE_ASSIGN, parse_u8, &talker_ad->PriorityAndRank},
		{"L" PARSE_ASSIGN, parse_u32, &talker_ad->AccumulatedLatency},
		{"B" PARSE_ASSIGN, parse_c64, &talker_ad->FailureInformation.BridgeID},
		{"C" PARSE_ASSIGN, parse_u8, &talker_ad->FailureInformation.FailureCode},
		{0, parse_null, 0}
	};
	if (buflen < 47)
		return -1;
	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

/*
 * Required fields are:
 * talker_ad->StreamID
 * talker_ad->DataFrameParameters.Dest_Addr
 * talker_ad->DataFrameParameters.Vlan_ID
 * talker_ad->TSpec.MaxFrameSize
 * talker_ad->TSpec.MaxIntervalFrames
 * talker_ad->PriorityAndRank
 * talker_ad->AccumulatedLatency
 *
 * For TalkerFaild, also require non-zero values in:
 * talker_ad->FailureInformation.BridgeID
 * talker_ad->FailureInformation.FailureCode
 */

static int msrp_cmd_join_or_new_stream(struct msrpdu_talker_fail *talker_ad,
				       int attrib_type,
				       int mrp_event)
{
	struct msrp_attribute *attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}
	attrib->type = attrib_type;
	attrib->attribute.talk_listen = *talker_ad;
	msrp_event(mrp_event, attrib);
	return 0;
}

/* S-- - LV a stream */
static int msrp_cmd_parse_leave_stream(char *buf, int buflen,
				       struct msrpdu_talker_fail *talker_ad,
				       int *err_index)
{
	struct parse_param specs[] = {
		{"S" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{0, parse_null, 0}
	};
	if (buflen < 22)
		return -1;
	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

static int msrp_cmd_leave_stream(struct msrpdu_talker_fail *talker_ad)
{
	struct msrp_attribute *attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}
	attrib->type = MSRP_TALKER_ADV_TYPE;
	attrib->attribute.talk_listen = *talker_ad;
	msrp_event(MRP_EVENT_LV, attrib);

	return 0;
}

/* S+L   Report a listener status */
static int msrp_cmd_parse_report_listener_status(char *buf, int buflen,
						 struct msrpdu_talker_fail
						 *talker_ad,
						 uint32_t * substate,
						 int *err_index)
{
	struct parse_param specs[] = {
		{"L" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{"D" PARSE_ASSIGN, parse_u32, substate},
		{0, parse_null, 0}
	};
	if (buflen < 26)
		return -1;
	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

static int msrp_cmd_report_listener_status(struct msrpdu_talker_fail *talker_ad,
					   uint32_t substate)
{
	struct msrp_attribute *attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}
	attrib->type = MSRP_LISTENER_TYPE;
	attrib->direction = MSRP_DIRECTION_LISTENER;
	attrib->substate = substate;
	attrib->attribute.talk_listen = *talker_ad;
	msrp_event(MRP_EVENT_NEW, attrib);

	return 0;
}

/* S-L   Withdraw a listener status */
static int msrp_cmd_parse_withdraw_listener_status(char *buf, int buflen, struct msrpdu_talker_fail
						   *talker_ad, int *err_index)
{
	struct parse_param specs[] = {
		{"L" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{0, parse_null, 0}
	};
	if (buflen < 22)
		return -1;

	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

static int msrp_cmd_withdraw_listener_status(struct msrpdu_talker_fail
					     *talker_ad)
{
	struct msrp_attribute *attrib;
	struct msrp_attribute *existing_listener_attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}
	attrib->type = MSRP_LISTENER_TYPE;
	attrib->attribute.talk_listen = *talker_ad;
	/* need to maintain substate on LV, i.e. it can't just be 0 (IGNORE) */
	existing_listener_attrib = msrp_lookup(attrib);
	if (existing_listener_attrib) {
		attrib->substate = existing_listener_attrib->substate;
	}
	msrp_event(MRP_EVENT_LV, attrib);

	return 0;
}

/* S+D   Report a domain status */
/* S-D   Withdraw a domain status */
static int msrp_cmd_parse_domain_status(char *buf, int buflen,
					struct msrpdu_domain *domain,
					int *err_index)
{
	struct parse_param specs[] = {
		{"C" PARSE_ASSIGN, parse_u8, &domain->SRclassID},
		{"P" PARSE_ASSIGN, parse_u8, &domain->SRclassPriority},
		{"V" PARSE_ASSIGN, parse_u16_04x, &domain->SRclassVID},
		{0, parse_null, 0}
	};
	if (buflen < 18)
		return -1;
	memset(domain, 0, sizeof(*domain));
	return parse(buf + 4, buflen - 4, specs, err_index);
}

static int msrp_cmd_report_domain_status(struct msrpdu_domain *domain,
					 int report)
{
	struct msrp_attribute *attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}

	attrib->type = MSRP_DOMAIN_TYPE;
	attrib->attribute.domain = *domain;
	/* assume SRclassPriority for neighbor is same as ours */
	attrib->attribute.domain.neighborSRclassPriority =
		domain->SRclassPriority;
	if (report)
		msrp_event(MRP_EVENT_JOIN, attrib);
	else
		msrp_event(MRP_EVENT_LV, attrib);

	return 0;
}

int msrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client)
{
	int rc;
	char respbuf[12];
	int mrp_event;
	unsigned int substate;
	struct msrpdu_domain domain_param;
	struct msrpdu_talker_fail talker_param;
	int err_index;
	int attrib_type;

	if (NULL == MSRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC MSRP_db %s\n", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}

	rc = mrp_client_add(&(MSRP_db->mrp_db.clients), client);

	if (buflen < 3)
		return -1;

	if ('S' != buf[0])
		return -1;

	/*
	 * S?? - query MSRP Registrar database
	 * S+? - (re)JOIN a stream
	 * S++   NEW a stream
	 * S-- - LV a stream
	 * S+L   Report a listener status
	 * S-L   Withdraw a listener status
	 * S+D   Report a domain status
	 * S-D   Withdraw a domain status
	 */

	if (strncmp(buf, "S??", 3) == 0) {
		msrp_dumptable(client);

	} else if (strncmp(buf, "S-L", 3) == 0) {

		/* buf[] should look similar to 'S-L:L=xxyyzz...' */
		rc = msrp_cmd_parse_withdraw_listener_status(buf, buflen,
							     &talker_param,
							     &err_index);
		if (rc)
			goto out_ERP;
		rc = msrp_cmd_withdraw_listener_status(&talker_param);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else if (strncmp(buf, "S-D", 3) == 0) {

		/* buf[] should look similar to 'S-D:C=%d,P=%d,V:%04x" */
		rc = msrp_cmd_parse_domain_status(buf, buflen, &domain_param,
						  &err_index);
		if (rc)
			goto out_ERP;
		rc = msrp_cmd_report_domain_status(&domain_param, 0);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else if (strncmp(buf, "S--", 3) == 0) {
		/* buf[] should look similar to 'S--:S=xxyyzz...' */
		rc = msrp_cmd_parse_leave_stream(buf, buflen, &talker_param,
						 &err_index);
		if (rc)
			goto out_ERP;
		rc = msrp_cmd_leave_stream(&talker_param);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else if (strncmp(buf, "S+L", 3) == 0) {
		/* buf[] should look similar to 'S+L:L=xxyyzz...:D=a' */
		rc = msrp_cmd_parse_report_listener_status(buf, buflen,
							   &talker_param,
							   &substate,
							   &err_index);
		if (rc)
			goto out_ERP;
		rc = msrp_cmd_report_listener_status(&talker_param, substate);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else if (strncmp(buf, "S+D", 3) == 0) {
		/* buf[] should look similar to 'S+D:C=%d,P=%d,V=%04x" */
		rc = msrp_cmd_parse_domain_status(buf, buflen, &domain_param,
						  &err_index);
		if (rc)
			goto out_ERP;
		rc = msrp_cmd_report_domain_status(&domain_param, 1);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else if ((strncmp(buf, "S+?", 3) == 0)
		   || (strncmp(buf, "S++", 3) == 0)) {
		/*
		 * create or join a stream
		 * interesting to note the spec doesn't talk about
		 * what happens if two talkers attempt to define the identical
		 * stream twice  - does the bridge report STREAM_CHANGE error?
		 */

		/*
		   buf[] should look similar to "S=%02x%02x%02x%02x%02x%02x%02x%02x"
		   ",A=%02x%02x%02x%02x%02x%02x"
		   ",V=%04x" \
		   ",Z=%d" \
		   ",I=%d" \
		   ",P=%d" \
		   ",L=%d" \

		   if declaring a TalkerFailed, additional string fields
		   ",B=%02x%02x%02x%02x%02x%02x%02x%02x" \   (Failure BridgeID)
		   ",C=%d" \    (failure code)
		   are required.

		 */
		if (strstr(buf,"B=")) {
			attrib_type = MSRP_TALKER_FAILED_TYPE;
			rc = msrp_cmd_parse_join_or_new_stream_failure(buf, buflen,
						       &talker_param,
						       &err_index);
		} else {
			attrib_type = MSRP_TALKER_ADV_TYPE;
			rc = msrp_cmd_parse_join_or_new_stream(buf, buflen,
						       &talker_param,
						       &err_index);
		}

		if (rc)
			goto out_ERP;
		if ('?' == buf[2]) {
			mrp_event = MRP_EVENT_JOIN;
		} else {
			mrp_event = MRP_EVENT_NEW;
		}
		rc = msrp_cmd_join_or_new_stream(&talker_param, attrib_type, mrp_event);
		if (rc)
			goto out_ERI;	/* oops - internal error */
	} else {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC MSRP %s", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}
	return 0;

 out_ERI:
	snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s", buf);
	mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
	goto out;

 out_ERP:
	snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
	mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
	goto out;

 out:
	return -1;
}

int msrp_init(int msrp_enable)
{
	int rc;

	/* XXX doesn't handle re-start */

	msrp_socket = INVALID_SOCKET;
	MSRP_db = NULL;

	if (0 == msrp_enable) {
		return 0;
	}

	rc = mrpd_init_protocol_socket(MSRP_ETYPE, &msrp_socket, MSRP_ADDR);
	if (rc < 0)
		return -1;

	MSRP_db = (struct msrp_database*) malloc(sizeof(struct msrp_database));

	if (NULL == MSRP_db)
		goto abort_socket;

	memset(MSRP_db, 0, sizeof(struct msrp_database));

	/* if registration is FIXED or FORBIDDEN
	 * updates from MRP are discarded, and
	 * only IN and JOININ messages are sent
	 */
	MSRP_db->mrp_db.registration = MRP_REGISTRAR_CTL_NORMAL;	/* default */

	/* if participant role is 'SILENT' (or non-participant)
	 * applicant doesn't send any messages -
	 *
	 * Note - theoretically configured per-attribute
	 */
	MSRP_db->mrp_db.participant = MRP_APPLICANT_CTL_NORMAL;	/* default */

	rc = mrpd_init_timers(&(MSRP_db->mrp_db));

	if (rc < 0)
		goto abort_alloc;

	mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_BEGIN);

	return 0;

 abort_alloc:
	/* free MSRP_db and related structures */
	free(MSRP_db);
	MSRP_db = NULL;
 abort_socket:
	mrpd_close_socket(msrp_socket);
	msrp_socket = INVALID_SOCKET;
	/* XXX */
	return -1;
}

void msrp_reset(void)
{
	struct msrp_attribute *free_sattrib;
	struct msrp_attribute *sattrib;

	if (NULL == MSRP_db)
		return;

	sattrib = MSRP_db->attrib_list;
	while (NULL != sattrib) {
		free_sattrib = sattrib;
		sattrib = sattrib->next;
		free(free_sattrib);
    }
	free(MSRP_db);
}

void msrp_bye(struct sockaddr_in *client)
{
	if (NULL != MSRP_db)
		mrp_client_delete(&(MSRP_db->mrp_db.clients), client);
}

static struct msrp_attribute *msrp_conditional_reclaim(struct msrp_attribute *sattrib)
{
	struct msrp_attribute *free_sattrib;

	if ((sattrib->registrar.mrp_state == MRP_MT_STATE) &&
	    ((sattrib->applicant.mrp_state == MRP_VO_STATE) ||
	     (sattrib->applicant.mrp_state == MRP_AO_STATE) ||
	     (sattrib->applicant.mrp_state == MRP_QO_STATE))) {
		if (NULL != sattrib->prev)
			sattrib->prev->next = sattrib->next;
		else
			MSRP_db->attrib_list = sattrib->next;
		if (NULL != sattrib->next)
			sattrib->next->prev = sattrib->prev;
		free_sattrib = sattrib;
		sattrib = sattrib->next;
#if LOG_MSRP_GARBAGE_COLLECTION
		mrpd_log_printf("MSRP -------------> free attrib of type (%s), current 0x%p, next 0x%p\n",
				msrp_attrib_type_string(free_sattrib->type),
				free_sattrib, sattrib);
#endif
		msrp_send_notifications(free_sattrib, MRP_NOTIFY_LV);
		free(free_sattrib);
		return sattrib;
	} else {
		return sattrib->next;
	}

}

int msrp_reclaim(void)
{
	struct msrp_attribute *sattrib;

	if (NULL == MSRP_db)
		return 0;

	sattrib = MSRP_db->attrib_list;
	while (NULL != sattrib) {
		sattrib = msrp_conditional_reclaim(sattrib);
	}
	return 0;
}
