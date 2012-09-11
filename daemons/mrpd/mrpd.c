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
/* global mgmt parameters */
int daemonize;
int mmrp_enable;
int mvrp_enable;
int msrp_enable;
int logging_enable;
int mrpd_port;

char *interface;
int interface_fd;

/* state machine controls */
int p2pmac;
int periodic_enable;
int registration;

/* if registration is FIXED or FORBIDDEN
 * updates from MRP are discarded, and
 * only IN and JOININ messages are sent
 */

int participant;

/* if participant role is 'SILENT' (or non-participant)
 * applicant doesn't send any messages - configured per-attribute
 */

#define VERSION_STR	"0.0"

static const char *version_str =
    "mrpd v" VERSION_STR "\n" "Copyright (c) 2012, Intel Corporation\n";

unsigned char MMRP_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x20 };
unsigned char MVRP_CUSTOMER_BRIDGE_ADDR[] = \
	{ 0x01, 0x80, 0xC2, 0x00, 0x00, 0x21 };	/* 81-00 */
unsigned char MVRP_PROVIDER_BRIDGE_ADDR[] = \
	{ 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0D };	/* 88-A8 */
unsigned char MSRP_ADDR[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E };
unsigned char STATION_ADDR[] = { 0x00, 0x88, 0x77, 0x66, 0x55, 0x44 };

/* global variables */
int control_socket;
int mmrp_socket;
int mvrp_socket;
int msrp_socket;

int periodic_timer;
int gc_timer;

struct mmrp_database *MMRP_db;
struct mvrp_database *MVRP_db;
struct msrp_database *MSRP_db;

int client_lookup(client_t *list, struct sockaddr_in *newclient)
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

int client_add(client_t **list, struct sockaddr_in *newclient)
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

int client_delete(client_t **list, struct sockaddr_in *newclient)
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

int gctimer_start()
{

	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	/* reclaim memory every 30 minutes */
	itimerspec_new.it_interval.tv_sec = 30;
	itimerspec_new.it_value.tv_sec = 30;

	rc = timerfd_settime(gc_timer, 0, &itimerspec_new, &itimerspec_old);

	return rc;
}

int periodictimer_start()
{

	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	/* periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	itimerspec_new.it_interval.tv_sec = 1;
	itimerspec_new.it_value.tv_sec = 1;

	rc = timerfd_settime(periodic_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
}

int periodictimer_stop()
{
	/* periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */
	int rc;
	struct itimerspec itimerspec_new;
	struct itimerspec itimerspec_old;

	/* periodictimer has expired. (10.7.5.23)
	 * PeriodicTransmission state machine generates periodic events
	 * period is one-per-sec
	 */

	memset(&itimerspec_new, 0, sizeof(itimerspec_new));
	memset(&itimerspec_old, 0, sizeof(itimerspec_old));

	itimerspec_new.it_interval.tv_sec = 1;

	rc = timerfd_settime(periodic_timer, 0, &itimerspec_new,
			     &itimerspec_old);

	return rc;
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

int mrp_decode_state(mrp_registrar_attribute_t *rattrib, \
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

/* prototypes */
int send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data,
		 int notify_len);
/* MMRP */
int mmrp_send_notifications(struct mmrp_attribute *attrib, int notify);
int mmrp_txpdu(void);
int mvrp_send_notifications(struct mvrp_attribute *attrib, int notify);
int mvrp_txpdu(void);
int msrp_send_notifications(struct msrp_attribute *attrib, int notify);
int msrp_txpdu(void);

struct mmrp_attribute *mmrp_lookup(struct mmrp_attribute *rattrib)
{
	struct mmrp_attribute *attrib;
	int mac_eq;

	attrib = MMRP_db->attrib_list;
	while (NULL != attrib) {
		if (rattrib->type == attrib->type) {
			if (MMRP_SVCREQ_TYPE == attrib->type) {
				/* this makes no sense to me ...
				 * its boolean .. but ..
				 */
				if (attrib->attribute.svcreq ==
				    rattrib->attribute.svcreq)
					return attrib;
			} else {
				mac_eq = memcmp(attrib->attribute.macaddr,
						rattrib->attribute.macaddr, 6);
				if (0 == mac_eq)
					return attrib;
			}
		}
		attrib = attrib->next;
	}
	return NULL;
}

int mmrp_add(struct mmrp_attribute *rattrib)
{
	struct mmrp_attribute *attrib;
	struct mmrp_attribute *attrib_tail;
	int mac_eq;

	/* XXX do a lookup first to guarantee uniqueness? */

	attrib_tail = attrib = MMRP_db->attrib_list;

	while (NULL != attrib) {
		/* sort list into types, then sorted in order within types */
		if (rattrib->type == attrib->type) {
			if (MMRP_SVCREQ_TYPE == attrib->type) {
				if (attrib->attribute.svcreq <
				    rattrib->attribute.svcreq) {
					/* possible tail insertion ... */

					if (NULL != attrib->next) {
						if (MMRP_SVCREQ_TYPE ==
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
						MMRP_db->attrib_list = rattrib;

					return 0;
				}
			} else {
				mac_eq = memcmp(attrib->attribute.macaddr,
						rattrib->attribute.macaddr, 6);

				if (mac_eq < 0) {
					/* possible tail insertion ... */

					if (NULL != attrib->next) {
						if (MMRP_MACVEC_TYPE ==
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
						MMRP_db->attrib_list = rattrib;

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
		MMRP_db->attrib_list = rattrib;
	} else {
		rattrib->next = NULL;
		rattrib->prev = attrib_tail;
		attrib_tail->next = rattrib;
	}

	return 0;
}

int mmrp_merge(struct mmrp_attribute *rattrib)
{
	struct mmrp_attribute *attrib;

	attrib = mmrp_lookup(rattrib);

	if (NULL == attrib)
		return -1;	/* shouldn't happen */

	/* primarily we update the last mac address state for diagnostics */
	memcpy(attrib->registrar.macaddr, rattrib->registrar.macaddr, 6);
	return 0;
}

int mmrp_event(int event, struct mmrp_attribute *rattrib)
{
	struct mmrp_attribute *attrib;

	switch (event) {
	case MRP_EVENT_LVATIMER:
		mrp_jointimer_stop(&(MMRP_db->mrp_db));
		/* update state */
		attrib = MMRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TXLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db), MRP_EVENT_TXLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MMRP_db->mrp_db), MRP_EVENT_LVATIMER);

		mmrp_txpdu();
		break;
	case MRP_EVENT_RLA:
		mrp_jointimer_stop(&(MMRP_db->mrp_db));
		/* update state */
		attrib = MMRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_RLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db), MRP_EVENT_RLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MMRP_db->mrp_db), MRP_EVENT_RLA);

		break;
	case MRP_EVENT_TX:
		mrp_jointimer_stop(&(MMRP_db->mrp_db));
		attrib = MMRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TX);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MMRP_db->mrp_db), MRP_EVENT_TX);

		mmrp_txpdu();
		break;
	case MRP_EVENT_LVTIMER:
		mrp_lvtimer_stop(&(MMRP_db->mrp_db));
		attrib = MMRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db),
					  MRP_EVENT_LVTIMER);

			attrib = attrib->next;
		}
		break;
	case MRP_EVENT_PERIODIC:
		attrib = MMRP_db->attrib_list;

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
		mrp_jointimer_start(&(MMRP_db->mrp_db));
		if (NULL == rattrib)
			return -1;	/* XXX internal fault */

		/* update state */
		attrib = mmrp_lookup(rattrib);

		if (NULL == attrib) {
			mmrp_add(rattrib);
			attrib = rattrib;
		} else {
			mmrp_merge(rattrib);
			free(rattrib);
		}

		mrp_applicant_fsm(&(attrib->applicant), event);
		/* remap local requests into registrar events */
		switch (event) {
		case MRP_EVENT_NEW:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db), MRP_EVENT_RNEW);
			break;
		case MRP_EVENT_JOIN:
			if (MRP_IN_STATE == attrib->registrar.mrp_state)
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MMRP_db->mrp_db),
						  MRP_EVENT_RJOININ);
			else
				mrp_registrar_fsm(&(attrib->registrar),
						  &(MMRP_db->mrp_db),
						  MRP_EVENT_RJOINMT);
			break;
		case MRP_EVENT_LV:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db), MRP_EVENT_RLV);
			break;
		default:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MMRP_db->mrp_db), event);
			break;
		}
		break;
	default:
		break;
	}

	/*
	 * XXX should honor the MMRP_db->mrp_db.registration and
	 * MMRP_db->mrp_db.participant controls
	 */

	/* generate local notifications */
	attrib = MMRP_db->attrib_list;

	while (NULL != attrib) {
		if (MRP_NOTIFY_NONE != attrib->registrar.notify) {
			mmrp_send_notifications(attrib,
						attrib->registrar.notify);
			attrib->registrar.notify = MRP_NOTIFY_NONE;
		}
		attrib = attrib->next;
	}

	return 0;
}

