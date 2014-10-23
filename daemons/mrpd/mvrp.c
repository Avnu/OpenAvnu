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
 * MVRP protocol (part of 802.1Q-2011)
 */

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "mrpd.h"
#include "mrp.h"
#include "mvrp.h"
#include "parse.h"

int mvrp_send_notifications(struct mvrp_attribute *attrib, int notify);
static struct mvrp_attribute *mvrp_conditional_reclaim(struct mvrp_attribute *sattrib);
int mvrp_txpdu(void);

unsigned char MVRP_CUSTOMER_BRIDGE_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x21 };	/* 81-00 */
unsigned char MVRP_PROVIDER_BRIDGE_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0D };	/* 88-A8 */

extern unsigned char STATION_ADDR[];

/* global variables */
SOCKET mvrp_socket;
struct mvrp_database *MVRP_db;

/* MVRP */
#if LOG_MVRP && LOG_MRP
void mvrp_print_debug_info(int evt, const struct mvrp_attribute *attrib)
{
	char * state_mc_states = NULL;

	state_mc_states = mrp_print_status(&(attrib->applicant),
					   &(attrib->registrar));
	mrpd_log_printf("MVRP event %s, %s\n",
		        mrp_event_string(evt),
		        state_mc_states);		
}
#endif

struct mvrp_attribute *mvrp_lookup(struct mvrp_attribute *rattrib)
{
	struct mvrp_attribute *attrib;

	attrib = MVRP_db->attrib_list;
	while (NULL != attrib) {
		if (attrib->attribute == rattrib->attribute)
			return attrib;
		attrib = attrib->next;
	}
	return NULL;
}

int mvrp_add(struct mvrp_attribute *rattrib)
{
	struct mvrp_attribute *attrib;
	struct mvrp_attribute *attrib_tail;

	/* XXX do a lookup first to guarantee uniqueness? */

	attrib_tail = attrib = MVRP_db->attrib_list;

	while (NULL != attrib) {
		if (attrib->attribute < rattrib->attribute) {
			/* possible tail insertion ... */
			if (NULL != attrib->next) {
				attrib = attrib->next;
				continue;
			}
			rattrib->next = attrib->next;
			rattrib->prev = attrib;
			attrib->next = rattrib;
			return 0;

		} else {
			/* head insertion ... */
			rattrib->next = attrib;
			rattrib->prev = attrib->prev;
			attrib->prev = rattrib;
			if (NULL != rattrib->prev)
				rattrib->prev->next = rattrib;
			else
				MVRP_db->attrib_list = rattrib;

			return 0;
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
		MVRP_db->attrib_list = rattrib;
	} else {
		rattrib->next = NULL;
		rattrib->prev = attrib_tail;
		attrib_tail->next = rattrib;
	}

	return 0;
}

int mvrp_merge(struct mvrp_attribute *rattrib)
{
	struct mvrp_attribute *attrib;

	attrib = mvrp_lookup(rattrib);

	if (NULL == attrib)
		return -1;	/* shouldn't happen */

	/* primarily we update the last mac address state for diagnostics */
	memcpy(attrib->registrar.macaddr, rattrib->registrar.macaddr, 6);
	return 0;
}

int mvrp_event(int event, struct mvrp_attribute *rattrib)
{
	struct mvrp_attribute *attrib;
	int count = 0;
	int rc;

#if LOG_MVRP
	mrpd_log_printf("MVRP event %s\n", mrp_event_string(event));
#endif

	switch (event) {
	case MRP_EVENT_LVATIMER:
		mrp_lvatimer_stop(&(MVRP_db->mrp_db));
		mrp_jointimer_stop(&(MVRP_db->mrp_db));
		/* update state */
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MVRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_TXLA,
					  mrp_registrar_in(&(attrib->registrar)));
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_TXLA);
#if LOG_MVRP
			mvrp_print_debug_info(event, attrib);
#endif
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_LVATIMER);

		MVRP_db->send_empty_LeaveAll_flag = 1;
		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_TX);
		mvrp_txpdu();
		MVRP_db->send_empty_LeaveAll_flag = 0;
		break;
	case MRP_EVENT_RLA:
		mrp_jointimer_start(&(MVRP_db->mrp_db));
		/* update state */
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MVRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_RLA,
					  mrp_registrar_in(&(attrib->registrar)));
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_RLA);
#if LOG_MVRP
			mvrp_print_debug_info(event, attrib);
