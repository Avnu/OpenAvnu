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

#include "parse.h"
#include "mrpd.h"
#include "mrp.h"
#include "msrp.h"
#include "mmrp.h"

int msrp_txpdu(void);
int msrp_send_notifications(struct msrp_attribute *attrib, int notify);

unsigned char MSRP_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E };

extern unsigned char STATION_ADDR[];

/* global variables */
SOCKET msrp_socket;
struct msrp_database *MSRP_db;

struct msrp_attribute *msrp_lookup(struct msrp_attribute *rattrib)
{
	struct msrp_attribute *attrib;
	int mac_eq;

	attrib = MSRP_db->attrib_list;
	while (NULL != attrib) {
		if (rattrib->type == attrib->type) {
			if (MSRP_DOMAIN_TYPE == attrib->type) {
				mac_eq = memcmp(&(attrib->attribute.domain),
						&(rattrib->attribute.domain),
						sizeof(msrpdu_domain_t));
				if (0 == mac_eq)
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
	struct msrp_attribute *attrib;

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
		attrib->attribute.talk_listen.FailureInformation.FailureCode =
		    rattrib->attribute.talk_listen.FailureInformation.
		    FailureCode;
		memcpy(attrib->attribute.talk_listen.FailureInformation.
		       BridgeID,
		       rattrib->attribute.talk_listen.FailureInformation.
		       BridgeID, 8);
		attrib->attribute.talk_listen.AccumulatedLatency =
		    rattrib->attribute.talk_listen.AccumulatedLatency;
		break;
	case MSRP_LISTENER_TYPE:
		if (attrib->substate != rattrib->substate) {
			attrib->substate = rattrib->substate;
			attrib->registrar.mrp_state = MRP_MT_STATE;	/* ugly - force a notify */
		}
		break;
	case MSRP_DOMAIN_TYPE:
		/* doesn't/shouldn't happen */
		break;
	default:
		break;
	}
	return 0;
}

int msrp_event(int event, struct msrp_attribute *rattrib)
{
	struct msrp_attribute *attrib;
	int rc;

	switch (event) {
	case MRP_EVENT_LVATIMER:
		mrp_jointimer_stop(&(MSRP_db->mrp_db));
		/* update state */
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TXLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_TXLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_LVATIMER);

		msrp_txpdu();
		break;
	case MRP_EVENT_RLA:
		mrp_jointimer_stop(&(MSRP_db->mrp_db));
		/* update state */
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_RLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_RLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_RLA);

		break;
	case MRP_EVENT_TX:
		mrp_jointimer_stop(&(MSRP_db->mrp_db));
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TX);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_TX);

		msrp_txpdu();
		break;
	case MRP_EVENT_LVTIMER:
		mrp_lvtimer_stop(&(MSRP_db->mrp_db));
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db),
					  MRP_EVENT_LVTIMER);

			attrib = attrib->next;
		}
		break;
	case MRP_EVENT_PERIODIC:
		attrib = MSRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant),
					  MRP_EVENT_PERIODIC);
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

		mrp_applicant_fsm(&(attrib->applicant), event);

		/* remap local requests into registrar events */
		switch (event) {
		case MRP_EVENT_NEW:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MSRP_db->mrp_db), MRP_EVENT_RNEW);
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
				    ("MSRP registrar error on attrib->type = %d\n",
				     attrib->type);
			}
			break;
		}
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

struct msrp_attribute *msrp_alloc()
{
	struct msrp_attribute *attrib;

	attrib = malloc(sizeof(struct msrp_attribute));
	if (NULL == attrib)
		return NULL;

	memset(attrib, 0, sizeof(struct msrp_attribute));

	attrib->applicant.mrp_state = MRP_VO_STATE;
	attrib->applicant.tx = 0;
	attrib->applicant.sndmsg = MRP_SND_NULL;
	attrib->applicant.encode = MRP_ENCODE_OPTIONAL;

	attrib->registrar.mrp_state = MRP_MT_STATE;
	attrib->registrar.notify = MRP_NOTIFY_NONE;

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
	/* XXX check dest mac address too? */