struct mmrp_attribute *mmrp_alloc()
{
	struct mmrp_attribute *attrib;

	attrib = malloc(sizeof(struct mmrp_attribute));
	if (NULL == attrib)
		return NULL;

	memset(attrib, 0, sizeof(struct mmrp_attribute));

	attrib->applicant.mrp_state = MRP_VO_STATE;
	attrib->applicant.tx = 0;
	attrib->applicant.sndmsg = MRP_SND_NULL;
	attrib->applicant.encode = MRP_ENCODE_OPTIONAL;

	attrib->registrar.mrp_state = MRP_MT_STATE;
	attrib->registrar.notify = MRP_NOTIFY_NONE;

	return attrib;
}

void mmrp_increment_macaddr(u_int8_t *macaddr)
{

	int i;

	i = 5;
	while (i > 0) {
		if (255 != macaddr[i]) {
			macaddr[i]++;
			return;
		}
		macaddr[i] = 0;
		i--;
	}
	return;
}

int recv_mmrp_msg()
{
	char *msgbuf;
	struct sockaddr_ll client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	eth_hdr_t *eth;
	mrpdu_t *mrpdu;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr;
	unsigned char *mrpdu_msg_eof;
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	u_int16_t numvalues;
	u_int16_t numvalues_processed;
	int numvectorbytes;
	u_int8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	u_int8_t svcreq_firstval;
	u_int8_t macvec_firstval[6];
	struct mmrp_attribute *attrib;
	int endmarks;

	msgbuf = (char *)malloc(MAX_FRAME_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(msgbuf, 0, MAX_FRAME_SIZE);

	iov.iov_len = MAX_FRAME_SIZE;
	iov.iov_base = msgbuf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	bytes = recvmsg(mmrp_socket, &msg, 0);
	if (bytes <= 0)
		goto out;

	if ((unsigned int)bytes < (sizeof(eth_hdr_t) + sizeof(mrpdu_t) +
				   sizeof(mrpdu_message_t)))
		goto out;

	eth = (eth_hdr_t *) msgbuf;

	/* note that MMRP frames should always arrive untagged (no vlan) */
	if (MMRP_ETYPE != ntohs(eth->typelen))
		goto out;

	/* XXX check dest mac address too? */

	mrpdu = (mrpdu_t *) (msgbuf + sizeof(struct eth_hdr));

	if (MMRP_PROT_VER != mrpdu->ProtocolVersion)	/* should accept */
		goto out;

	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= offsetof(mrpdu_t, MessageList);

	endmarks = 0;

	while (mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) {
		mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
		if ((mrpdu_msg->AttributeType == 0) &&
		    (mrpdu_msg->AttributeLength == 0)) {
			mrpdu_msg_ptr += 2;
			endmarks++;
			if (endmarks < 2)
				continue;	/* end-mark */
			else
				break;	/* two endmarks - end of messages */
		}

		endmarks = 0;	/* reset the endmark counter */

		switch (mrpdu_msg->AttributeType) {
		case MMRP_SVCREQ_TYPE:
			if (mrpdu_msg->AttributeLength != 1) {
				/* we could seek for an endmark to recover
				 */
				goto out;
			}
			/* AttributeListLength not used for MMRP, hence
			 * Data points to the beginning of the VectorAttributes
			 */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) mrpdu_msg->Data;
			mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;

			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues)
					/* Malformed - cant tell how long the trailing vectors are */
					goto out;

				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof)
					/* Malformed - runs off the end of the pdu */
					goto out;

				svcreq_firstval =
				    mrpdu_vectorptr->FirstValue_VectorEvents[0];

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 1;
				     vectidx <= (numvectorbytes + 1);
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

						if (1 < svcreq_firstval)	/*discard junk */
							continue;

						attrib = mmrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type = MMRP_SVCREQ_TYPE;
						attrib->attribute.svcreq =
						    svcreq_firstval;
						svcreq_firstval++;
						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							mmrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							mmrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							mmrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							mmrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							mmrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							mmrp_event
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

				/* as either firstval is either 0 or 1 ... the common
				 * case will be nulls for evt_2 and evt_3 ...
				 */
				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					mmrp_event(MRP_EVENT_RLA, NULL);

				/* 2 byte numvalues + 34 byte FirstValue + (n) vector bytes */
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 3 + numvectorbytes;
				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}

			break;
		case MMRP_MACVEC_TYPE:
			if (mrpdu_msg->AttributeLength != 6) {
				/* we can seek for an endmark to recover .. but this version
				 * dumps the entire packet as malformed
				 */
				goto out;
			}
			/* AttributeListLength not used for MMRP, hence
			 * Data points to the beginning of the VectorAttributes
			 */
			mrpdu_vectorptr =
			    (mrpdu_vectorattrib_t *) mrpdu_msg->Data;
			mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;

			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues)
					/* Malformed - cant tell how long the trailing vectors are */
					goto out;

				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof)
					/* Malformed - runs off the end of the pdu */
					goto out;

				memcpy(macvec_firstval,
				       mrpdu_vectorptr->FirstValue_VectorEvents,
				       6);

				/* if not an even multiple ... */
				if (numvalues != ((numvalues / 3) * 3))
					numvectorbytes = (numvalues / 3) + 1;
				else
					numvectorbytes = (numvalues / 3);

				for (vectidx = 6;
				     vectidx <= (numvectorbytes + 6);
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

						attrib = mmrp_alloc();
						if (NULL == attrib)
							goto out;	/* oops - internal error */

						attrib->type = MMRP_MACVEC_TYPE;
						memcpy(attrib->attribute.
						       macaddr, macvec_firstval,
						       6);
						mmrp_increment_macaddr
						    (macvec_firstval);

						memcpy(attrib->registrar.
						       macaddr, eth->srcaddr,
						       6);

						switch (vectevt[vectevt_idx]) {
						case MRPDU_NEW:
							mmrp_event
							    (MRP_EVENT_RNEW,
							     attrib);
							break;
						case MRPDU_JOININ:
							mmrp_event
							    (MRP_EVENT_RJOININ,
							     attrib);
							break;
						case MRPDU_IN:
							mmrp_event
							    (MRP_EVENT_RIN,
							     attrib);
							break;
						case MRPDU_JOINMT:
							mmrp_event
							    (MRP_EVENT_RJOINMT,
							     attrib);
							break;
						case MRPDU_MT:
							mmrp_event
							    (MRP_EVENT_RMT,
							     attrib);
							break;
						case MRPDU_LV:
							mmrp_event
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
					mmrp_event(MRP_EVENT_RLA, NULL);

				/* 1 byte Type, 1 byte Len, 6 byte FirstValue, and (n) vector bytes */
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 8 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		default:
			/* unrecognized attribute type
			 * we can seek for an endmark to recover .. but this version
			 * dumps the entire packet as malformed
			 */
			goto out;
		}
	}

	free(msgbuf);
	return 0;
 out:
	free(msgbuf);

	return -1;
}