#endif
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_RLA);

		break;
	case MRP_EVENT_TX:
		mrp_jointimer_stop(&(MVRP_db->mrp_db));
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MVRP_db->mrp_db),
					  &(attrib->applicant), MRP_EVENT_TX,
					  mrp_registrar_in(&(attrib->registrar)));
#if LOG_MVRP
			mvrp_print_debug_info(event, attrib);
#endif
			count += mrp_applicant_state_transition_implies_tx(&(attrib->applicant));
			attrib = attrib->next;
		}

		mvrp_txpdu();

		/*
		 * Certain state transitions imply we need to request another tx
		 * opportunity.
		 */
		if (count) {
			mrp_jointimer_start(&(MVRP_db->mrp_db));
		}
		break;
	case MRP_EVENT_LVTIMER:
		mrp_lvtimer_stop(&(MVRP_db->mrp_db));
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db),
					  MRP_EVENT_LVTIMER);

#if LOG_MVRP
			mvrp_print_debug_info(event, attrib);
#endif
			attrib = mvrp_conditional_reclaim(attrib);
		}
		break;
	case MRP_EVENT_PERIODIC:
		attrib = MVRP_db->attrib_list;

		if (NULL != attrib) {
			mrp_jointimer_start(&(MVRP_db->mrp_db));
		}

		while (NULL != attrib) {
			mrp_applicant_fsm(&(MVRP_db->mrp_db),
					  &(attrib->applicant),
					  MRP_EVENT_PERIODIC,
					  mrp_registrar_in(&(attrib->registrar)));
#if LOG_MVRP
			mvrp_print_debug_info(event, attrib);
#endif
			attrib = attrib->next;
		}
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
		mrp_jointimer_start(&(MVRP_db->mrp_db));
		if (NULL == rattrib)
			return -1;	/* XXX internal fault */

		/* update state */
		attrib = mvrp_lookup(rattrib);

		if (NULL == attrib) {
			mvrp_add(rattrib);
			attrib = rattrib;
		} else {
			mvrp_merge(rattrib);
			free(rattrib);
		}

		mrp_applicant_fsm(&(MVRP_db->mrp_db), &(attrib->applicant),
				  event,
				  mrp_registrar_in(&(attrib->registrar)));
		/* remap local requests into registrar events */
		switch (event) {
		case MRP_EVENT_NEW:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_BEGIN);
			attrib->registrar.notify = MRP_NOTIFY_NEW;
			break;
		case MRP_EVENT_JOIN:
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MVRP_db->mrp_db),
						  MRP_EVENT_RJOININ);
			else
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MVRP_db->mrp_db),
						  MRP_EVENT_RJOINMT);
			break;
		case MRP_EVENT_LV:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_RLV);
			break;
		default:
			rc = mrp_registrar_fsm(&(attrib->registrar),
					       &(MVRP_db->mrp_db), event);
			if (-1 == rc) {
				printf
				    ("MVRP registrar error on attrib->attribute = %d\n",
				     attrib->attribute);
			}
			break;
		}
		attrib = mvrp_conditional_reclaim(attrib);
#if LOG_MVRP
		mvrp_print_debug_info(event, attrib);
#endif
		break;
	default:
		break;
	}

	/*
	 * XXX should honor the MVRP_db->mrp_db.registration and
	 * MVRP_db->mrp_db.participant controls
	 */

	/* generate local notifications */
	attrib = MVRP_db->attrib_list;

	while (NULL != attrib) {
		if (MRP_NOTIFY_NONE != attrib->registrar.notify) {
			mvrp_send_notifications(attrib,
						attrib->registrar.notify);
			attrib->registrar.notify = MRP_NOTIFY_NONE;
		}
		attrib = attrib->next;
	}

	return 0;
}

struct mvrp_attribute *mvrp_alloc()
{
	struct mvrp_attribute *attrib;

	attrib = (struct mvrp_attribute*) malloc(sizeof(struct mvrp_attribute));
	if (NULL == attrib)
		return NULL;

	memset(attrib, 0, sizeof(struct mvrp_attribute));

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

int mvrp_recv_msg(void)
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
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	uint16_t vid_firstval;
	struct mvrp_attribute *attrib;
	int endmarks;
	int saw_vlan_lva = 0;

