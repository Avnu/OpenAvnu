/*
Copyright (c) 2015, Jeff Koftinoff
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

#ifndef ACMP_LISTENER_H_
#define ACMP_LISTENER_H_

#include "acmpdu.h"
#include "srp_info.h"

#ifndef ACMP_LISTENER_MAX_STREAMS
#define ACMP_LISTENER_MAX_STREAMS ( 8 )
#endif

struct acmp_listener_slots;

/** These are the signals that can come from an acmp_listener state machine */
struct acmp_listener_signals {

    /** The signals were connected */
	void (*acmp_listener_connected) (struct acmp_listener_signals * self,
					 struct acmp_listener_slots *
					 acmp_controller_slots);

    /** The signals were disconnected */
	void (*acmp_listener_disconnected) (struct acmp_listener_signals *
					    self);

    /** The object was started */
	void (*acmp_listener_started) (struct acmp_listener_signals * self,
				       struct acmp_listener_slots * source);

    /** The object was stopped */
	void (*acmp_listener_stopped) (struct acmp_listener_signals * self,
				       struct acmp_listener_slots * source);

    /** The object is asking to send an ethernet frame */
	void (*acmp_listener_send_pdu) (void *self, struct frame * acmpdu);

    /** The object is asking to be notified about the specified talker stream id  */
	void (*acmp_listener_srp_info_request) (struct acmp_listener_signals *
						self,
						struct acmp_listener_slots *
						source,
						uint64_t talker_stream_id);

    /** The object is listening to a talker */
	void (*acmp_listener_talker_added) (struct acmp_listener_signals * self,
					    struct acmp_listener_slots * source,
					    uint64_t talker_stream_id,
					    uint64_t talker_mac,
					    uint16_t talker_vid);

    /** The object is no longer listening to a talker */
	void (*acmp_listener_talker_removed) (struct acmp_listener_signals *
					      self,
					      struct acmp_listener_slots *
					      source,
					      uint64_t talker_stream_id);
};

/** These are the the messages that an acmp listener state machine can receive */
struct acmp_listener_slots {
    /** Ask the object to to terminate */
	void (*terminate) (struct acmp_listener_slots * self);

    /** Ask the object to send its signals to destination_signals */
	void (*connect_signals) (struct acmp_listener_slots * self,
				 struct acmp_listener_signals *
				 destination_signals);

    /** Ask the object to disconnect the destination_signals */
	void (*disconnect_signals) (struct acmp_listener_slots * self,
				    struct acmp_listener_signals *
				    destination_signals);

    /** Ask the object to start processing */
	void (*start) (struct acmp_listener_slots * self);

    /** Ask the object to stop processing */
	void (*stop) (struct acmp_listener_slots * self);

    /** Ask the object to handle the specified ethernet frame */
	void (*handle_pdu) (struct acmp_listener_slots * self,
			    struct frame * frame);

    /** Ask the object to process any periodic timers */
	void (*tick) (struct acmp_listener_slots * self,
		      timestamp_in_microseconds current_time);

    /** Notify the ACMP listener that some talker stream info was received via SRP */
	void (*srp_talker_info_received) (struct acmp_listener_slots * self,
                      const struct srp_info_first_value_talker *
					  stream_info);
};

/** \addtogroup acmp_talker ACMP Listener State Machine */
/*@{*/

struct acmp_listener_context {
	enum {
		ACMP_LISTENER_STATE_DISABLED = 0,
		ACMP_LISTENER_STATE_DISCONNECTED,
		ACMP_LISTENER_STATE_FAST_CONNECTION_IN_PROGRESS,
		ACMP_LISTENER_STATE_CONNECTION_IN_PROGRESS,
		ACMP_LISTENER_STATE_CONNECTED,
		ACMP_LISTENER_STATE_DISCONNECTION_IN_PROGRESS,
	} state;

	uint64_t destination_mac_address;
	uint64_t stream_id;
	uint64_t talker_entity_id;
	uint16_t talker_unique_id;
	uint16_t flags;
	uint16_t stream_vlan_id;
	void *context;
};

struct acmp_listener {
	struct acmp_listener_slots incoming_slots;
	struct acmp_listener_signals outgoing_signals;
	void (*terminate) (struct acmp_listener * self);

	uint16_t listener_stream_sinks;
	struct acmp_listener_context listener_sink[ACMP_LISTENER_MAX_STREAMS];
};

bool acmp_listener_init(struct acmp_listener *self);

/// Destroy any resources that the acmp_listener uses
void acmp_listener_terminate(struct acmp_listener *self);

/// Receive an ACMPDU and process it
bool acmp_listener_rx_frame(void *self, struct frame *rx_frame, size_t pos);

/// Notify the state machine that time has passed. Call asap if early_tick is true.
void acmp_listener_tick(void *self, timestamp_in_microseconds timestamp);

/*@}*/

#endif				/* ACMP_LISTENER_H_ */
