/*
Copyright (c) 2014, Jeff Koftinoff
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the {organization} nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "adp_adv.h"

/** Form the entity available message and send it */
static void adp_adv_send_entity_available(struct adp_adv *self);

/** Form the entity departing message and send it */
static void adp_adv_send_entity_departing(struct adp_adv *self);

/** Form the entity discover message and send it */
static void adp_adv_send_entity_discover(struct adp_adv *self);

int adp_adv_init(struct adp_adv *self, void *context,
		 void (*frame_send)(struct adp_adv *self, void *context,
				    uint8_t const *buf, uint16_t len),
		 void (*received_entity_available_or_departing)(
		     struct adp_adv *self, void *context,
		     void const *source_address, int source_address_len,
		     struct jdksavdecc_adpdu *adpdu))
{
	self->last_time_in_microseconds = 0;
	self->do_send_entity_available = 1;
	self->do_send_entity_departing = 0;
	self->do_send_entity_discover = 0;
	self->received_entity_available_or_departing =
	    received_entity_available_or_departing;

	memset(&self->adpdu, 0, sizeof(self->adpdu));
	self->adpdu.header.cd = true;
	self->adpdu.header.subtype = JDKSAVDECC_SUBTYPE_ADP;
	self->adpdu.header.version = 0;
	self->adpdu.header.sv = 0;
	self->adpdu.header.message_type =
	    JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE;
	self->adpdu.header.valid_time = 10;
	self->adpdu.header.control_data_length =
	    JDKSAVDECC_ADPDU_LEN - JDKSAVDECC_COMMON_CONTROL_HEADER_LEN;

	/* todo: fill in rest of adpdu */

	return 1;
}

void adp_adv_terminate(struct adp_adv *self) { (void)self; }

int adp_adv_receive(struct adp_adv *self,
		    jdksavdecc_timestamp_in_microseconds time_in_microseconds,
		    void const *source_address, int source_address_len,
		    uint8_t const *buf, uint16_t len)
{
	struct jdksavdecc_adpdu incoming;
	int r = 0;
	(void)time_in_microseconds;
	if (jdksavdecc_adpdu_read(&incoming, buf, 0, len) > 0) {
		r = 1;
		switch (incoming.header.message_type) {
		case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DISCOVER:
			/* only respond to discover messages if we are not
			 * stopped
			 */
			if (!self->stopped) {
				/* handle the case where the discover message
				 * references our entity id or 0
				 */
				if (jdksavdecc_eui64_compare(
					&incoming.header.entity_id,
					&self->adpdu.header.entity_id) ||
				    jdksavdecc_eui64_convert_to_uint64(
					&incoming.header.entity_id) == 0) {
					self->do_send_entity_available = 1;
					self->early_tick = 1;
				}
			}
			break;
		case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE:
			/* only handle incoming available messages if we have a
			 * place to give them to
			 */
			if (self->received_entity_available_or_departing) {
				self->received_entity_available_or_departing(
				    self, self->context, source_address,
				    source_address_len, &incoming);
			}
			break;
		case JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DEPARTING:
			/* only handle incoming departing messages if we have a
			 * place to give them to
			 */
			if (self->received_entity_available_or_departing) {
				self->received_entity_available_or_departing(
				    self, self->context, source_address,
				    source_address_len, &incoming);
			}
			break;
		default:
			break;
		}
	}
	return r;
}