	bytes = mrpd_recvmsgbuf(mvrp_socket, &msgbuf);
	if (bytes <= 0)
		goto out;

	if ((unsigned int)bytes < (sizeof(eth_hdr_t) + sizeof(mrpdu_t) +
				   sizeof(mrpdu_message_t)))
		goto out;

	eth = (eth_hdr_t *) msgbuf;

	/* note that MVRP frames should always arrive untagged (no vlan) */
	if (MVRP_ETYPE != ntohs(eth->typelen))
		goto out;

	/* check dest mac address too */
	if (memcmp(eth->destaddr, MVRP_CUSTOMER_BRIDGE_ADDR, sizeof(eth->destaddr)))
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
	if (MVRP_PROT_VER != mrpdu->ProtocolVersion)
		goto out;
	*/

	mrpdu_msg_ptr = (unsigned char *)MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu);

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= MRPD_OFFSETOF_MRPD_GET_MRPDU_MESSAGE_LIST;

	/*
	 * MVRP_VID_TYPE FirstValue is the 12 bit (2-byte) VLAN with
	 * corresponding attrib_length=2
	 *
	 * MVRP uses ThreePackedEvents for all vector encodings
	 *
	 * walk list until we run to the end of the PDU, or encounter a double end-mark
	 */

	endmarks = 0;

	while (mrpdu_msg_ptr < (mrpdu_msg_eof - MRPDU_ENDMARK_SZ)) {
		mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
		if ((mrpdu_msg->AttributeType == 0) &&
		    (mrpdu_msg->AttributeLength == 0)) {
			mrpdu_msg_ptr += 2;
			endmarks++;
			if (endmarks < 2)
				continue;	/* end-mark of message-list */
			else
				break;	/* two endmarks - end of message list */
		}

		endmarks = 0;

		switch (mrpdu_msg->AttributeType) {
		case MVRP_VID_TYPE:
			if (mrpdu_msg->AttributeLength != 2) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}
			/* AttributeListLength not used for MVRP, hence
			 * Data points to the beginning of the VectorAttributes
			 */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) mrpdu_msg->Data;
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (MRPDU_VECT_LVA(ntohs(mrpdu_vectorptr->VectorHeader)) && !saw_vlan_lva) {
					saw_vlan_lva = 1;
					mvrp_event(MRP_EVENT_RLA, NULL);
				}

				if (0 == numvalues)
					/* Malformed - cant tell how long the trailing vectors are */
					goto out;

				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof)
					/* Malformed - runs off the end of the pdu */
					goto out;

				vid_firstval = (((uint16_t)
						 mrpdu_vectorptr->
						 FirstValue_VectorEvents[0]) <<
						8)
				    | mrpdu_vectorptr->
				    FirstValue_VectorEvents[1];

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 2;
				     vectidx <= (numvectorbytes + 2);
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

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						if (0xFFF < vid_firstval)	/*discard junk */
							continue;

						attrib = mvrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->attribute =
						    vid_firstval;
						vid_firstval++;
						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							mvrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							mvrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							mvrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							mvrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							mvrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							mvrp_event
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

				/* 1 byte Type, 1 byte Len, 2 byte FirstValue, and (n) vector bytes */
				mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 4 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		default:
			/*
			 * If protocol version is 0, but attribute type is unknown, we have
			 * a malformed PDU, so terminate parsing.
			 */
			if (MVRP_PROT_VER == mrpdu->ProtocolVersion)
				goto out;

			/*
			 * Try to parse unknown message type.
			 */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) mrpdu_msg->Data;
			numvalues = MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
			/* skip this null attribute ... some switches generate these ... */
			/* 2 byte numvalues + FirstValue + vector bytes */
			mrpdu_msg_ptr += 2 + mrpdu_msg->AttributeLength + (numvalues + 2) / 3;
			mrpdu_vectorptr = (mrpdu_vectorattrib_t *)mrpdu_msg_ptr;
			break;
		}
	}

	free(msgbuf);
	return 0;
 out:
	free(msgbuf);

	return -1;
}