int
mmrp_emit_svcvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
		     int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	u_int16_t numvalues;
	u_int8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	u_int8_t svcreq_firstval;
	struct mmrp_attribute *attrib, *vattrib;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;

	/* need at least 6 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - 6))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MMRP_SVCREQ_TYPE;
	mrpdu_msg->AttributeLength = 1;

	attrib = MMRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg->Data;

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) && (NULL != attrib)) {

		if (MMRP_SVCREQ_TYPE != attrib->type) {
			attrib = attrib->next;
			continue;
		}

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
		svcreq_firstval = attrib->attribute.svcreq;
		mrpdu_vectorptr->FirstValue_VectorEvents[0] =
		    attrib->attribute.svcreq;

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

		/* now attempt to vectorize contiguous other attributes
		 * which also need to be transmitted
		 */

		vectidx = 2;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (MMRP_SVCREQ_TYPE != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			svcreq_firstval++;

			if (vattrib->attribute.svcreq != svcreq_firstval)
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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_3pack;
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

		/* 2 byte header, followed by FirstValues/Vectors - remember vectidx starts at 0 */
		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

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

int
mmrp_emit_macvectors(unsigned char *msgbuf, unsigned char *msgbuf_eof,
		     int *bytes_used, int lva)
{
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;
	u_int16_t numvalues;
	u_int8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	u_int8_t macvec_firstval[6];
	struct mmrp_attribute *attrib, *vattrib;
	int mac_eq;

	/* need at least 11 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - 11))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MMRP_MACVEC_TYPE;
	mrpdu_msg->AttributeLength = 6;

	attrib = MMRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg->Data;

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) && (NULL != attrib)) {

		if (MMRP_MACVEC_TYPE != attrib->type) {
			attrib = attrib->next;
			continue;
		}

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
		memcpy(macvec_firstval, attrib->attribute.macaddr, 6);
		memcpy(mrpdu_vectorptr->FirstValue_VectorEvents,
		       attrib->attribute.macaddr, 6);

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

		vectidx = 6;
		vattrib = attrib->next;

		while (NULL != vattrib) {
			if (MMRP_MACVEC_TYPE != vattrib->type)
				break;

			if (0 == vattrib->applicant.tx)
				break;

			mmrp_increment_macaddr(macvec_firstval);

			mac_eq =
			    memcmp(vattrib->attribute.macaddr, macvec_firstval,
				   6);

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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_3pack;
				vectidx++;
				vectevt[0] = 0;
				vectevt[1] = 0;
				vectevt[2] = 0;
				vectevt_idx = 0;
			}

			if (&(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx])
			    > (mrpdu_msg_eof - 11))
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

		/* 2 byte header, followed by FirstValues/Vectors - remember vectidx starts at 0 */
		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

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

int mmrp_txpdu(void)
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

	/* note that MMRP frames should always be untagged (no vlan) */
	eth->typelen = htons(MMRP_ETYPE);
	memcpy(eth->destaddr, MMRP_ADDR, sizeof(eth->destaddr));
	memcpy(eth->srcaddr, STATION_ADDR, sizeof(eth->srcaddr));

	msgbuf_wrptr += sizeof(eth_hdr_t);

	mrpdu = (mrpdu_t *) msgbuf_wrptr;

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

	mrpdu->ProtocolVersion = MMRP_PROT_VER;
	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;
	mrpdu_msg_eof = (unsigned char *)msgbuf + MAX_FRAME_SIZE;

	/*
	 * Iterate over all attributes, transmitting those marked
	 * with 'tx', attempting to coalesce multiple contiguous tx attributes
	 * with appropriate vector encodings.
	 *
	 * MMRP_MACVEC_TYPE FirstValue is the 6-byte MAC address, with
	 * corresponding attrib_length=6
	 *
	 * MMRP_SVCREQ_TYPE FirstValue is a single byte - values follow,
	 * attrib_length=1
	 *
	 * MMRP uses ThreePackedEvents for all vector encodings
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
	 * variable number of vectors. So with MMRP, you will have at most
	 * two messages (SVCREQ or MACVEC)
	 */

	if (MMRP_db->mrp_db.lva.tx) {
		lva = 1;
		MMRP_db->mrp_db.lva.tx = 0;
	}

	rc = mmrp_emit_macvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	if (mrpdu_msg_ptr >= (mrpdu_msg_eof - 2))
		goto out;

	rc = mmrp_emit_svcvectors(mrpdu_msg_ptr, mrpdu_msg_eof, &bytes, lva);
	if (-1 == rc)
		goto out;

	mrpdu_msg_ptr += bytes;

	/* endmark */

	if (mrpdu_msg_ptr == (unsigned char *)mrpdu->MessageList) {
		free(msgbuf);
		return 0;
	}

	if (mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) {
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
		*mrpdu_msg_ptr = 0;
		mrpdu_msg_ptr++;
	} else
		goto out;

	msgbuf_len = mrpdu_msg_ptr - msgbuf;

	bytes = send(mmrp_socket, msgbuf, msgbuf_len, 0);
	if (bytes <= 0)
		goto out;

	free(msgbuf);
	return 0;
 out:
	free(msgbuf);
	/* caller should assume TXLAF */
	return -1;
}

int mmrp_send_notifications(struct mmrp_attribute *attrib, int notify)
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

	if (MMRP_SVCREQ_TYPE == attrib->type) {
		sprintf(variant, "S%d", attrib->attribute.svcreq);
	} else {
		sprintf(variant, "M%02x%02x%02x%02x%02x%02x",
			attrib->attribute.macaddr[0],
			attrib->attribute.macaddr[1],
			attrib->attribute.macaddr[2],
			attrib->attribute.macaddr[3],
			attrib->attribute.macaddr[4],
			attrib->attribute.macaddr[5]);
	}

	sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x",
		attrib->registrar.macaddr[0],
		attrib->registrar.macaddr[1],
		attrib->registrar.macaddr[2],
		attrib->registrar.macaddr[3],
		attrib->registrar.macaddr[4], attrib->registrar.macaddr[5]);
	switch (attrib->registrar.mrp_state) {
	case MRP_IN_STATE:
		sprintf(stage, "MIN %s %s\n", variant, regsrc);
		break;
	case MRP_LV_STATE:
		sprintf(stage, "MLV %s %s\n", variant, regsrc);
		break;
	case MRP_MT_STATE:
		sprintf(stage, "MMT %s %s\n", variant, regsrc);
		break;
	default:
		break;
	}

	switch (notify) {
	case MRP_NOTIFY_NEW:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "MNE %s", stage);
		break;
	case MRP_NOTIFY_JOIN:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "MJO %s", stage);
		break;
	case MRP_NOTIFY_LV:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "MLE %s", stage);
		break;
	default:
		goto free_msgbuf;
		break;
	}

	client = MMRP_db->mrp_db.clients;
	while (NULL != client) {
		send_ctl_msg(&(client->client), msgbuf, MAX_MRPD_CMDSZ);
		client = client->next;
	}

 free_msgbuf:
	if (variant)
		free(variant);
	if (stage)
		free(stage);
	if (regsrc)
		free(regsrc);
	free(msgbuf);
	return 0;
}

int mmrp_dumptable(struct sockaddr_in *client)
{
	char *msgbuf;
	char *msgbuf_wrptr;
	char *stage;
	char *variant;
	char *regsrc;
	struct mmrp_attribute *attrib;

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

	attrib = MMRP_db->attrib_list;

	while (NULL != attrib) {
		if (MMRP_SVCREQ_TYPE == attrib->type) {
			sprintf(variant, "S%d", attrib->attribute.svcreq);
		} else {
			sprintf(variant, "M%02x%02x%02x%02x%02x%02x",
				attrib->attribute.macaddr[0],
				attrib->attribute.macaddr[1],
				attrib->attribute.macaddr[2],
				attrib->attribute.macaddr[3],
				attrib->attribute.macaddr[4],
				attrib->attribute.macaddr[5]);
		}
		sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x",
			attrib->registrar.macaddr[0],
			attrib->registrar.macaddr[1],
			attrib->registrar.macaddr[2],
			attrib->registrar.macaddr[3],
			attrib->registrar.macaddr[4],
			attrib->registrar.macaddr[5]);
		switch (attrib->registrar.mrp_state) {
		case MRP_IN_STATE:
			sprintf(stage, "MIN %s %s\n", variant, regsrc);
			break;
		case MRP_LV_STATE:
			sprintf(stage, "MLV %s %s\n", variant, regsrc);
			break;
		case MRP_MT_STATE:
			sprintf(stage, "MMT %s %s\n", variant, regsrc);
			break;
		default:
			break;
		}
		sprintf(msgbuf_wrptr, "%s", stage);
		msgbuf_wrptr += strnlen(stage, 128);
		attrib = attrib->next;
	}

	send_ctl_msg(client, msgbuf, MAX_MRPD_CMDSZ);

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

int recv_mmrp_cmd(char *buf, int buflen, struct sockaddr_in *client)
{
	int rc;
	char respbuf[8];
	struct mmrp_attribute *attrib;
	u_int8_t svcreq_firstval;
	u_int8_t macvec_firstval[6];
	u_int8_t macvec_parsestr[8];
	int i;

	if (NULL == MMRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}

	rc = client_add(&(MMRP_db->mrp_db.clients), client);

	if (buflen < 3)
		return -1;

	if ('M' != buf[0])
		return -1;

	/*
	 * M?? - query MMRP Registrar MAC Address database
	 * M+? - JOIN a MAC address or service declaration
	 * M++   NEW a MAC Address (XXX: MMRP doesn't use 'New' though?)
	 * M-- - LV a MAC address or service declaration
	 */
	switch (buf[1]) {
	case '?':
		mmrp_dumptable(client);
		break;
	case '-':
		/* parse the type - service request or MACVEC */
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[3]) {
		case 's':
		case 'S':
			/* buf[] should look similar to 'M--s1' */
			svcreq_firstval = buf[4] - '0';
			if (svcreq_firstval > 1) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			attrib = mmrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MMRP_SVCREQ_TYPE;
			attrib->attribute.svcreq = svcreq_firstval;
			memset(attrib->registrar.macaddr, 0, 6);

			mmrp_event(MRP_EVENT_LV, attrib);
			break;
		case 'm':
		case 'M':
			/*
			 * XXX note could also register VID with mac address if we ever wanted to
			 * support more than one Spanning Tree context
			 */

			/* buf[] should look similar to 'M--m010203040506' */
			if (buflen < 16) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 6; i++) {
				macvec_parsestr[0] = buf[4 + i * 2];
				macvec_parsestr[1] = buf[5 + i * 2];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &macvec_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					send_ctl_msg(client, respbuf,
						     sizeof(respbuf));
					goto out;
				}
			}

			attrib = mmrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MMRP_MACVEC_TYPE;
			memcpy(attrib->attribute.macaddr, macvec_firstval, 6);
			memset(attrib->registrar.macaddr, 0, 6);

			mmrp_event(MRP_EVENT_LV, attrib);
			break;
		default:
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	case '+':
		/* parse the type - service request or MACVEC */
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[3]) {
		case 's':
		case 'S':
			/* buf[] should look similar to 'M+?s1'
			 * or buf[] should look similar to 'M++s1'
			 */
			if (('?' != buf[2]) && ('+' != buf[2])) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			svcreq_firstval = buf[4] - '0';
			if (svcreq_firstval > 1) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			attrib = mmrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MMRP_SVCREQ_TYPE;
			attrib->attribute.svcreq = svcreq_firstval;
			memset(attrib->registrar.macaddr, 0, 6);

			mmrp_event(MRP_EVENT_JOIN, attrib);
			break;
		case 'm':
		case 'M':
			/* buf[] should look similar to 'M+?m010203040506' */
			if (buflen < 16) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			memset(macvec_parsestr, 0, sizeof(macvec_parsestr));

			for (i = 0; i < 6; i++) {
				macvec_parsestr[0] = buf[4 + i * 2];
				macvec_parsestr[1] = buf[5 + i * 2];

				rc = sscanf((char *)macvec_parsestr, "%hhx",
					    &macvec_firstval[i]);
				if (0 == rc) {
					snprintf(respbuf, sizeof(respbuf) - 1,
						 "ERP %s", buf);
					send_ctl_msg(client, respbuf,
						     sizeof(respbuf));
					goto out;
				}
			}

			attrib = mmrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MMRP_MACVEC_TYPE;
			memcpy(attrib->attribute.macaddr, macvec_firstval, 6);
			memset(attrib->registrar.macaddr, 0, 6);

			mmrp_event(MRP_EVENT_JOIN, attrib);
			break;
		default:
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	default:
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
		break;
	}
	return 0;
 out:
	return -1;
}