	mrpdu = (mrpdu_t *) (msgbuf + sizeof(struct eth_hdr));

	/*
	 * ProtocolVersion handling - a receiver must process received frames with a lesser
	 * protcol version consistent with the older protocol processing requirements (e.g. a V2
	 * agent receives a V1 message, the V1 message shoudl be parsed with V1 rules).
	 *
	 * However - if an agent receives a NEWER protocol, the agent shoudl still attempt
	 * to parse the frame. If the agent finds an AttributeType not recognized
	 * the agent discards the current message including any associated trailing vectors
	 * up to the end-mark, and resumes with the next message or until the end of the PDU
	 * is reached.
	 *
	 * If a VectorAttribute is found with an unknown Event for the Type, the specific
	 * VectorAttrute is discarded and processing continues with the next VectorAttribute.
	 */

	if (MSRP_PROT_VER != mrpdu->ProtocolVersion) {	/* XXX should accept ... */
		goto out;
	}
	mrpdu_msg_ptr = MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu);

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= MRPD_OFFSETOF_MRPD_GET_MRPDU_MESSAGE_LIST;

	endmarks = 0;

	while (mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) {
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
			    (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

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

				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					msrp_event(MRP_EVENT_RLA, NULL);

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
			    (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

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
				     [listener_endbyte])
>= mrpdu_msg_eof)
					goto out;

				if (NULL == listener_vectevt) {
					listener_vectevt_sz = 64;;
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
							    realloc
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

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						if (MSRP_LISTENER_IGNORE ==
						    vectevt[vectevt_idx]) {
							msrp_increment_streamid
							    (streamid_firstval);
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

				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					msrp_event(MRP_EVENT_RLA, NULL);

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
			    (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;

			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

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

				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					msrp_event(MRP_EVENT_RLA, NULL);

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
			    (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);
			mrpdu_msg_ptr = (uint8_t *) mrpdu_vectorptr;
			while (!((mrpdu_msg_ptr[0] == 0)
				 && (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

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

					for (vectevt_idx = 0;
					     vectevt_idx < numvalues_processed;
					     vectevt_idx++) {

						attrib = msrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */
						/* update the failure information on merge, but don't support
						 * different ADV and FAILED_TYPES in the list of attributes */
						attrib->type =
						    MSRP_TALKER_ADV_TYPE;

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

				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					msrp_event(MRP_EVENT_RLA, NULL);

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
			printf("unrecognized attribute type (%d)\n",
			       mrpdu_msg->AttributeType);
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
msrp_emit_talkvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
		      int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	uint16_t numvalues;
	uint8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	uint8_t streamid_firstval[8];
	uint8_t destmac_firstval[6];
	int attriblistlen;
	struct msrp_attribute *attrib, *vattrib;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;
	int mac_eq;

	/* need at least 28 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - 28))
		goto oops;

	/*
	 * as an endpoint, we don't advertise talker failed attributes (which
	 * only a listener should receive) - but since this daemon could
	 * function dual-mode - listening to streams as well as being a talker
	 * station - we only advertise the talker vectors for declared streams,
	 * not those we know about as a listener ...
	 */
	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MSRP_TALKER_ADV_TYPE;
	mrpdu_msg->AttributeLength = 25;

	attrib = MSRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) && (NULL != attrib)) {

		if (MSRP_TALKER_ADV_TYPE != attrib->type) {
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
		if (MRP_ENCODE_OPTIONAL == attrib->applicant.encode) {
			attrib->applicant.tx = 0;
			attrib = attrib->next;
			continue;
		}

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

		mrpdu_vectorptr->FirstValue_VectorEvents[16] =
		    attrib->attribute.talk_listen.TSpec.MaxFrameSize >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[17] =
		    (uint8_t) attrib->attribute.talk_listen.TSpec.MaxFrameSize;

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

		vectidx = 25;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (MSRP_TALKER_ADV_TYPE != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			msrp_increment_streamid(streamid_firstval);
			mmrp_increment_macaddr(destmac_firstval);

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
			    > (mrpdu_msg_eof - 2))
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
		    (mrpdu_msg_eof - 2))
			goto oops;

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(numvalues);

		if (lva)
			mrpdu_vectorptr->VectorHeader |= MRPDU_VECT_LVA(0xFFFF);

		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);
		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;

		attrib = attrib->next;

	}

	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2])) {
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
	int mac_eq;

	/* need at least 13 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - 13))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MSRP_LISTENER_TYPE;
	mrpdu_msg->AttributeLength = 8;

	listen_declare = (int *)malloc(listen_declare_sz * sizeof(int));
	if (NULL == listen_declare)
		goto oops;

	attrib = MSRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2]);

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) && (NULL != attrib)) {

		if (MSRP_LISTENER_TYPE != attrib->type) {
			attrib = attrib->next;
			continue;
		}

		listen_declare_idx = 0;

		/* if we have a listener type registered, we always will
		 * send out an update. We also don't bother trying to
		 * insert IGNORES between stream IDs - we only send out
		 * contiguous active streams in messages
		 */

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
			    > (mrpdu_msg_eof - 2))
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
			    > (mrpdu_msg_eof - 2))
				goto oops;

			if (vectevt_idx > 3) {
				vect_4pack =
				    MRPDU_4PACK_ENCODE(listen_declare
						       [listen_declare_idx],
						       listen_declare
						       [listen_declare_idx + 1],
						       listen_declare
						       [listen_declare_idx + 2],
						       listen_declare
						       [listen_declare_idx +
							3]);

				mrpdu_vectorptr->FirstValue_VectorEvents
				    [vectidx] = vect_4pack;
				vectidx++;
			}

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - 3))
				goto oops;

		}

		/* handle any trailers */
		vectevt_idx = 0;
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
		    (mrpdu_msg_eof - 2))
			goto oops;

		mrpdu_vectorptr->VectorHeader = MRPDU_VECT_NUMVALUES(numvalues);

		if (lva)
			mrpdu_vectorptr->VectorHeader |= MRPDU_VECT_LVA(0xFFFF);

		mrpdu_vectorptr->VectorHeader =
		    htons(mrpdu_vectorptr->VectorHeader);

		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

		mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
	}

	if (mrpdu_vectorptr == (mrpdu_vectorattrib_t *)&(mrpdu_msg->Data[2])) {
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

	rc = msrp_emit_talkvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	if (mrpdu_msg_ptr >= (mrpdu_msg_eof - 2))
		goto out;

	rc = msrp_emit_listenvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	if (mrpdu_msg_ptr == MRPD_GET_MRPDU_MESSAGE_LIST(mrpdu)) {
		goto out;	/* nothing to send */
	}

	/* endmark */
	if (mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) {
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
	} else
		goto out;

	msgbuf_len = mrpdu_msg_ptr - msgbuf;

	bytes = mrpd_send(msrp_socket, msgbuf, msgbuf_len, 0);
	if (bytes <= 0)
		goto out;

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
	char *stage;
	char *variant;
	char *regsrc;
	client_t *client;

	if (NULL == attrib)
		return -1;

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

	if (MSRP_LISTENER_TYPE == attrib->type) {
		sprintf(variant, "L:D:%d:S:%02x%02x%02x%02x%02x%02x%02x%02x",
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
		sprintf(variant, "D:C:%d:P:%d:V:%04x",
			attrib->attribute.domain.SRclassID,
			attrib->attribute.domain.SRclassPriority,
			attrib->attribute.domain.SRclassVID);
	} else {
		sprintf(variant, "T:S:%02x%02x%02x%02x%02x%02x%02x%02x"
			":A:%02x%02x%02x%02x%02x%02x"
			":V:%04x"
			":Z:%d"
			":I:%d"
			":P:%d"
			":L:%d"
			":B:%02x%02x%02x%02x%02x%02x%02x%02x"
			":C:%d",
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

	sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x",
		attrib->registrar.macaddr[0],
		attrib->registrar.macaddr[1],
		attrib->registrar.macaddr[2],
		attrib->registrar.macaddr[3],
		attrib->registrar.macaddr[4], attrib->registrar.macaddr[5]);

	switch (notify) {
	case MRP_NOTIFY_NEW:
		sprintf(msgbuf, "SNE %s %s", variant, regsrc);
		break;
	case MRP_NOTIFY_JOIN:
		sprintf(msgbuf, "SJO %s %s", variant, regsrc);
		break;
	case MRP_NOTIFY_LV:
		sprintf(msgbuf, "SLE %s %s", variant, regsrc);
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
	if (stage)
		free(stage);
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

	while (NULL != attrib) {
		if (MSRP_LISTENER_TYPE == attrib->type) {
			sprintf(variant,
				"L:D:%d:S:%02x%02x%02x%02x%02x%02x%02x%02x",
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
			sprintf(variant, "D:C:%d:P:%d:V:%04x",
				attrib->attribute.domain.SRclassID,
				attrib->attribute.domain.SRclassPriority,
				attrib->attribute.domain.SRclassVID);
		} else {
			sprintf(variant, "T:S:%02x%02x%02x%02x%02x%02x%02x%02x"
				":A:%02x%02x%02x%02x%02x%02x"
				":V:%04x"
				":Z:%d"
				":I:%d"
				":P:%d"
				":L:%d"
				":B:%02x%02x%02x%02x%02x%02x%02x%02x"
				":C:%d",
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

		sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x %s",
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


int msrp_test_cmd_parse_join_or_new_stream(
		char *buf, int buflen,
		struct msrpdu_talker_fail *talker_ad
)
{
	int err, err_index;

	struct parse_param specs[] = {
		{"S" PARSE_ASSIGN, parse_c64, talker_ad->StreamID},
		{"A" PARSE_ASSIGN, parse_mac, talker_ad->DataFrameParameters.Dest_Addr},
		{"V" PARSE_ASSIGN, parse_u16, &talker_ad->DataFrameParameters.Vlan_ID},
		{"Z" PARSE_ASSIGN, parse_u16, &talker_ad->TSpec.MaxFrameSize},
		{"I" PARSE_ASSIGN, parse_u16, &talker_ad->TSpec.MaxIntervalFrames},
		{"P" PARSE_ASSIGN, parse_u8, &talker_ad->PriorityAndRank},
		{"L" PARSE_ASSIGN, parse_u32, &talker_ad->AccumulatedLatency},
		{0, parse_null, 0}
	};
	memset(talker_ad, 0, sizeof(*talker_ad));
	return parse(buf, specs, &err_index);
}

/* S+? - (re)JOIN a stream */
/* S++ - NEW a stream      */

int msrp_cmd_parse_join_or_new_stream(
		char *buf, int buflen,
		struct msrpdu_talker_fail *talker_ad
)
{
	char *stream_str = NULL;
	char *daddr_str = NULL;
	char *vlan_str = NULL;
	char *size_str = NULL;
	char *interval_str = NULL;
	char *prio_str = NULL;
	char *latency_str = NULL;
	uint8_t streamid_firstval[8];
	uint8_t macvec_firstval[6];
	uint8_t macvec_parsestr[64];
	unsigned vlan, size, interval, prio, latency;
	int i, rc;

	memset(talker_ad, 0, sizeof(*talker_ad));
		/*
		 * create or join a stream
		 * interesting to note the spec doesn't talk about
		 * what happens if two talkers attempt to define the identical
		 * stream twice  - does the bridge report STREAM_CHANGE error?
		 */

		if (buflen < 22) {
			return -1;
		}
		/*
		   buf[] should look similar to "S:%02x%02x%02x%02x%02x%02x%02x%02x"
		   ":A:%02x%02x%02x%02x%02x%02x"
		   ":V:%04x" \
		   ":Z:%d" \
		   ":I:%d" \
		   ":P:%d" \
		   ":L:%d" \
		 */

		i = 0;

		while (i < buflen) {
			if (buf[i] == ':') {
				stream_str = &(buf[i + 1]);
				i++;
				break;
			}
			i++;
		}

		if (i >= buflen) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'A')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				daddr_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'V')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				vlan_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'Z')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				size_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'I')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				interval_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'P')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				prio_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		while (i < (buflen - 3)) {
			if ((buf[i] == ':') && (buf[i + 1] == 'L')
			    && (buf[i + 2] == ':')) {
				buf[i] = '\0';
				latency_str = &(buf[i + 3]);
				i += 4;
				break;
			}
			i++;
		}

		if (i >= (buflen - 3)) {
			return -1;
		}

		memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

		for (i = 0; i < 8; i++) {
			macvec_parsestr[0] = stream_str[i * 2];
			macvec_parsestr[1] = stream_str[i * 2 + 1];

			rc = sscanf((char *)macvec_parsestr, "%hhx",
				    &streamid_firstval[i]);
			if (0 == rc) {
				return -1;
			}
		}

		memset(macvec_parsestr, 0, sizeof(macvec_parsestr));
		for (i = 0; i < 6; i++) {
			macvec_parsestr[0] = daddr_str[i * 2];
			macvec_parsestr[1] = daddr_str[i * 2 + 1];

			rc = sscanf((char *)macvec_parsestr, "%hhx",
				    &macvec_firstval[i]);
			if (0 == rc) {
				return -1;
			}
		}

		rc = sscanf((char *)vlan_str, "%x", &vlan);
		if (0 == rc) {
			return -1;
		}

		rc = sscanf((char *)size_str, "%d", &size);
		if (0 == rc) {
			return -1;
		}

		rc = sscanf((char *)interval_str, "%d", &interval);
		if (0 == rc) {
			return -1;
		}

		rc = sscanf((char *)prio_str, "%d", &prio);
		if (0 == rc) {
			return -1;
		}

		rc = sscanf((char *)latency_str, "%d", &latency);
		if (0 == rc) {
			return -1;
		}

		memcpy(talker_ad->StreamID,
		       streamid_firstval, 8);
		memcpy(talker_ad->DataFrameParameters.Dest_Addr, macvec_firstval,
		       6);
		talker_ad->DataFrameParameters.Vlan_ID = vlan;
		talker_ad->TSpec.MaxFrameSize = size;
		talker_ad->TSpec.MaxIntervalFrames = interval;
		talker_ad->PriorityAndRank = prio;
		talker_ad->AccumulatedLatency = latency;


	return 0;
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
 */

int msrp_cmd_join_or_new_stream(
		int join,
		struct msrpdu_talker_fail *talker_ad
)
{
	struct msrp_attribute *attrib;

	attrib = msrp_alloc();
	if (NULL == attrib) {
		return -1;
	}
	attrib->type = MSRP_TALKER_ADV_TYPE;
	attrib->attribute.talk_listen = *talker_ad;
	memset(attrib->registrar.macaddr, 0, 6);

	if (join)
		msrp_event(MRP_EVENT_JOIN, attrib);
	else
		msrp_event(MRP_EVENT_NEW, attrib);

	return 0;
}

/*
Future


int msrp_cmd_parse_leave_stream();
int msrp_cmd_parse_report_listener_status();
int msrp_cmd_parse_withdraw_listener_status();
int msrp_cmd_parse_report_domain_status();
int msrp_cmd_parse_withdraw_domain_status();

int msrp_cmd_leave_stream();
int msrp_cmd_report_listener_status();
int msrp_cmd_withdraw_listener_status();
int msrp_cmd_report_domain_status();
int msrp_cmd_withdraw_domain_status();
*/

int msrp_recv_cmd(char *buf, int buflen, struct sockaddr_in *client)
{
	int rc;
	char respbuf[8];
	struct msrp_attribute *attrib;
	char *stream_str = NULL;
	char *daddr_str = NULL;
	char *vlan_str = NULL;
	char *size_str = NULL;
	char *interval_str = NULL;
	char *prio_str = NULL;
	char *latency_str = NULL;
	int join = 0;
	uint8_t streamid_firstval[8];
	uint8_t macvec_firstval[6];
	uint8_t macvec_parsestr[64];
	unsigned vlan, size, interval, prio, latency;
	int i;
	//struct msrpdu_talker_fail t0,t1;

	if (NULL == MSRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
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
	switch (buf[1]) {
	case '?':
		msrp_dumptable(client);
		break;
	case '-':
		/* parse the type - either talker, listener or domain */
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[2]) {
		case 'l':
		case 'L':
			if (buflen < 22) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S-L:xxyyzz...' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_LISTENER_TYPE;
			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 8; i++) {
				macvec_parsestr[0] = buf[4 + i * 2];
				macvec_parsestr[1] = buf[5 + i * 2];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &streamid_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					mrpd_send_ctl_msg(client, respbuf,
							  sizeof(respbuf));
					free(attrib);
					goto out;
				}
			}

			memcpy(attrib->attribute.talk_listen.StreamID,
			       streamid_firstval, 8);
			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_LV, attrib);
			break;
		case 'D':
			if (buflen < 18) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S-D:C:%d:P:%d:V:%04x" */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_DOMAIN_TYPE;

			i = 6;
			rc = sscanf((char *)&(buf[6]), "%d", &prio);

			attrib->attribute.domain.SRclassID = (uint8_t) prio;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}
			while ((i < buflen) && (buf[i] != 'P'))
				i++;

			i += 2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%d", &prio);

			attrib->attribute.domain.SRclassPriority =
			    (uint8_t) prio;
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			while ((i < buflen) && (buf[i] != 'V'))
				i++;

			i += 2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%04x", &vlan);
			attrib->attribute.domain.SRclassVID = (uint8_t) vlan;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_LV, attrib);
			break;
		case '-':
			/*
			 * leave a stream - only need the stream ID
			 */

			if (buflen < 22) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S--S:xxyyzz...' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_TALKER_ADV_TYPE;
			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 8; i++) {
				macvec_parsestr[0] = buf[5 + i * 2];
				macvec_parsestr[1] = buf[6 + i * 2];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &streamid_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					mrpd_send_ctl_msg(client, respbuf,
							  sizeof(respbuf));
					goto out;
				}
			}

			memcpy(attrib->attribute.talk_listen.StreamID,
			       streamid_firstval, 8);
			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_LV, attrib);
			break;
		default:
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	case '+':
		/* parse the type - either talker or listener */
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[2]) {
		case 'l':
		case 'L':
			if (buflen < 26) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S+L:xxyyzz...:D:a' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}

			attrib->type = MSRP_LISTENER_TYPE;
			attrib->direction = MSRP_DIRECTION_LISTENER;

			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 8; i++) {
				macvec_parsestr[0] = buf[4 + i * 2];
				macvec_parsestr[1] = buf[5 + i * 2];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &streamid_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					mrpd_send_ctl_msg(client, respbuf,
							  sizeof(respbuf));
					goto out;
				}
			}

			memcpy(attrib->attribute.talk_listen.StreamID,
			       streamid_firstval, 8);
			rc = sscanf((char *)&buf[23], "%d",
				    &(attrib->substate));
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_JOIN, attrib);
			break;
		case 'D':
			if (buflen < 18) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S+D:C:%d:P:%d:V:%04x" */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_DOMAIN_TYPE;

			i = 6;
			rc = sscanf((char *)&(buf[6]), "%d", &prio);

			attrib->attribute.domain.SRclassID = (uint8_t) prio;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}
			while ((i < buflen) && (buf[i] != 'P'))
				i++;

			i += 2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%d", &prio);

			attrib->attribute.domain.SRclassPriority =
			    (uint8_t) prio;
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			while ((i < buflen) && (buf[i] != 'V'))
				i++;

			i += 2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%04x", &vlan);
			attrib->attribute.domain.SRclassVID = (uint8_t) vlan;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				free(attrib);
				goto out;
			}

			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_JOIN, attrib);
			break;
		case '?':
			join = 1;
			/* fallthru */
		case '+':
			/*
			 * create or join a stream
			 * interesting to note the spec doesn't talk about
			 * what happens if two talkers attempt to define the identical
			 * stream twice  - does the bridge report STREAM_CHANGE error?
			 */

			if (buflen < 22) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}
			/*
			   buf[] should look similar to "S:%02x%02x%02x%02x%02x%02x%02x%02x"
			   ":A:%02x%02x%02x%02x%02x%02x"
			   ":V:%04x" \
			   ":Z:%d" \
			   ":I:%d" \
			   ":P:%d" \
			   ":L:%d" \
			 */

			i = 0;

			while (i < buflen) {
				if (buf[i] == ':') {
					stream_str = &(buf[i + 1]);
					i++;
					break;
				}
				i++;
			}

			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'A')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					daddr_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'V')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					vlan_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'Z')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					size_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'I')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					interval_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'P')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					prio_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			while (i < (buflen - 3)) {
				if ((buf[i] == ':') && (buf[i + 1] == 'L')
				    && (buf[i + 2] == ':')) {
					buf[i] = '\0';
					latency_str = &(buf[i + 3]);
					i += 4;
					break;
				}
				i++;
			}

			if (i >= (buflen - 3)) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 8; i++) {
				macvec_parsestr[0] = stream_str[i * 2];
				macvec_parsestr[1] = stream_str[i * 2 + 1];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &streamid_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					mrpd_send_ctl_msg(client, respbuf,
							  sizeof(respbuf));
					goto out;
				}
			}

			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));
			for (i = 0; i < 6; i++) {
				macvec_parsestr[0] = daddr_str[i * 2];
				macvec_parsestr[1] = daddr_str[i * 2 + 1];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &macvec_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					mrpd_send_ctl_msg(client, respbuf,
							  sizeof(respbuf));
					goto out;
				}
			}

			rc = sscanf((char *)vlan_str, "%x", &vlan);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)size_str, "%d", &size);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)interval_str, "%d", &interval);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)prio_str, "%d", &prio);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)latency_str, "%d", &latency);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;
			}

			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				mrpd_send_ctl_msg(client, respbuf,
						  sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_TALKER_ADV_TYPE;
			memcpy(attrib->attribute.talk_listen.StreamID,
			       streamid_firstval, 8);
			memcpy(attrib->attribute.talk_listen.
			       DataFrameParameters.Dest_Addr, macvec_firstval,
			       6);
			memset(attrib->registrar.macaddr, 0, 6);

			attrib->attribute.talk_listen.DataFrameParameters.
			    Vlan_ID = vlan;
			attrib->attribute.talk_listen.TSpec.MaxFrameSize = size;
			attrib->attribute.talk_listen.TSpec.MaxIntervalFrames =
			    interval;
			attrib->attribute.talk_listen.PriorityAndRank = prio;
			attrib->attribute.talk_listen.AccumulatedLatency =
			    latency;

			/*
			msrp_cmd_parse_join_or_new_stream(buf, buflen, &t0);
			msrp_test_cmd_parse_join_or_new_stream(buf+3, buflen, &t1);

			if(memcmp(&t0, &t1, sizeof(t0))==0)
				printf("parse match\n");
			if(memcmp(&t0,&attrib->attribute.talk_listen,sizeof(t0)))
				printf("old and new parse match\n");
			*/

			if (join)
				msrp_event(MRP_EVENT_JOIN, attrib);
			else
				msrp_event(MRP_EVENT_NEW, attrib);
			break;
		default:
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	default:
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		mrpd_send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
		break;
	}
	return 0;
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

	MSRP_db = malloc(sizeof(struct msrp_database));

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

void msrp_bye(struct sockaddr_in *client)
{
	if (NULL != MSRP_db)
		mrp_client_delete(&(MSRP_db->mrp_db.clients), client);
}

int msrp_reclaim(void)
{
	struct msrp_attribute *sattrib, *free_sattrib;

	if (NULL == MSRP_db)
		return 0;

	sattrib = MSRP_db->attrib_list;
	while (NULL != sattrib) {
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
			msrp_send_notifications(free_sattrib, MRP_NOTIFY_LV);
			free(free_sattrib);
		} else
			sattrib = sattrib->next;
	}
	return 0;
}
