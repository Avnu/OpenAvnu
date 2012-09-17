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
 * an MRP (MMRP, MVRP, MSRP) endpoint implementation of 802.1Q-2011
 */
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/user.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <sys/un.h>

#include "mrpd.h"
#include "mrp.h"

/* state machine controls */
int p2pmac;


static int client_lookup(client_t *list, struct sockaddr_in *newclient)
{
	client_t *client_item;

	client_item = list;

	if (NULL == newclient)
		return 0;

	while (NULL != client_item) {
		if (client_item->client.sin_port == newclient->sin_port)
			return 1;
		client_item = client_item->next;
	}
	return 0;
}

int mrp_client_add(client_t **list, struct sockaddr_in *newclient)
{
	client_t *client_item;

	client_item = *list;

	if (NULL == newclient)
		return -1;

	if (client_lookup(client_item, newclient))
		return 0;	/* already present */

	/* handle 1st entry into list */
	if (NULL == client_item) {
		client_item = malloc(sizeof(client_t));
		if (NULL == client_item)
			return -1;
		client_item->next = NULL;
		client_item->client = *newclient;
		*list = client_item;
		return 0;
	}

	while (client_item) {
		if (NULL == client_item->next) {
			client_item->next = malloc(sizeof(client_t));
			if (NULL == client_item->next)
				return -1;
			client_item = client_item->next;
			client_item->next = NULL;
			client_item->client = *newclient;
			return 0;
		}
		client_item = client_item->next;
	}
	return -1;
}

int mrp_client_delete(client_t **list, struct sockaddr_in *newclient)
{
	client_t *client_item;
	client_t *client_last;

	client_item = *list;
	client_last = NULL;

	if (NULL == newclient)
		return 0;

	if (NULL == client_item)
		return 0;

	while (NULL != client_item) {
		if (0 == memcmp((u_int8_t *) newclient,
				(u_int8_t *) &client_item->client,
				sizeof(struct sockaddr_in))) {

			if (client_last) {
				client_last->next = client_item->next;
				free(client_item);
			} else {
				/* reset the head pointer */
				*list = client_item->next;
				free(client_item);
			}
			return 0;
		}
		client_last = client_item;
		client_item = client_item->next;
	}
	/* not found ... no error */
	return 0;
}