/* MVRP */

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
	int rc;

	switch (event) {
	case MRP_EVENT_LVATIMER:
		mrp_jointimer_stop(&(MVRP_db->mrp_db));
		/* update state */
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TXLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_TXLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_LVATIMER);

		mvrp_txpdu();
		break;
	case MRP_EVENT_RLA:
		mrp_jointimer_stop(&(MVRP_db->mrp_db));
		/* update state */
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_RLA);
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_RLA);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_RLA);

		break;
	case MRP_EVENT_TX:
		mrp_jointimer_stop(&(MVRP_db->mrp_db));
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_applicant_fsm(&(attrib->applicant), MRP_EVENT_TX);
			attrib = attrib->next;
		}

		mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_TX);

		mvrp_txpdu();
		break;
	case MRP_EVENT_LVTIMER:
		mrp_lvtimer_stop(&(MVRP_db->mrp_db));
		attrib = MVRP_db->attrib_list;

		while (NULL != attrib) {
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db),
					  MRP_EVENT_LVTIMER);

			attrib = attrib->next;
		}
		break;
	case MRP_EVENT_PERIODIC:
		attrib = MVRP_db->attrib_list;

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

		mrp_applicant_fsm(&(attrib->applicant), event);
		/* remap local requests into registrar events */
		switch (event) {
		case MRP_EVENT_NEW:
			mrp_registrar_fsm(&(attrib->registrar),
					  &(MVRP_db->mrp_db), MRP_EVENT_RNEW);
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

	attrib = malloc(sizeof(struct mvrp_attribute));
	if (NULL == attrib)
		return NULL;

	memset(attrib, 0, sizeof(struct mvrp_attribute));

	attrib->applicant.mrp_state = MRP_VO_STATE;
	attrib->applicant.tx = 0;
	attrib->applicant.sndmsg = MRP_SND_NULL;
	attrib->applicant.encode = MRP_ENCODE_OPTIONAL;

	attrib->registrar.mrp_state = MRP_MT_STATE;
	attrib->registrar.notify = MRP_NOTIFY_NONE;

	return attrib;
}

int recv_mvrp_msg()
{
	char *msgbuf;
	struct sockaddr_ll client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	eth_hdr_t *eth;
	mrpdu_t *mrpdu;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr;
	unsigned char *mrpdu_msg_eof;
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	u_int16_t numvalues;
	u_int16_t numvalues_processed;
	int numvectorbytes;
	u_int8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	u_int16_t vid_firstval;
	struct mvrp_attribute *attrib;
	int endmarks;

	msgbuf = (char *)malloc(MAX_FRAME_SIZE);
	if (NULL == msgbuf)
		return -1;
	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(msgbuf, 0, MAX_FRAME_SIZE);

	iov.iov_len = MAX_FRAME_SIZE;
	iov.iov_base = msgbuf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	bytes = recvmsg(mvrp_socket, &msg, 0);
	if (bytes <= 0)
		goto out;

	if ((unsigned int)bytes < (sizeof(eth_hdr_t) + sizeof(mrpdu_t) +
				   sizeof(mrpdu_message_t)))
		goto out;

	eth = (eth_hdr_t *) msgbuf;

	/* note that MVRP frames should always arrive untagged (no vlan) */
	if (MVRP_ETYPE != ntohs(eth->typelen))
		goto out;

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

	if (MVRP_PROT_VER != mrpdu->ProtocolVersion)	/* XXX should accept ... */
		goto out;

	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= offsetof(mrpdu_t, MessageList);

	/*
	 * MVRP_VID_TYPE FirstValue is the 12 bit (2-byte) VLAN with
	 * corresponding attrib_length=2
	 *
	 * MVRP uses ThreePackedEvents for all vector encodings
	 *
	 * walk list until we run to the end of the PDU, or encounter a double end-mark
	 */

	endmarks = 0;

	while (mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) {
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
			mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;

			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues)
					/* Malformed - cant tell how long the trailing vectors are */
					goto out;

				if ((mrpdu_vectorptr->FirstValue_VectorEvents +
				     numvalues / 3) >= mrpdu_msg_eof)
					/* Malformed - runs off the end of the pdu */
					goto out;

				vid_firstval =
				    (((u_int16_t) mrpdu_vectorptr->
				      FirstValue_VectorEvents[0]) << 8)
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

				if (MRPDU_VECT_LVA
				    (ntohs(mrpdu_vectorptr->VectorHeader)))
					mvrp_event(MRP_EVENT_RLA, NULL);

				/* 1 byte Type, 1 byte Len, 2 byte FirstValue, and (n) vector bytes */
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
				mrpdu_msg_ptr += 4 + numvectorbytes;

				mrpdu_vectorptr =
				    (mrpdu_vectorattrib_t *) mrpdu_msg_ptr;
			}
			break;
		default:
			/* unrecognized attribute type
			 * we can seek for an endmark to recover .. but this version
			 * dumps the entire packet as malformed
			 */
			goto out;
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
	u_int16_t numvalues;
	u_int8_t vect_3pack;
	int vectidx;
	unsigned int vectevt[3];
	int vectevt_idx;
	u_int16_t vid_firstval;
	struct mvrp_attribute *attrib, *vattrib;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr = msgbuf;
	unsigned char *mrpdu_msg_eof = msgbuf_eof;

	/* need at least 6 bytes for a single vector */
	if (mrpdu_msg_ptr > (mrpdu_msg_eof - 6))
		goto oops;

	mrpdu_msg = (mrpdu_message_t *) mrpdu_msg_ptr;
	mrpdu_msg->AttributeType = MVRP_VID_TYPE;
	mrpdu_msg->AttributeLength = 2;

	attrib = MVRP_db->attrib_list;

	mrpdu_vectorptr = (mrpdu_vectorattrib_t *) mrpdu_msg->Data;

	while ((mrpdu_msg_ptr < (mrpdu_msg_eof - 2)) && (NULL != attrib)) {

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
		attrib->applicant.tx = 0;
		vid_firstval = attrib->attribute;
		mrpdu_vectorptr->FirstValue_VectorEvents[0] =
		    (u_int8_t) (attrib->attribute >> 8);
		mrpdu_vectorptr->FirstValue_VectorEvents[1] =
		    (u_int8_t) (attrib->attribute);

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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_3pack;
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

		/* 2 byte header, followed by FirstValues/Vectors - remember vectidx starts at 0 */
		mrpdu_msg_ptr =
		    &(mrpdu_vectorptr->FirstValue_VectorEvents[vectidx]);

		attrib = attrib->next;

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

	mrpdu->ProtocolVersion = MVRP_PROT_VER;
	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;
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

	if (mrpdu_msg_ptr == (unsigned char *)mrpdu->MessageList) {
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

	bytes = send(mvrp_socket, msgbuf, msgbuf_len, 0);
	if (bytes <= 0)
		goto out;

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

	sprintf(variant, "%04x", attrib->attribute);

	sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x",
		attrib->registrar.macaddr[0],
		attrib->registrar.macaddr[1],
		attrib->registrar.macaddr[2],
		attrib->registrar.macaddr[3],
		attrib->registrar.macaddr[4], attrib->registrar.macaddr[5]);
	switch (attrib->registrar.mrp_state) {
	case MRP_IN_STATE:
		sprintf(stage, "VIN %s %s\n", variant, regsrc);
		break;
	case MRP_LV_STATE:
		sprintf(stage, "VLV %s %s\n", variant, regsrc);
		break;
	case MRP_MT_STATE:
		sprintf(stage, "VMT %s %s\n", variant, regsrc);
		break;
	default:
		break;
	}

	switch (notify) {
	case MRP_NOTIFY_NEW:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VNE %s", stage);
		break;
	case MRP_NOTIFY_JOIN:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VJO %s", stage);
		break;
	case MRP_NOTIFY_LV:
		snprintf(msgbuf, MAX_MRPD_CMDSZ - 1, "VLE %s", stage);
		break;
	default:
		goto free_msgbuf;
		break;
	}

	client = MVRP_db->mrp_db.clients;
	while (NULL != client) {
		send_ctl_msg(&(client->client), msgbuf, MAX_MRPD_CMDSZ);
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

int mvrp_dumptable(struct sockaddr_in *client)
{
	char *msgbuf;
	char *msgbuf_wrptr;
	char *stage;
	char *variant;
	char *regsrc;
	struct mvrp_attribute *attrib;

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

	while (NULL != attrib) {
		sprintf(variant, "%04x", attrib->attribute);
		sprintf(regsrc, "R%02x%02x%02x%02x%02x%02x",
			attrib->registrar.macaddr[0],
			attrib->registrar.macaddr[1],
			attrib->registrar.macaddr[2],
			attrib->registrar.macaddr[3],
			attrib->registrar.macaddr[4],
			attrib->registrar.macaddr[5]);
		switch (attrib->registrar.mrp_state) {
		case MRP_IN_STATE:
			sprintf(stage, "VIN %s %s\n", variant, regsrc);
			break;
		case MRP_LV_STATE:
			sprintf(stage, "VLV %s %s\n", variant, regsrc);
			break;
		case MRP_MT_STATE:
			sprintf(stage, "VMT %s %s\n", variant, regsrc);
			break;
		default:
			break;
		}
		sprintf(msgbuf_wrptr, "%s", stage);
		msgbuf_wrptr += strnlen(stage, 128);
		attrib = attrib->next;
	}

	send_ctl_msg(client, msgbuf, MAX_MRPD_CMDSZ);

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

int recv_mvrp_cmd(char *buf, int buflen, struct sockaddr_in *client)
{
	int rc;
	char respbuf[8];
	struct mvrp_attribute *attrib;
	unsigned int vid_firstval;
	u_int8_t vid_parsestr[8];

	if (NULL == MVRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}

	rc = client_add(&(MVRP_db->mrp_db.clients), client);

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
	switch (buf[1]) {
	case '?':
		mvrp_dumptable(client);
		break;
	case '-':
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		/* buf[] should look similar to 'V--0001' where VID is in hex */
		if (buflen < 7) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}

		memset(vid_parsestr, 0, sizeof(vid_parsestr));

		rc = sscanf((char *)&buf[3], "%04x", &vid_firstval);
		if (0 == rc) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}

		attrib = mvrp_alloc();
		if (NULL == attrib) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;	/* oops - internal error */
		}
		attrib->attribute = vid_firstval;
		memset(attrib->registrar.macaddr, 0, 6);

		mvrp_event(MRP_EVENT_LV, attrib);
		break;
	case '+':
		/* buf[] should look similar to 'V+?0001' where VID is in hex */
		if (('?' != buf[2]) && ('+' != buf[2])) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		if (buflen < 7) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		rc = sscanf((char *)&buf[3], "%04x", &vid_firstval);

		if (0 == rc) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}

		attrib = mvrp_alloc();
		if (NULL == attrib) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;	/* oops - internal error */
		}
		attrib->attribute = vid_firstval;
		memset(attrib->registrar.macaddr, 0, 6);

		if ('?' == buf[2]) {
			mvrp_event(MRP_EVENT_JOIN, attrib);
		} else {
			mvrp_event(MRP_EVENT_NEW, attrib);
		}
		break;
	default:
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
		break;
	}
	return 0;
 out:
	return -1;
}

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