int
mvrp_emit_vidvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
		     int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	uint16_t numvalues;
	uint8_t vect_3pack;
	int vectidx;
	unsigned int vectevt[3];
	int vectevt_idx;
	uint16_t vid_firstval;
	struct mvrp_attribute *attrib, *vattrib;
	mrpdu_message_t *mrpdu_msg;
	unsigned int attrib_found_flag = 0;
	unsigned int vector_size = 6;

	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;

	/* need at least 6 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - vector_size))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MVRP_VID_TYPE;
	mrpdu_msg->AttributeLength = 2;

	attrib = MVRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg->Data;

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - vector_size - MRPDU_ENDMARK_SZ)) && (NULL != attrib)) {

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
		vid_firstval = attrib->attribute;
		mrpdu_vectorptr->FirstValue_VectorEvents[0] =
		    (uint8_t) (attrib->attribute >> 8);
		mrpdu_vectorptr->FirstValue_VectorEvents[1] =
		    (uint8_t) (attrib->attribute);

		switch (attrib->applicant.sndmsg) {
		case MRP_SND_IN:
			/*
			 * If 'In' is indicated by the applicant attribute, then
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
			/* If 'Join' in indicated by the applicant, look at
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
#if LOG_MVRP
		mrpd_log_printf("MVRP -> mvrp_emit_vidvectors() send %s, pdu %s\n",
			mrp_send_string(attrib->applicant.sndmsg),
			mrp_pdu_string(vectevt[0]));
#endif

		vectevt_idx = 1;
		numvalues = 1;
		vectevt[1] = 0;
		vectevt[2] = 0;

		/* now attempt to vectorize contiguous other attributes
		 * which also need to be transmitted
		 */

		vectidx = 2;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (0 == vattrib->applicant.tx)
				break;

			vid_firstval++;

			if (vattrib->attribute != vid_firstval)
				break;

			vattrib->applicant.tx = 0;

			switch (vattrib->applicant.sndmsg) {
			case MRP_SND_IN:
				/*
				 * If 'In' is indicated by the applicant attribute, then
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

		/* 2 byte header, followed by FirstValues/Vectors - remember vectidx starts at 0 */
		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}

	/*
	 * If no attributes are declared, send a LeaveAll with an all 0
	 * FirstValue, Number of Values set to 0 and not attribute event.
	 */
	if ((0 == attrib_found_flag) && MVRP_db->send_empty_LeaveAll_flag) {

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(0) |
						MRPDU_VECT_LVA_FLAG;
		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[mrpdu_msg->AttributeLength]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}


	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *) mrpdu_msg->Data) {
		*bytes_used = 0;
		return 0;
	}

	/* endmark */
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;
	*mrpdu_msg_ptr = 0;
	mrpdu_msg_ptr++;

	*bytes_used = (mrpdu_msg_ptr - msgbuf);
	return 0;
 oops:
	/* an internal error - caller should assume TXLAF */
	*bytes_used = 0;
	return -1;
}

int mvrp_txpdu(void)
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

	/* note that MVRP frames should always be untagged (no vlan) */
	eth->typelen = htons(MVRP_ETYPE);
	memcpy(eth->destaddr, MVRP_CUSTOMER_BRIDGE_ADDR, sizeof(eth->destaddr));
	memcpy(eth->srcaddr, STATION_ADDR, sizeof(eth->srcaddr));

	msgbuf_wrptr += sizeof(eth_hdr_t);

	mrpdu = (mrpdu_t *) msgbuf_wrptr;

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

	mrpdu->ProtocolVersion = MVRP_PROT_VER;
	mrpdu_msg_ptr = MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu);
	mrpdu_msg_eof = (unsigned char *)msgbuf + MAX_FRAME_SIZE;

	/*
	 * Iterate over all attributes, transmitting those marked
	 * with 'tx', attempting to coalesce multiple contiguous tx attributes
	 * with appropriate vector encodings.
	 *
	 * MVRP_VID_TYPE FirstValue is the 2-byte VLAN with
	 * corresponding attrib_length=2
	 *
	 * MVRP uses ThreePackedEvents for all vector encodings
	 *
	 * the expanded version of the MRPDU looks like
	 * ProtocolVersion
	 * AttributeType
	 * AttributeLength
	 * <Variable number of>
	 *              LeaveAllEvent | NumberOfValues
	 *              <Variable FirstValue>
	 *              <VectorEventBytes>
	 * EndMark
	 *
	 * in effect, each AttributeType present gets its own 'message' with
	 * variable number of vectors. So with MVRP, you will have at most
	 * one message.
	 */

	if (MVRP_db->mrp_db.lva.tx) {
		lva = 1;
		MVRP_db->mrp_db.lva.tx = 0;
	}

	rc = mvrp_emit_vidvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
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

	bytes = mrpd_send(mvrp_socket, msgbuf, msgbuf_len, 0);
