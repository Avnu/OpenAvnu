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

#include "pdu.h"
#include "avtp.h"
#include "acmpdu.h"
#include "acmp_controller.h"

static struct acmp_controller_slots acmp_controller_slots_table
    = { acmp_controller_terminate, acmp_controller_connect_signals,
	acmp_controller_disconnect_signals, acmp_controller_start,
	acmp_controller_stop, acmp_controller_handle_pdu,
	acmp_controller_tick, acmp_controller_track_stream_sink,
	acmp_controller_track_stream_source, acmp_controller_connect_stream
};

void acmp_controller_init(struct acmp_controller *self,
			  int max_stream_sources,
			  struct acmp_controller_stream_source *stream_sources,
			  int max_stream_sinks,
			  struct acmp_controller_stream_sink *stream_sinks,
			  int max_connections,
			  struct acmp_controller_connection *stream_connections)
{
	self->slots = acmp_controller_slots_table;
	self->signals = 0;
	self->state = ACMP_CONTROLLER_STOPPED;
	self->max_stream_sources = max_stream_sources;
	self->num_stream_sources = 0;
	self->stream_sources = stream_sources;
	self->max_stream_sinks = max_stream_sinks;
	self->num_stream_sinks = 0;
	self->stream_sinks = stream_sinks;
	self->max_connections = max_connections;
	self->num_connections = 0;
	self->stream_connections = stream_connections;
}

/** Ask the object to to terminate */
void acmp_controller_terminate(struct acmp_controller_slots *self_)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
	if (self->signals) {
		self_->disconnect_signals(self_, self->signals);
	}
}

/** Ask the object to send its signals to destination_signals */
void acmp_controller_connect_signals(struct acmp_controller_slots *self_, struct acmp_controller_signals
				     *destination_signals)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
	self->signals = destination_signals;
	destination_signals->acmp_controller_connected(destination_signals,
						       self_);
}

/** Ask the object to disconnect the destination_signals */
void acmp_controller_disconnect_signals(struct acmp_controller_slots *self_, struct acmp_controller_signals
					*destination_signals)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
	destination_signals->acmp_controller_disconnected(destination_signals);
	self->signals = 0;
}

/** Ask the object to start processing */
void acmp_controller_start(struct acmp_controller_slots *self_)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
	if (ACMP_CONTROLLER_STOPPED == self->state) {
		/* TODO: Initialize state machines */
		self->state = ACMP_CONTROLLER_STARTED;
		if (self->signals) {
			self->signals->acmp_controller_started(self->signals);
		}
	}
}

/** Ask the object to stop processing */
void acmp_controller_stop(struct acmp_controller_slots *self_)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;

	if (ACMP_CONTROLLER_STARTED == self->state) {
		/* TODO: Halt state machines */
		self->state = ACMP_CONTROLLER_STOPPED;
		if (self->signals) {
			self->signals->acmp_controller_stopped(self->signals);
		}
	}
}

/** Ask the object to handle the specified ethernet frame */
void acmp_controller_handle_pdu(struct acmp_controller_slots *self_,
				struct frame *frame)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
}

/** Ask the object to process any periodic timers */
void acmp_controller_tick(struct acmp_controller_slots *self_,
			  timestamp_in_microseconds current_time)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
}

/** Ask the object to track a specific stream sink */
void acmp_controller_track_stream_sink(struct acmp_controller_slots *self_,
                       struct eui64 listener_entity_id,
				       uint16_t listener_unique_id, bool enable)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
}

/** Ask the object to track a specific stream source */
void acmp_controller_track_stream_source(struct acmp_controller_slots *self_,
                     struct eui64 talker_entity_id,
					 uint16_t talker_unique_id, bool enable)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
}

/** Ask the object to connect a stream source to a stream sink */
void acmp_controller_connect_stream(struct acmp_controller_slots *self_,
                    struct eui64 talker_entity_id,
				    uint16_t talker_unique_id,
                    struct eui64 listener_entity_id,
				    uint16_t listener_unique_id, bool enable)
{
	struct acmp_controller *self = (struct acmp_controller *)self_;
}