void msrp_increment_streamid(u_int8_t *streamid)
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

int recv_msrp_msg()
{
	char *msgbuf;
	struct sockaddr_ll client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;
	eth_hdr_t *eth;
	mrpdu_t *mrpdu;
	mrpdu_message_t *mrpdu_msg;
	unsigned char *mrpdu_msg_ptr;
	unsigned char *mrpdu_msg_eof;
	mrpdu_vectorattrib_t *mrpdu_vectorptr;
	u_int16_t numvalues;
	u_int16_t numvalues_processed;
	int numvectorbytes;
	u_int8_t vect_3pack;
	u_int8_t vect_4pack;
	int vectidx;
	int vectevt[4];
	int vectevt_idx;
	u_int8_t srclassID_firstval;
	u_int8_t srclassprio_firstval;
	u_int16_t srclassvid_firstval;
	u_int8_t streamid_firstval[8];
	u_int8_t destmac_firstval[6];
	struct msrp_attribute *attrib;
	int endmarks;
	int *listener_vectevt = NULL;
	int listener_vectevt_sz = 0;
	int listener_vectevt_idx;
	int listener_endbyte;

	msgbuf = (char *)malloc(MAX_FRAME_SIZE);
	if (NULL == msgbuf)
		return -1;

	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(msgbuf, 0, MAX_FRAME_SIZE);

	iov.iov_len = MAX_FRAME_SIZE;
	iov.iov_base = msgbuf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	bytes = recvmsg(msrp_socket, &msg, 0);
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
	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;

	mrpdu_msg_eof = (unsigned char *)mrpdu_msg_ptr;
	mrpdu_msg_eof += bytes;
	mrpdu_msg_eof -= sizeof(eth_hdr_t);
	mrpdu_msg_eof -= offsetof(mrpdu_t, MessageList);

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
			mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues) {
					/* skip this null attribute ... some switches generate these ... */
					/* 2 byte numvalues + 4 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (u_int8_t *) mrpdu_vectorptr;
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
				srclassvid_firstval =
				    ((u_int16_t) mrpdu_vectorptr->
				     FirstValue_VectorEvents[2]) << 8 |
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
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
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
			mrpdu_msg_ptr = (u_int8_t *)mrpdu_vectorptr;

			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues) {
					/* 2 byte numvalues + 8 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (u_int8_t *) mrpdu_vectorptr;
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
				       &(mrpdu_vectorptr->
					 FirstValue_VectorEvents[0]), 8);

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
				    (mrpdu_vectorptr->
				     FirstValue_VectorEvents[listener_endbyte])
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
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
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
			mrpdu_msg_ptr = (u_int8_t *)mrpdu_vectorptr;

			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues) {
					/* 2 byte numvalues + 25 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (u_int8_t *) mrpdu_vectorptr;
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
				       &(mrpdu_vectorptr->
					 FirstValue_VectorEvents[8]), 6);

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
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
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
			mrpdu_msg_ptr = (u_int8_t *)mrpdu_vectorptr;
			while (!
			       ((mrpdu_msg_ptr[0] == 0)
				&& (mrpdu_msg_ptr[1] == 0))) {
				numvalues =
				    MRPDU_VECT_NUMVALUES(ntohs
							 (mrpdu_vectorptr->
							  VectorHeader));

				if (0 == numvalues) {
					/* 2 byte numvalues + 34 byte FirstValue + (0) vector bytes */
					mrpdu_msg_ptr =
					    (u_int8_t *) mrpdu_vectorptr;
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
				       &(mrpdu_vectorptr->
					 FirstValue_VectorEvents[8]), 6);

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
				mrpdu_msg_ptr = (u_int8_t *) mrpdu_vectorptr;
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
	u_int16_t numvalues;
	u_int8_t vect_3pack;
	int vectidx;
	int vectevt[3];
	int vectevt_idx;
	u_int8_t streamid_firstval[8];
	u_int8_t destmac_firstval[6];
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
		    attrib->attribute.talk_listen.DataFrameParameters.Vlan_ID;

		mrpdu_vectorptr->FirstValue_VectorEvents[16] =
		    attrib->attribute.talk_listen.TSpec.MaxFrameSize >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[17] =
		    attrib->attribute.talk_listen.TSpec.MaxFrameSize;

		mrpdu_vectorptr->FirstValue_VectorEvents[18] =
		    attrib->attribute.talk_listen.TSpec.MaxIntervalFrames >> 8;
		mrpdu_vectorptr->FirstValue_VectorEvents[19] =
		    attrib->attribute.talk_listen.TSpec.MaxIntervalFrames;

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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_3pack;
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
	mrpdu_msg->Data[0] = (u_int8_t) (attriblistlen >> 8);
	mrpdu_msg->Data[1] = (u_int8_t) attriblistlen;
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
	u_int16_t numvalues;
	u_int8_t vect_3pack;
	u_int8_t vect_4pack;
	int vectidx;
	int attriblistlen;
	int vectevt[4];
	int vectevt_idx;
	int *listen_declare = NULL;
	int listen_declare_sz = 64;
	int listen_declare_idx = 0;
	int listen_declare_end = 0;
	u_int8_t streamid_firstval[8];
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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_3pack;
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

				mrpdu_vectorptr->
				    FirstValue_VectorEvents[vectidx] =
				    vect_4pack;
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
	mrpdu_msg->Data[0] = (u_int8_t) (attriblistlen >> 8);
	mrpdu_msg->Data[1] = (u_int8_t) attriblistlen;

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
	mrpdu_msg_ptr = (unsigned char *)mrpdu->MessageList;
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

	if (mrpdu_msg_ptr == (unsigned char *)mrpdu->MessageList) {
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

	bytes = send(msrp_socket, msgbuf, msgbuf_len, 0);
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
		send_ctl_msg(&(client->client), msgbuf, MAX_MRPD_CMDSZ);
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
	char	mrp_state[8];

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
			attrib->registrar.macaddr[5],
			mrp_state);

		sprintf(stage, "%s %s\n", variant, regsrc);

		sprintf(msgbuf_wrptr, "%s", stage);
		msgbuf_wrptr += strlen(stage);
		attrib = attrib->next;
	}

	send_ctl_msg(client, msgbuf, MAX_MRPD_CMDSZ);

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