#if LOG_MVRP
	mrpd_log_printf("MVRP send PDU\n");
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

int mvrp_send_notifications(struct mvrp_attribute *attrib, int notify)
{
	char *msgbuf;
	char *variant;
	char *regsrc;
	char mrp_state[8];
	client_t *client;

	if (NULL == attrib)
		return -1;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return -1;

	variant = regsrc = NULL;

	variant = (char *)malloc(128);
	regsrc = (char *)malloc(128);

	if ((NULL == variant) || (NULL == regsrc))
		goto free_msgbuf;

	memset(msgbuf, 0, MAX_MRPD_CMDSZ);

	sprintf(variant, "%04x", attrib->attribute);

	sprintf(regsrc, "R=%02x%02x%02x%02x%02x%02x",
		attrib->registrar.macaddr[0],
		attrib->registrar.macaddr[1],
		attrib->registrar.macaddr[2],
		attrib->registrar.macaddr[3],
		attrib->registrar.macaddr[4], attrib->registrar.macaddr[5]);

	mrp_decode_state(&attrib->registrar, &attrib->applicant,
				 mrp_state, sizeof(mrp_state));


	switch (notify) {
	case MRP_NOTIFY_NEW:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VNE %s %s %s\n", variant, regsrc, mrp_state);
		break;
	case MRP_NOTIFY_JOIN:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VJO %s %s %s\n", variant, regsrc, mrp_state);
		break;
	case MRP_NOTIFY_LV:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VLE %s %s %s\n", variant, regsrc, mrp_state);
		break;
	default:
		goto free_msgbuf;
		break;
	}

	client = MVRP_db->mrp_db.clients;
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

int mvrp_dumptable(struct sockaddr_in *client)
{
	char *msgbuf;
	char *msgbuf_wrptr;
	char *stage;
	char *variant;
	char *regsrc;
	struct mvrp_attribute *attrib;
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

	attrib = MVRP_db->attrib_list;
	if (attrib == NULL) {
		sprintf(msgbuf, "MVRP:Empty\n");
	}

	while (NULL != attrib) {
		sprintf(variant, "V:I=%04x", attrib->attribute);

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
		msgbuf_wrptr += strnlen(stage, 128);
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

/* V+? - JOIN a VID
 * V++   NEW a VID (XXX: note network disturbance) */
int mvrp_cmd_parse_vid(char *buf, int buflen,
		       uint16_t * attribute, int *err_index)
{
	struct parse_param specs[] = {
		{"I" PARSE_ASSIGN, parse_u16_04x, attribute},
		{0, parse_null, 0}
	};
	*attribute = 0;
	if (buflen < 9)
		return -1;
	return parse(buf + 4, buflen - 4, specs, err_index);
}

int mvrp_cmd_vid(uint16_t attribute, int mrp_event)
{
	struct mvrp_attribute *attrib;

	attrib = mvrp_alloc();
	if (NULL == attrib)
		return -1;
	attrib->attribute = attribute;
	mvrp_event(mrp_event, attrib);
	return 0;
}

int mvrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client)
{
	int rc;
	int mrp_event;
	char respbuf[12];
	uint16_t vid_param;
	int err_index;

	if (NULL == MVRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC MVRP_db %s\n", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}

	rc = mrp_client_add(&(MVRP_db->mrp_db.clients), client);

	if (buflen < 3)
		return -1;

	if ('V' != buf[0])
		return -1;

	/*
	 * V?? - query MVRP Registrar VID database
	 * V+? - JOIN a VID
	 * V++   NEW a VID (XXX: note network disturbance)
	 * V-- - LV a VID
	 */
	if (strncmp(buf, "V??", 3) == 0) {
		mvrp_dumptable(client);
	} else if (strncmp(buf, "V--", 3) == 0) {
		rc = mvrp_cmd_parse_vid(buf, buflen, &vid_param, &err_index);
		if (rc)
			goto out_ERP;
		rc = mvrp_cmd_vid(vid_param, MRP_EVENT_LV);
		if (rc)
			goto out_ERI;
	} else if ((strncmp(buf, "V++", 3) == 0)
		   || (strncmp(buf, "V+?", 3) == 0)) {
		rc = mvrp_cmd_parse_vid(buf, buflen, &vid_param, &err_index);
		if (rc)
			goto out_ERP;
		if ('?' == buf[2]) {
			mrp_event = MRP_EVENT_JOIN;
		} else {
			mrp_event = MRP_EVENT_NEW;
		}
		rc = mvrp_cmd_vid(vid_param, mrp_event);
		if (rc)
			goto out_ERI;
	} else {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC MVRP %s", buf);
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

int mvrp_init(int mvrp_enable)
{
	int rc;

	/* XXX doesn't handle re-start */

	mvrp_socket = INVALID_SOCKET;
	MVRP_db = NULL;

	if (0 == mvrp_enable) {
		return 0;
	}

	rc = mrpd_init_protocol_socket(MVRP_ETYPE, &mvrp_socket,
				       MVRP_CUSTOMER_BRIDGE_ADDR);
	if (rc < 0)
		return -1;

	MVRP_db = (struct mvrp_database*) malloc(sizeof(struct mvrp_database));

	if (NULL == MVRP_db)
		goto abort_socket;

	memset(MVRP_db, 0, sizeof(struct mvrp_database));

	/* if registration is FIXED or FORBIDDEN
	 * updates from MRP are discarded, and
	 * only IN and JOININ messages are sent
	 */
	MVRP_db->mrp_db.registration = MRP_REGISTRAR_CTL_NORMAL;	/* default */

	/* if participant role is 'SILENT' (or non-participant)
	 * applicant doesn't send any messages -
	 *
	 * Note - theoretically configured per-attribute
	 */
	MVRP_db->mrp_db.participant = MRP_APPLICANT_CTL_NORMAL;	/* default */

	rc = mrpd_init_timers(&(MVRP_db->mrp_db));

	if (rc < 0)
		goto abort_alloc;

	mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_BEGIN);
	return 0;

 abort_alloc:
	/* free MVRP_db and related structures */
	free(MVRP_db);
	MVRP_db = NULL;
 abort_socket:
	mrpd_close_socket(mvrp_socket);
	mvrp_socket = INVALID_SOCKET;
	/* XXX */
	return -1;
}

static struct mvrp_attribute *mvrp_conditional_reclaim(struct mvrp_attribute *vattrib)
{
	struct mvrp_attribute *free_vattrib;

	if ((vattrib->registrar.mrp_state == MRP_MT_STATE) &&
	    ((vattrib->applicant.mrp_state == MRP_VO_STATE) ||
	     (vattrib->applicant.mrp_state == MRP_AO_STATE) ||
	     (vattrib->applicant.mrp_state == MRP_QO_STATE))) {
		if (NULL != vattrib->prev)
			vattrib->prev->next = vattrib->next;
		else
			MVRP_db->attrib_list = vattrib->next;
		if (NULL != vattrib->next)
			vattrib->next->prev = vattrib->prev;
		free_vattrib = vattrib;
		vattrib = vattrib->next;
#if LOG_MVRP_GARBAGE_COLLECTION
		mrpd_log_printf("MVRP -------------> free attrib of type (%d), current 0x%p, next 0x%p\n",
				free_vattrib->attribute,
				free_vattrib, vattrib);
#endif
		mvrp_send_notifications(free_vattrib, MRP_NOTIFY_LV);
		free(free_vattrib);
		return vattrib;
	} else {
		return vattrib->next;
	}

}

int mvrp_reclaim(void)
{
	struct mvrp_attribute *vattrib;
	if (NULL == MVRP_db)
		return 0;

	vattrib = MVRP_db->attrib_list;
	while (NULL != vattrib) {
		vattrib = mvrp_conditional_reclaim(vattrib);

	}
	return 0;
}

void mvrp_reset(void)
{
	struct mvrp_attribute *free_sattrib;
	struct mvrp_attribute *sattrib;

	if (NULL == MVRP_db)
		return;

	sattrib = MVRP_db->attrib_list;
	while (NULL != sattrib) {
		free_sattrib = sattrib;
		sattrib = sattrib->next;
		free(free_sattrib);
	}
	free(MVRP_db);
}

void mvrp_bye(struct sockaddr_in *client)
{
	if (NULL != MVRP_db)
		mrp_client_delete(&(MVRP_db->mrp_db.clients), client);

}