int mrp_jointimer_start(struct mrp_database *mrp_db)
{
	/* 10.7.4.1 - interval between transmit opportunities
	 * for applicant state machine
	 */
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	itimerspec_new.it_value.tv_nsec = MRP_JOINTIMER_VAL * 1000000;

	rc = timerfd_settime(mrp_db->join_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;

}

int mrp_jointimer_stop(struct mrp_database *mrp_db)
{
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	rc = timerfd_settime(mrp_db->join_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;

}

int mrp_lvtimer_start(struct mrp_database *mrp_db)
{
	/* leavetimer has expired (10.7.5.21)
	 * controls how long the Registrar state machine stays in the
	 * LV state before transitioning to the MT state.
	 */
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;
	unsigned long lv_next = MRP_LVTIMER_VAL;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	while (lv_next >= 1000) {
		itimerspec_new.it_value.tv_sec++;
		lv_next -= 1000;
	}

	itimerspec_new.it_value.tv_nsec = lv_next * 1000000;

	rc = timerfd_settime(mrp_db->lv_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
}

int mrp_lvtimer_stop(struct mrp_database *mrp_db)
{
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	rc = timerfd_settime(mrp_db->lv_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
}

static unsigned long lva_next;

int mrp_lvatimer_start(struct mrp_database *mrp_db)
{
	/* leavealltimer has expired. (10.7.5.22)
	 * on expire, sends a LEAVEALL message
	 * value is RANDOM in range (LeaveAllTime , 1.5xLeaveAllTime)
	 * timer is for all attributes of a given application and port, but
	 * expires each listed attribute individually (per application)
	 */
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	lva_next = MRP_LVATIMER_VAL + (random() % (MRP_LVATIMER_VAL / 2));

	while (lva_next >= 1000) {
		itimerspec_new.it_value.tv_sec++;
		lva_next -= 1000;
	}

	itimerspec_new.it_value.tv_nsec = lva_next * 1000000;
	rc = timerfd_settime(mrp_db->lva_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
}

int mrp_lvatimer_stop(struct mrp_database *mrp_db)
{
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	rc = timerfd_settime(mrp_db->lva_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
}

int mrp_lvatimer_fsm(struct mrp_database *mrp_db, int event)
{
	int la_state;
	int sndmsg = MRP_SND_NONE;
	int tx = 0;

	if (NULL == mrp_db)
		return -1;

	la_state = mrp_db->lva.state;

	switch (event) {
	case MRP_EVENT_BEGIN:
		la_state = MRP_TIMER_PASSIVE;
		mrp_lvatimer_start(mrp_db);
		break;
	case MRP_EVENT_TX:
		if (la_state == MRP_TIMER_ACTIVE) {
			tx = 1;
			sndmsg = MRP_SND_LVA;
			la_state = MRP_TIMER_PASSIVE;
		}
		break;
	case MRP_EVENT_RLA:
		la_state = MRP_TIMER_PASSIVE;
		mrp_lvatimer_start(mrp_db);
		break;
	case MRP_EVENT_LVATIMER:
		la_state = MRP_TIMER_ACTIVE;
		mrp_lvatimer_start(mrp_db);
		break;
	default:
		printf("mrp_lvatimer_fsm:unexpected event (%d)\n", event);
		return -1;
		break;
	}
	mrp_db->lva.state = la_state;
	mrp_db->lva.sndmsg = sndmsg;
	mrp_db->lva.tx = tx;

	return 0;
}

#ifdef LATER
int mrp_periodictimer_fsm(struct mrp_database *mrp_db, int event)
{
	int p_state;
	int sndmsg = MRP_SND_NONE;
	int tx = 0;

	if (NULL == mrp_db)
		return -1;

	p_state = mrp_db->periodic.state;

	switch (event) {
	case MRP_EVENT_BEGIN:
		p_state = MRP_TIMER_ACTIVE;
		mrp_periodictimer_start();
		break;
	case MRP_EVENT_PERIODIC:
		if (p_state == MRP_TIMER_ACTIVE) {
			mrp_periodictimer_start();
		}
		break;
	case MRP_EVENT_PERIODIC_DISABLE:
		p_state = MRP_TIMER_PASSIVE;
		/* this lets the timer expire without rescheduling */
		break;
	case MRP_EVENT_PERIODIC_ENABLE:
		if (p_state == MRP_TIMER_PASSIVE) {
			p_state = MRP_TIMER_ACTIVE;
			mrp_periodictimer_start();
		}
		break;
	default:
		printf("mrp_periodictimer_fsm:unexpected event (%d)\n", event);
		return;
		break;
	}
	mrp_db->periodic.state = p_state;
	mrp_db->periodic.sndmsg = sndmsg;
	mrp_db->periodic.tx = tx;
}
#endif

/*
 * per-attribute MRP FSM
 */

int mrp_applicant_fsm(mrp_applicant_attribute_t *attrib, int event)
{
	int tx = 0;
	int optional = 0;
	int mrp_state = attrib->mrp_state;
	int sndmsg = MRP_SND_NULL;

	switch (event) {
	case MRP_EVENT_BEGIN:
		mrp_state = MRP_VO_STATE;
		break;
	case MRP_EVENT_NEW:
		/*
		 * New declaration (publish) event as a result
		 * of a mad_join request
		 */
		switch (mrp_state) {
		case MRP_VN_STATE:
		case MRP_AN_STATE:
			/* do nothing */
			break;
		default:
			mrp_state = MRP_VN_STATE;
			break;
		}
		break;
	case MRP_EVENT_JOIN:
		/*
		 * declaration (of interest) event as a result of
		 * a mad_join request
		 */
		switch (mrp_state) {
		case MRP_LO_STATE:
		case MRP_VO_STATE:
			mrp_state = MRP_VP_STATE;
			break;
		case MRP_LA_STATE:
			mrp_state = MRP_AA_STATE;
			break;
		case MRP_AO_STATE:
			mrp_state = MRP_AP_STATE;
			break;
		case MRP_QO_STATE:
			mrp_state = MRP_QP_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_LV:
		/*
		 * declaration removal event as a result of a mad_leave request
		 */
		switch (mrp_state) {
		case MRP_VN_STATE:
		case MRP_AN_STATE:
		case MRP_AA_STATE:
		case MRP_QA_STATE:
			mrp_state = MRP_LA_STATE;
			break;
		case MRP_VP_STATE:
			mrp_state = MRP_VO_STATE;
			break;
		case MRP_AP_STATE:
			mrp_state = MRP_AO_STATE;
			break;
		case MRP_QP_STATE:
			mrp_state = MRP_QO_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_TXLA:
		/*
		 * transmit leaveall is signaled (overrides a TX)
		 */
		switch (mrp_state) {
		case MRP_VO_STATE:
			/* send <NULL> only if it improves encoding */
			optional = 1;
			tx = 1;
			sndmsg = MRP_SND_IN;
			mrp_state = MRP_LO_STATE;
			break;
		case MRP_VP_STATE:
			tx = 1;
			sndmsg = MRP_SND_IN;
			mrp_state = MRP_AA_STATE;
			break;
		case MRP_VN_STATE:
			/* send NEW */
			tx = 1;
			sndmsg = MRP_SND_NEW;
			mrp_state = MRP_AN_STATE;
			break;
		case MRP_AN_STATE:
			/* send NEW */
			tx = 1;
			sndmsg = MRP_SND_NEW;
			mrp_state = MRP_QA_STATE;
			break;
		case MRP_QP_STATE:
		case MRP_AP_STATE:
		case MRP_AA_STATE:
			/* send JOIN */
			tx = 1;
			sndmsg = MRP_SND_JOIN;
			mrp_state = MRP_QA_STATE;
			break;
		case MRP_QA_STATE:
			/* send JOIN */
			tx = 1;
			sndmsg = MRP_SND_JOIN;
			break;
		case MRP_LA_STATE:
		case MRP_AO_STATE:
		case MRP_QO_STATE:
			/* send <NULL> only if it improves encoding */
			optional = 1;
			tx = 1;
			sndmsg = MRP_SND_IN;
			mrp_state = MRP_LO_STATE;
			break;
		case MRP_LO_STATE:
			/* send <NULL> */
			optional = 1;
			tx = 1;
			sndmsg = MRP_SND_IN;
			mrp_state = MRP_VO_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_TXLAF:
		/*
		 * transmit leaveall failure (no room)
		 */
		switch (mrp_state) {
		case MRP_VO_STATE:
			mrp_state = MRP_LO_STATE;
			break;
		case MRP_LO_STATE:
		case MRP_VP_STATE:
		case MRP_VN_STATE:
			/* don't advance state */
			break;
		case MRP_AN_STATE:
			mrp_state = MRP_VN_STATE;
			break;
		case MRP_QP_STATE:
		case MRP_AP_STATE:
		case MRP_AA_STATE:
		case MRP_QA_STATE:
			mrp_state = MRP_VP_STATE;
			break;
		case MRP_QO_STATE:
		case MRP_AO_STATE:
		case MRP_LA_STATE:
			mrp_state = MRP_LO_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_TX:
		/*
		 * transmit updates unless leaveall is signaled
		 * (then it becomes a TXLA)
		 */
		switch (mrp_state) {
		case MRP_VO_STATE:
			/* send <NULL> only if it improves encoding */
			tx = 1;
			optional = 1;
			sndmsg = MRP_SND_IN;
			break;
		case MRP_VP_STATE:
			/* send JOIN */
			tx = 1;
			sndmsg = MRP_SND_JOIN;
			mrp_state = MRP_AA_STATE;
			break;
		case MRP_VN_STATE:
			/* send NEW */
			tx = 1;
			sndmsg = MRP_SND_NEW;
			mrp_state = MRP_AN_STATE;
			break;
		case MRP_AN_STATE:
			/* send NEW */
			tx = 1;
			sndmsg = MRP_SND_NEW;
			mrp_state = MRP_QA_STATE;
			break;
		case MRP_AP_STATE:
		case MRP_AA_STATE:
			/* send JOIN */
			tx = 1;
			sndmsg = MRP_SND_JOIN;
			mrp_state = MRP_QA_STATE;
			break;
		case MRP_QA_STATE:
			/* send JOIN only if it improves encoding */
			tx = 1;
			optional = 1;
			sndmsg = MRP_SND_JOIN;
			break;
		case MRP_LA_STATE:
			/* send LV */
			tx = 1;
			sndmsg = MRP_SND_LV;
			mrp_state = MRP_VO_STATE;
			break;
		case MRP_AO_STATE:
		case MRP_QO_STATE:
		case MRP_QP_STATE:
			/* send <NULL> only if it improves encoding */
			tx = 1;
			optional = 1;
			sndmsg = MRP_SND_IN;
			break;
		case MRP_LO_STATE:
			/* send <NULL> */
			tx = 1;
			sndmsg = MRP_SND_IN;
			mrp_state = MRP_VO_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_RNEW:
		/* do nothing */
		break;

	case MRP_EVENT_RJOININ:
		switch (mrp_state) {
		case MRP_VO_STATE:
			if (0 == p2pmac)
				mrp_state = MRP_AO_STATE;
			break;
		case MRP_VP_STATE:
			if (0 == p2pmac) {
				mrp_state = MRP_AP_STATE;
			}
			break;
		case MRP_AA_STATE:
			mrp_state = MRP_QA_STATE;
			break;
		case MRP_AO_STATE:
			mrp_state = MRP_QO_STATE;
			break;
		case MRP_AP_STATE:
			mrp_state = MRP_QP_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_RIN:
		switch (mrp_state) {
		case MRP_AA_STATE:
			if (1 == p2pmac)
				mrp_state = MRP_QA_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_RJOINMT:
	case MRP_EVENT_RMT:
		switch (mrp_state) {
		case MRP_QA_STATE:
			mrp_state = MRP_AA_STATE;
			break;
		case MRP_QO_STATE:
			mrp_state = MRP_AO_STATE;
			break;
		case MRP_QP_STATE:
			mrp_state = MRP_AP_STATE;
			break;
		case MRP_LO_STATE:
			mrp_state = MRP_VO_STATE;
			break;
		default:
			break;
		}
		break;

	case MRP_EVENT_RLV:
	case MRP_EVENT_RLA:
	case MRP_EVENT_REDECLARE:
		switch (mrp_state) {
		case MRP_VO_STATE:
			mrp_state = MRP_LO_STATE;
			break;
		case MRP_AN_STATE:
			mrp_state = MRP_VN_STATE;
			break;
		case MRP_QA_STATE:
		case MRP_AA_STATE:
			mrp_state = MRP_VP_STATE;
			break;
		case MRP_AO_STATE:
		case MRP_QO_STATE:
			mrp_state = MRP_LO_STATE;
			break;
		case MRP_AP_STATE:
		case MRP_QP_STATE:
			mrp_state = MRP_VP_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_PERIODIC:
		switch (mrp_state) {
		case MRP_QA_STATE:
			mrp_state = MRP_AA_STATE;
			break;
		case MRP_QP_STATE:
			mrp_state = MRP_AP_STATE;
			break;
		default:
			break;
		}
		break;

	default:
		printf("mrp_applicant_fsm:unexpected event (%d)\n", event);
		return -1;
		break;
	}

	attrib->tx = tx;
	attrib->mrp_state = mrp_state;
	attrib->sndmsg = sndmsg;
	attrib->encode = (optional ? MRP_ENCODE_OPTIONAL : MRP_ENCODE_YES);
	return 0;
}

int
mrp_registrar_fsm(mrp_registrar_attribute_t *attrib,
		  struct mrp_database *mrp_db, int event)
{
	int mrp_state = attrib->mrp_state;
	int notify = MRP_NOTIFY_NONE;

	switch (event) {
	case MRP_EVENT_BEGIN:
		mrp_state = MRP_MT_STATE;
		break;
	case MRP_EVENT_RLV:
		notify = MRP_NOTIFY_LV;
		/* fall thru */
	case MRP_EVENT_TXLA:
	case MRP_EVENT_RLA:
	case MRP_EVENT_REDECLARE:
		/*
		 * on link-status-change
		 * trigger Applicant and Registrar state machines to re-declare
		 * previously registered attributes.
		 */
		switch (mrp_state) {
		case MRP_IN_STATE:
			mrp_lvtimer_start(mrp_db);
			mrp_state = MRP_LV_STATE;
		default:
			break;
		}
		break;
	case MRP_EVENT_RNEW:
		notify = MRP_NOTIFY_NEW;
		switch (mrp_state) {
		case MRP_MT_STATE:
		case MRP_IN_STATE:
			mrp_state = MRP_IN_STATE;
			break;
		case MRP_LV_STATE:
			/* should stop leavetimer - but we have only 1 lvtimer
			 * for all attributes, and recieving a LVTIMER event
			 * is a don't-care if the attribute is in the IN state.
			 */
			mrp_state = MRP_IN_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_RJOININ:
	case MRP_EVENT_RJOINMT:
		switch (mrp_state) {
		case MRP_MT_STATE:
			notify = MRP_NOTIFY_JOIN;
			mrp_state = MRP_IN_STATE;
			break;
		case MRP_IN_STATE:
			mrp_state = MRP_IN_STATE;
			break;
		case MRP_LV_STATE:
			/* should stop leavetimer - but we have only 1 lvtimer
			 * for all attributes, and recieving a LVTIMER event
			 * is a don't-care if the attribute is in the IN state.
			 */
			notify = MRP_NOTIFY_JOIN;
			mrp_state = MRP_IN_STATE;
			break;
		default:
			break;
		}
		break;

	case MRP_EVENT_LVTIMER:
		switch (mrp_state) {
		case MRP_LV_STATE:
			mrp_state = MRP_MT_STATE;
			break;
		case MRP_MT_STATE:
			mrp_state = MRP_MT_STATE;
			break;
		case MRP_IN_STATE:
		default:
			break;
		}
		break;
	case MRP_EVENT_FLUSH:
		notify = MRP_NOTIFY_LV;
		switch (mrp_state) {
		case MRP_LV_STATE:
			mrp_state = MRP_MT_STATE;
			break;
		case MRP_MT_STATE:
		case MRP_IN_STATE:
			mrp_state = MRP_MT_STATE;
			break;
		default:
			break;
		}
		break;
	case MRP_EVENT_RMT:
		/* ignore on soon to be deleted attributes */
		break;
	default:
		printf("mrp_registrar_fsm:unexpected event (%d)\n", event);
		return -1;
		break;
	}

	attrib->mrp_state = mrp_state;
	attrib->notify = notify;
	return 0;
}

int mrp_decode_state(mrp_registrar_attribute_t *rattrib,
	mrp_applicant_attribute_t *aattrib, char *str, int strlen) {
	char reg_stat[8];

	switch(rattrib->mrp_state) {
	case MRP_IN_STATE:
		snprintf(reg_stat, sizeof(reg_stat)-1, "IN");
		break;
	case MRP_LV_STATE:
		snprintf(reg_stat, sizeof(reg_stat)-1, "LV");
		break;
	case MRP_MT_STATE:
		snprintf(reg_stat, sizeof(reg_stat)-1, "MT");
		break;
	default:
		snprintf(reg_stat, sizeof(reg_stat)-1, "%d", rattrib->mrp_state);
		break;
	}

	switch(aattrib->mrp_state) {
	case MRP_VO_STATE:
		snprintf(str, strlen-1, "VO/%s", reg_stat);
		break;
	case MRP_VP_STATE:
		snprintf(str, strlen-1, "VP/%s", reg_stat);
		break;
	case MRP_VN_STATE:
		snprintf(str, strlen-1, "VN/%s", reg_stat);
		break;
	case MRP_AN_STATE:
		snprintf(str, strlen-1, "AN/%s", reg_stat);
		break;
	case MRP_AA_STATE:
		snprintf(str, strlen-1, "AA/%s", reg_stat);
		break;
	case MRP_QA_STATE:
		snprintf(str, strlen-1, "QA/%s", reg_stat);
		break;
	case MRP_LA_STATE:
		snprintf(str, strlen-1, "LA/%s", reg_stat);
		break;
	case MRP_AO_STATE:
		snprintf(str, strlen-1, "AO/%s", reg_stat);
		break;
	case MRP_QO_STATE:
		snprintf(str, strlen-1, "QO/%s", reg_stat);
		break;
	case MRP_AP_STATE:
		snprintf(str, strlen-1, "AP/%s", reg_stat);
		break;
	case MRP_QP_STATE:
		snprintf(str, strlen-1, "QP/%s", reg_stat);
		break;
	case MRP_LO_STATE:
		snprintf(str, strlen-1, "LO/%s", reg_stat);
		break;
	default:
		snprintf(str, strlen-1, "%d/%s", aattrib->mrp_state, reg_stat);
		break;
	}
	return 0;
}

int mrp_init(void)
{
	p2pmac = 0;		/* operPointToPointMAC false by default */
	lva_next = MRP_LVATIMER_VAL;	/* leaveall timeout in msec */

	return 0;
}