int recv_msrp_cmd(char *buf, int buflen, struct sockaddr_in *client)
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
	u_int8_t streamid_firstval[8];
	u_int8_t macvec_firstval[6];
	u_int8_t macvec_parsestr[64];
	unsigned vlan, size, interval, prio, latency;
	int i;

	if (NULL == MSRP_db) {
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
	}

	rc = client_add(&(MSRP_db->mrp_db.clients), client);

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
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[2]) {
		case 'l':
		case 'L':
			if (buflen < 22) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S-L:xxyyzz...' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
					send_ctl_msg(client, respbuf,
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S-D:C:%d:P:%d:V:%04x"*/
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_DOMAIN_TYPE;

			i = 6;
			rc = sscanf((char *)&(buf[6]), "%d",
				&prio);

			attrib->attribute.domain.SRclassID = (uint8_t)prio;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}
			while ((i < buflen) && (buf[i] != 'P'))
				i++;

			i+=2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%d",
				&prio);

			attrib->attribute.domain.SRclassPriority = (u_int16_t)prio;
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			while ((i < buflen) && (buf[i] != 'V'))
				i++;

			i+=2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%04x",
				&vlan);
			attrib->attribute.domain.SRclassVID = (uint8_t)vlan;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S--S:xxyyzz...' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
					send_ctl_msg(client, respbuf,
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
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	case '+':
		/* parse the type - either talker or listener */
		if (buflen < 5) {
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		switch (buf[2]) {
		case 'l':
		case 'L':
			if (buflen < 26) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S+L:xxyyzz...:D:a' */
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
					send_ctl_msg(client, respbuf,
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			memset(attrib->registrar.macaddr, 0, 6);

			msrp_event(MRP_EVENT_JOIN, attrib);
			break;
		case 'D':
			if (buflen < 18) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}
			/* buf[] should look similar to 'S+D:C:%d:P:%d:V:%04x"*/
			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;	/* oops - internal error */
			}
			attrib->type = MSRP_DOMAIN_TYPE;

			i = 6;
			rc = sscanf((char *)&(buf[6]), "%d",
				&prio);

			attrib->attribute.domain.SRclassID = (uint8_t)prio;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}
			while ((i < buflen) && (buf[i] != 'P'))
				i++;

			i+=2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%d",
				&prio);

			attrib->attribute.domain.SRclassPriority = (u_int16_t)prio;
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			while ((i < buflen) && (buf[i] != 'V'))
				i++;

			i+=2;
			if (i >= buflen) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
					     sizeof(respbuf));
				free(attrib);
				goto out;
			}

			rc = sscanf((char *)&(buf[i]), "%04x",
				&vlan);
			attrib->attribute.domain.SRclassVID = (uint8_t)vlan;

			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1,
					 "ERP %s", buf);
				send_ctl_msg(client, respbuf,
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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
					send_ctl_msg(client, respbuf,
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
					send_ctl_msg(client, respbuf,
						     sizeof(respbuf));
					goto out;
				}
			}

			rc = sscanf((char *)vlan_str, "%x", &vlan);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)size_str, "%d", &size);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)interval_str, "%d", &interval);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)prio_str, "%d", &prio);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			rc = sscanf((char *)latency_str, "%d", &latency);
			if (0 == rc) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
				goto out;
			}

			attrib = msrp_alloc();
			if (NULL == attrib) {
				snprintf(respbuf, sizeof(respbuf) - 1, "ERI %s",
					 buf);
				send_ctl_msg(client, respbuf, sizeof(respbuf));
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

			if (join)
				msrp_event(MRP_EVENT_JOIN, attrib);
			else
				msrp_event(MRP_EVENT_NEW, attrib);
			break;
		default:
			snprintf(respbuf, sizeof(respbuf) - 1, "ERP %s", buf);
			send_ctl_msg(client, respbuf, sizeof(respbuf));
			goto out;
		}
		break;
	default:
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		goto out;
		break;
	}
	return 0;
 out:
	return -1;
}

int init_local_ctl(void)
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	int sock_fd = -1;
	int rc;

	sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock_fd < 0)
		goto out;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(mrpd_port);
	inet_aton("127.0.0.1", (struct in_addr *)&addr.sin_addr.s_addr);
	addr_len = sizeof(addr);

	rc = bind(sock_fd, (struct sockaddr *)&addr, addr_len);

	if (rc < 0)
		goto out;

	control_socket = sock_fd;

	return 0;
 out:
	if (sock_fd != -1)
		close(sock_fd);

	return -1;
}

int
send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data, int notify_len)
{

	int rc;

	if (-1 == control_socket)
		return 0;

	rc = sendto(control_socket, notify_data, notify_len,
		    0, (struct sockaddr *)client_addr, sizeof(struct sockaddr));
	return rc;
}

int process_ctl_msg(char *buf, int buflen, struct sockaddr_in *client)
{

	char respbuf[8];
	/*
	 * Inbound/output commands from/to a client:
	 *
	 * When sent from a client, indicates an operation on the
	 * internal attribute databases. When sent by the daemon to
	 * a client, indicates an event such as a new attribute arrival,
	 * or a leaveall timer to force clients to re-advertise continued
	 * interest in an attribute.
	 *
	 * BYE   Client detaches from daemon
	 *
	 * M+? - JOIN_MT a MAC address or service declaration
	 * M++   JOIN_IN a MAC Address (XXX: MMRP doesn't use 'New' though?)
	 * M-- - LV a MAC address or service declaration
	 * V+? - JOIN_MT a VID (VLAN ID)
	 * V++ - JOIN_IN a VID (VLAN ID)
	 * V-- - LV a VID (VLAN ID)
	 * S+? - JOIN_MT a Stream
	 * S++ - JOIN_IN a Stream
	 * S-- - LV a Stream
	 *
	 * Outbound messages
	 * ERC - error, unrecognized command
	 * ERP - error, unrecognized parameter
	 * ERI - error, internal
	 * OK+ - success
	 *
	 * Registrar Commands
	 *
	 * M?? - query MMRP Registrar MAC Address database
	 * V?? - query MVRP Registrar VID database
	 * S?? - query MSRP Registrar database
	 *
	 * Registrar Responses (to ?? commands)
	 *
	 * MIN - Registered
	 * MMT - Registered, Empty
	 * MLV - Registered, Leaving
	 * MNE - New attribute notification
	 * MJO - JOIN attribute notification
	 * MLV - LEAVE attribute notification
	 * VIN - Registered
	 * VMT - Registered, Empty
	 * VLV - Registered, Leaving
	 * SIN - Registered
	 * SMT - Registered, Empty
	 * SLV - Registered, Leaving
	 *
	 */

	memset(respbuf, 0, sizeof(respbuf));

	/* XXX */
	/* printf("CMD:%s from CLNT %d\n", buf, client->sin_port); */

	if (buflen < 3) {
		printf("buflen = %d!\b", buflen);

		return -1;
	}

	switch (buf[0]) {
	case 'M':
		return recv_mmrp_cmd(buf, buflen, client);
		break;
	case 'V':
		return recv_mvrp_cmd(buf, buflen, client);
		break;
	case 'S':
		return recv_msrp_cmd(buf, buflen, client);
		break;
	case 'B':
		if (NULL != MMRP_db)
			client_delete(&(MMRP_db->mrp_db.clients), client);
		if (NULL != MVRP_db)
			client_delete(&(MVRP_db->mrp_db.clients), client);
		if (NULL != MSRP_db)
			client_delete(&(MSRP_db->mrp_db.clients), client);
		break;
	default:
		printf("unrecognized command %s\n", buf);
		snprintf(respbuf, sizeof(respbuf) - 1, "ERC %s", buf);
		send_ctl_msg(client, respbuf, sizeof(respbuf));
		return -1;
		break;
	}

	return 0;
}