void adp_adv_tick(struct adp_adv *self,
		  jdksavdecc_timestamp_in_microseconds cur_time_in_micros)
{

	/* calculate the time since the last send */
	jdksavdecc_timestamp_in_microseconds difftime =
	    cur_time_in_micros - self->last_time_in_microseconds;

	jdksavdecc_timestamp_in_microseconds valid_time_in_ms =
	    self->adpdu.header.valid_time;

	/*
	 * calculate the time in microseconds between sends.
	 * header.valid_time is in 2 second increments. We are to send
	 * 4 available messages per valid_time.
	 */
	valid_time_in_ms = (valid_time_in_ms * 1000) / 2;

	/* limit it to be at most one send per second */
	if (valid_time_in_ms < 1000) {
		valid_time_in_ms = 1000;
	}

	/* clear any early tick flag */
	self->early_tick = 0;

	/* only send messages available/departing messages if we are not stopped */

	/* are we departing? */
	if (self->do_send_entity_departing) {

		/* yes, we are sending an entity departing message.
		 * clear any do_send flags
		 */

		self->do_send_entity_departing = 0;
		self->do_send_entity_available = 0;

		/* change into pause state */

		self->stopped = 1;

		/* record the time we send it */

		self->last_time_in_microseconds = cur_time_in_micros;

		/* send the departing */

		adp_adv_send_entity_departing(self);

		/* reset the available_index to 0 */

		self->adpdu.available_index = 0;
	}

	if (!self->stopped) {
		/* if we are running and it is time to send an available, set the flag */
		if (difftime > valid_time_in_ms) {
			self->do_send_entity_available = 1;
		}

		/* if the flag is set for whatever reason and we are running then send the available and clear the flag */
		if (self->do_send_entity_available) {
			/* we are to send entity available message. clear the request flag */
			self->do_send_entity_available = 0;

			/* record the time we send it */
			self->last_time_in_microseconds = cur_time_in_micros;

			/* send the available */
			adp_adv_send_entity_available(self);
		}
	}

	/* are we asked to send an entity discover message? */

	if (self->do_send_entity_discover) {

		/* yes, clear the flag and send it */

		self->do_send_entity_discover = 0;
		adp_adv_send_entity_discover(self);
	}
}

void adp_adv_trigger_send_discover(struct adp_adv *self)
{

	self->early_tick = 1;
	self->do_send_entity_discover = 1;
}

void adp_adv_trigger_send_available(struct adp_adv *self)
{
	self->early_tick = 1;
	self->do_send_entity_available = 1;
	self->stopped = 0;
}

void adp_adv_trigger_send_departing(struct adp_adv *self)
{
	self->early_tick = 1;
	self->do_send_entity_departing = 1;
}

static void adp_adv_send_entity_available(struct adp_adv *self)
{
	struct jdksavdecc_frame f;
	self->adpdu.header.message_type =
	    JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE;
	f.length = jdksavdecc_adpdu_write(&self->adpdu, &f.payload, 0,
					  sizeof(f.payload));

	if (f.length > 0) {
		self->frame_send->send(self->frame_send, &f);
		self->adpdu.available_index++;
	}
}

static void adp_adv_send_entity_departing(struct adp_adv *self)
{
	struct jdksavdecc_frame f;
	self->adpdu.header.message_type =
	    JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DEPARTING;
	f.length = jdksavdecc_adpdu_write(&self->adpdu, &f.payload, 0,
					  sizeof(f.payload));

	if (f.length > 0) {
		self->frame_send->send(self->frame_send, &f);
		self->adpdu.available_index = 0;
	}
}

static void adp_adv_send_entity_discover(struct adp_adv *self)
{
	struct jdksavdecc_frame f;
	struct jdksavdecc_adpdu adpdu;
	memset(&adpdu, 0, sizeof(adpdu));
	adpdu.header.cd = true;
	adpdu.header.subtype = JDKSAVDECC_SUBTYPE_ADP;
	adpdu.header.control_data_length =
	    JDKSAVDECC_ADPDU_LEN - JDKSAVDECC_COMMON_CONTROL_HEADER_LEN;
	adpdu.header.message_type = JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DISCOVER;
	adpdu.header.sv = 0;
	adpdu.header.version = 0;
	adpdu.header.valid_time = 0;
	jdksavdecc_eui64_init_from_uint64(&adpdu.header.entity_id, 0);

	f.length = jdksavdecc_adpdu_write(&self->adpdu, &f.payload, 0,
					  sizeof(f.payload));

	if (f.length > 0) {
		self->frame_send->send(self->frame_send, &f);
		self->adpdu.available_index++;
	}
}