int recv_ctl_msg()
{
	char *msgbuf;
	struct sockaddr_in client_addr;
	struct msghdr msg;
	struct iovec iov;
	int bytes = 0;

	msgbuf = (char *)malloc(MAX_MRPD_CMDSZ);
	if (NULL == msgbuf)
		return -1;

	memset(&msg, 0, sizeof(msg));
	memset(&client_addr, 0, sizeof(client_addr));
	memset(msgbuf, 0, MAX_MRPD_CMDSZ);

	iov.iov_len = MAX_MRPD_CMDSZ;
	iov.iov_base = msgbuf;
	msg.msg_name = &client_addr;
	msg.msg_namelen = sizeof(client_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	bytes = recvmsg(control_socket, &msg, 0);
	if (bytes <= 0)
		goto out;

	process_ctl_msg(msgbuf, bytes, &client_addr);
 out:
	free(msgbuf);

	return -1;
}

int
init_protocol_socket(u_int16_t etype, int *sock, unsigned char *multicast_addr)
{
	struct sockaddr_ll addr;
	struct ifreq if_request;
	int lsock;
	int rc;
	struct packet_mreq multicast_req;

	if (NULL == sock)
		return -1;
	if (NULL == multicast_addr)
		return -1;

	*sock = -1;

	lsock = socket(PF_PACKET, SOCK_RAW, htons(etype));
	if (lsock < 0)
		return -1;

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name));

	rc = ioctl(lsock, SIOCGIFHWADDR, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memcpy(STATION_ADDR, if_request.ifr_hwaddr.sa_data, sizeof(STATION_ADDR));

	memset(&if_request, 0, sizeof(if_request));

	strncpy(if_request.ifr_name, interface, sizeof(if_request.ifr_name));

	rc = ioctl(lsock, SIOCGIFINDEX, &if_request);
	if (rc < 0) {
		close(lsock);
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sll_ifindex = if_request.ifr_ifindex;
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = htons(etype);

	rc = bind(lsock, (struct sockaddr *)&addr, sizeof(addr));
	if (0 != rc) {
		close(lsock);
		return -1;
	}

	rc = setsockopt(lsock, SOL_SOCKET, SO_BINDTODEVICE, interface,
			strlen(interface));
	if (0 != rc) {
		close(lsock);
		return -1;
	}

	multicast_req.mr_ifindex = if_request.ifr_ifindex;
	multicast_req.mr_type = PACKET_MR_MULTICAST;
	multicast_req.mr_alen = 6;
	memcpy(multicast_req.mr_address, multicast_addr, 6);

	rc = setsockopt(lsock, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
			&multicast_req, sizeof(multicast_req));
	if (0 != rc) {
		close(lsock);
		return -1;
	}

	*sock = lsock;

	return 0;
}

int init_mrp_timers(struct mrp_database *mrp_db)
{
	mrp_db->join_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	fcntl(mrp_db->join_timer, F_SETFL, O_NONBLOCK);  
	mrp_db->lv_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	fcntl(mrp_db->lv_timer, F_SETFL, O_NONBLOCK);  
	mrp_db->lva_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	fcntl(mrp_db->lva_timer, F_SETFL, O_NONBLOCK);  

	if (-1 == mrp_db->join_timer)
		goto out;
	if (-1 == mrp_db->lv_timer)
		goto out;
	if (-1 == mrp_db->lva_timer)
		goto out;
	return 0;
 out:
	if (-1 != mrp_db->join_timer)
		close(mrp_db->join_timer);
	if (-1 != mrp_db->lv_timer)
		close(mrp_db->lv_timer);
	if (-1 != mrp_db->lva_timer)
		close(mrp_db->lva_timer);

	return -1;
}

int init_mmrp(int mmrp_enable)
{
	int rc;

	/* XXX doesn't handle re-start */

	mmrp_socket = -1;
	MMRP_db = NULL;

	if (0 == mmrp_enable) {
		return 0;
	}

	rc = init_protocol_socket(MMRP_ETYPE, &mmrp_socket, MMRP_ADDR);
	if (rc < 0)
		return -1;

	MMRP_db = malloc(sizeof(struct mmrp_database));

	if (NULL == MMRP_db)
		goto abort_socket;

	memset(MMRP_db, 0, sizeof(struct mmrp_database));

	/* if registration is FIXED or FORBIDDEN
	 * updates from MRP are discarded, and
	 * only IN and JOININ messages are sent
	 */
	MMRP_db->mrp_db.registration = MRP_REGISTRAR_CTL_NORMAL;	/* default */

	/* if participant role is 'SILENT' (or non-participant)
	 * applicant doesn't send any messages -
	 *
	 * Note - theoretically configured per-attribute
	 */
	MMRP_db->mrp_db.participant = MRP_APPLICANT_CTL_NORMAL;	/* default */

	rc = init_mrp_timers(&(MMRP_db->mrp_db));

	if (rc < 0)
		goto abort_alloc;

	mrp_lvatimer_fsm(&(MMRP_db->mrp_db), MRP_EVENT_BEGIN);
	return 0;

 abort_alloc:
	/* free MMRP_db and related structures */
	free(MMRP_db);
	MMRP_db = NULL;
 abort_socket:
	close(mvrp_socket);
	mvrp_socket = -1;
	/* XXX */
	return -1;

	return 0;
}

int init_mvrp(int mvrp_enable)
{
	int rc;

	/* XXX doesn't handle re-start */

	mvrp_socket = -1;
	MVRP_db = NULL;

	if (0 == mvrp_enable) {
		return 0;
	}

	rc = init_protocol_socket(MVRP_ETYPE, &mvrp_socket,
				  MVRP_CUSTOMER_BRIDGE_ADDR);
	if (rc < 0)
		return -1;

	MVRP_db = malloc(sizeof(struct mvrp_database));

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

	rc = init_mrp_timers(&(MVRP_db->mrp_db));

	if (rc < 0)
		goto abort_alloc;

	mrp_lvatimer_fsm(&(MVRP_db->mrp_db), MRP_EVENT_BEGIN);
	return 0;

 abort_alloc:
	/* free MVRP_db and related structures */
	free(MVRP_db);
	MVRP_db = NULL;
 abort_socket:
	close(mvrp_socket);
	mvrp_socket = -1;
	/* XXX */
	return -1;
}

int init_msrp(int msrp_enable)
{
	int rc;

	/* XXX doesn't handle re-start */

	msrp_socket = -1;
	MSRP_db = NULL;

	if (0 == msrp_enable) {
		return 0;
	}

	rc = init_protocol_socket(MSRP_ETYPE, &msrp_socket, MSRP_ADDR);
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

	rc = init_mrp_timers(&(MSRP_db->mrp_db));

	if (rc < 0)
		goto abort_alloc;

	mrp_lvatimer_fsm(&(MSRP_db->mrp_db), MRP_EVENT_BEGIN);

	return 0;

 abort_alloc:
	/* free MSRP_db and related structures */
	free(MSRP_db);
	MSRP_db = NULL;
 abort_socket:
	close(msrp_socket);
	msrp_socket = -1;
	/* XXX */
	return -1;
}

int handle_periodic(void)
{
	if (periodic_enable)
		periodictimer_start();
	else
		periodictimer_stop();

	return 0;
}

int init_timers(void)
{
	/*
	 * primarily whether to schedule the periodic timer as the
	 * rest are self-scheduling as a side-effect of state transitions
	 * of the various attributes
	 */

	periodic_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	fcntl(periodic_timer, F_SETFL, O_NONBLOCK);  
	gc_timer = timerfd_create(CLOCK_MONOTONIC, 0);
	fcntl(gc_timer, F_SETFL, O_NONBLOCK);  

	if (-1 == periodic_timer)
		goto out;
	if (-1 == gc_timer)
		goto out;

	gctimer_start();

	if (periodic_enable)
		periodictimer_start();

	return 0;
 out:
	return -1;
}

int register_mrp_timers(struct mrp_database *mrp_db, fd_set *fds)
{
	int max_fd;

	FD_SET(mrp_db->join_timer, fds);
	FD_SET(mrp_db->lv_timer, fds);
	FD_SET(mrp_db->lva_timer, fds);

	max_fd = mrp_db->join_timer;
	if (mrp_db->lv_timer > max_fd)
		max_fd = mrp_db->lv_timer;
	if (mrp_db->lva_timer > max_fd)
		max_fd = mrp_db->lva_timer;

	return max_fd;
}

int mrpd_reclaim()
{
	struct mmrp_attribute *mattrib, *free_mattrib;
	struct mvrp_attribute *vattrib, *free_vattrib;
	struct msrp_attribute *sattrib, *free_sattrib;

	/*
	 * if the local applications have neither registered interest
	 * by joining, and the remote node has quit advertising the attribute
	 * and allowing it to go into the MT state, delete the attribute 
	 */

	if (NULL != MMRP_db) {
		mattrib = MMRP_db->attrib_list;
		while (NULL != mattrib) {
			if ((mattrib->registrar.mrp_state == MRP_MT_STATE) && \
			((mattrib->applicant.mrp_state == MRP_VO_STATE) || \
			    (mattrib->applicant.mrp_state == MRP_AO_STATE) || \
				(mattrib->applicant.mrp_state == MRP_QO_STATE)))
			{
				if (NULL != mattrib->prev)
					mattrib->prev->next = mattrib->next;
				else
					MMRP_db->attrib_list = mattrib->next;
				if (NULL != mattrib->next)
					mattrib->next->prev = mattrib->prev;
				free_mattrib = mattrib;
				mattrib = mattrib->next;
				mmrp_send_notifications(free_mattrib, MRP_NOTIFY_LV);
				free(free_mattrib);
			} else
				mattrib = mattrib->next;
		}
	}
	if (NULL != MVRP_db) {
		vattrib = MVRP_db->attrib_list;
		while (NULL != vattrib) {
			if ((vattrib->registrar.mrp_state == MRP_MT_STATE) && \
			((vattrib->applicant.mrp_state == MRP_VO_STATE) || \
			    (vattrib->applicant.mrp_state == MRP_AO_STATE) || \
				(vattrib->applicant.mrp_state == MRP_QO_STATE)))
			{
				if (NULL != vattrib->prev)
					vattrib->prev->next = vattrib->next;
				else
					MVRP_db->attrib_list = vattrib->next;
				if (NULL != vattrib->next)
					vattrib->next->prev = vattrib->prev;
				free_vattrib = vattrib;
				vattrib = vattrib->next;
				mvrp_send_notifications(free_vattrib, MRP_NOTIFY_LV);
				free(free_vattrib);
			} else
				vattrib = vattrib->next;
		}
	}
	if (NULL != MSRP_db) {
		sattrib = MSRP_db->attrib_list;
		while (NULL != sattrib) {
			if ((sattrib->registrar.mrp_state == MRP_MT_STATE) && \
			((sattrib->applicant.mrp_state == MRP_VO_STATE) || \
			    (sattrib->applicant.mrp_state == MRP_AO_STATE) || \
				(sattrib->applicant.mrp_state == MRP_QO_STATE)))
			{
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
	}

	gctimer_start();

	return 0;

}

void process_events(void)
{

	fd_set fds, sel_fds;
	int rc;
	int max_fd;

	/* wait for events, demux the received packets, process packets */

	FD_ZERO(&fds);
	FD_SET(control_socket, &fds);

	max_fd = control_socket;

	if (mmrp_enable) {
		FD_SET(mmrp_socket, &fds);
		if (mmrp_socket > max_fd)
			max_fd = mmrp_socket;

		if (NULL == MMRP_db)
			return;

		rc = register_mrp_timers(&(MMRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;
	}
	if (mvrp_enable) {
		FD_SET(mvrp_socket, &fds);
		if (mvrp_socket > max_fd)
			max_fd = mvrp_socket;

		if (NULL == MVRP_db)
			return;
		rc = register_mrp_timers(&(MVRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;

	}
	if (msrp_enable) {
		FD_SET(msrp_socket, &fds);
		if (msrp_socket > max_fd)
			max_fd = msrp_socket;

		if (NULL == MSRP_db)
			return;
		rc = register_mrp_timers(&(MSRP_db->mrp_db), &fds);
		if (rc > max_fd)
			max_fd = rc;

	}

	FD_SET(periodic_timer, &fds);
	if (periodic_timer > max_fd)
		max_fd = periodic_timer;

	FD_SET(gc_timer, &fds);
	if (gc_timer > max_fd)
		max_fd = gc_timer;

	do {

		sel_fds = fds;
		rc = select(max_fd + 1, &sel_fds, NULL, NULL, NULL);

		if (-1 == rc)
			return;	/* exit on error */
		else {
			if (FD_ISSET(control_socket, &sel_fds))
				recv_ctl_msg();
			if (mmrp_enable) {
				if FD_ISSET
					(mmrp_socket, &sel_fds) recv_mmrp_msg();
				if FD_ISSET
					(MMRP_db->mrp_db.lva_timer, &sel_fds) {
					mmrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MMRP_db->mrp_db.lv_timer, &sel_fds) {
					mmrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MMRP_db->mrp_db.join_timer, &sel_fds) {
					mmrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (mvrp_enable) {
				if FD_ISSET
					(mvrp_socket, &sel_fds) recv_mvrp_msg();
				if FD_ISSET
					(MVRP_db->mrp_db.lva_timer, &sel_fds) {
					mvrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MVRP_db->mrp_db.lv_timer, &sel_fds) {
					mvrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MVRP_db->mrp_db.join_timer, &sel_fds) {
					mvrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (msrp_enable) {
				if FD_ISSET
					(msrp_socket, &sel_fds) recv_msrp_msg();
				if FD_ISSET
					(MSRP_db->mrp_db.lva_timer, &sel_fds) {
					msrp_event(MRP_EVENT_LVATIMER, NULL);
					}
				if FD_ISSET
					(MSRP_db->mrp_db.lv_timer, &sel_fds) {
					msrp_event(MRP_EVENT_LVTIMER, NULL);
					}
				if FD_ISSET
					(MSRP_db->mrp_db.join_timer, &sel_fds) {
					msrp_event(MRP_EVENT_TX, NULL);
					}
			}
			if (FD_ISSET(periodic_timer, &sel_fds)) {
				if (mmrp_enable) {
					mmrp_event(MRP_EVENT_PERIODIC, NULL);
				}
				if (mvrp_enable) {
					mvrp_event(MRP_EVENT_PERIODIC, NULL);
				}
				if (msrp_enable) {
					msrp_event(MRP_EVENT_PERIODIC, NULL);
				}
				handle_periodic();
			}
			if (FD_ISSET(gc_timer, &sel_fds)) {
				mrpd_reclaim();
			}
		}
	} while (1);
}

void usage(void)
{
	fprintf(stderr,
		"\n"
		"usage: mrpd [-hdlmvsp] -i interface-name"
		"\n"
		"options:\n"
		"    -h  show this message\n"
		"    -d  run daemon in the background\n"
		"    -l  enable logging (ignored in daemon mode)\n"
		"    -p  enable periodic timer\n"
		"    -m  enable MMRP Registrar and Participant\n"
		"    -v  enable MVRP Registrar and Participant\n"
		"    -s  enable MSRP Registrar and Participant\n"
		"    -i  specify interface to monitor\n"
		"\n" "%s" "\n", version_str);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	int rc = 0;

	daemonize = 0;
	mmrp_enable = 0;
	mvrp_enable = 0;
	msrp_enable = 0;
	logging_enable = 0;
	mrpd_port = MRPD_PORT_DEFAULT;
	interface = NULL;
	interface_fd = -1;
	p2pmac = 0;		/* operPointToPointMAC false by default */
	periodic_enable = 0;
	registration = MRP_REGISTRAR_CTL_NORMAL;	/* default */
	participant = MRP_APPLICANT_CTL_NORMAL;	/* default */
	control_socket = -1;
	mmrp_socket = -1;
	mvrp_socket = -1;
	msrp_socket = -1;
	periodic_timer = -1;
	gc_timer = -1;

	MMRP_db = NULL;
	MVRP_db = NULL;
	MSRP_db = NULL;

	lva_next = MRP_LVATIMER_VAL;	/* leaveall timeout in msec */
	for (;;) {
		c = getopt(argc, argv, "hdlmvspi:");

		if (c < 0)
			break;

		switch (c) {
		case 'm':
			mmrp_enable = 1;
			break;
		case 'v':
			mvrp_enable = 1;
			break;
		case 's':
			msrp_enable = 1;
			break;
		case 'l':
			logging_enable = 1;
			break;
		case 'd':
			daemonize = 1;
			break;
		case 'p':
			periodic_enable = 1;
			break;
		case 'i':
			if (interface) {
				printf("only one interface per daemon is supported\n");
				usage();
			}
			interface = strdup(optarg);
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	if (optind < argc)
		usage();

	if (NULL == interface)
		usage();

	if (!mmrp_enable && !mvrp_enable && !msrp_enable)
		usage();

	/* daemonize before we start creating file descriptors */

	if (daemonize) {
		rc = daemon(1, 0);
		if (rc)
			goto out;
	}

	rc = init_local_ctl();
	if (rc)
		goto out;

	rc = init_mmrp(mmrp_enable);
	if (rc)
		goto out;

	rc = init_mvrp(mvrp_enable);
	if (rc)
		goto out;

	rc = init_msrp(msrp_enable);
	if (rc)
		goto out;

	rc = init_timers();
	if (rc)
		goto out;

	process_events();
 out:
	return rc;

}
