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

#ifndef ACMP_CONTROLLER_H_
#define ACMP_CONTROLLER_H_

#include "jdksavdecc.h"

struct acmp_controller_stream_source;
struct acmp_controller_stream_sink;
struct acmp_controller_connection;
struct acmp_controller_signals;
struct acmp_controller_slots;

struct jdksavdecc_acmpdu_list_item {

	struct jdksavdecc_acmpdu acmpdu;
};

struct jdksavdecc_acmpdu_list {
	int max_items;
	int num_items;
	struct jdksavdecc_acmpdu *items;
};

void acmpdu_list_init(struct jdksavdecc_acmpdu_list *self,
		      struct jdksavdecc_acmpdu *items, int max_items);

struct acmp_controller_stream_source {
	jdksavdecc_timestamp_in_microseconds last_talker_refresh_time;
	struct jdksavdecc_acmpdu acmpdu;
};

struct acmp_controller_stream_sink {
	jdksavdecc_timestamp_in_microseconds last_listener_refresh_time;
	struct jdksavdecc_acmpdu acmpdu;
};

struct acmp_controller_connection {
	enum { ACMP_CONTROLLER_CONNECTION_DISABLED = 0,
	       ACMP_CONTROLLER_CONNECTION_DISCONNECTED,
	       ACMP_CONTROLLER_CONNECTION_CONNECTING,
	       ACMP_CONTROLLER_CONNECTION_CONNECTED,
	       ACMP_CONTROLLER_CONNECTION_DISCONNECTING,
	       ACMP_CONTROLLER_CONNECTION_ERROR } state;
	int retry_count;
	struct jdksavdecc_eui64 talker_entity_id;
	uint16_t talker_unique_id;
	struct jdksavdecc_eui64 listener_entity_id;
	uint16_t listener_unique_id;
	struct acmp_controller_stream_source last_talker_connection_info;
	struct acmp_controller_stream_sink last_listener_info;
};

void acmp_controller_connection_init(struct acmp_controller_connection *self);

/** The actions that an ACMP Controller can be asked to perform */
struct acmp_controller_slots {
	/** Ask the object to to terminate */
	void (*terminate)(struct acmp_controller_slots *self);

	/** Ask the object to send its signals to destination_signals */
	void (*connect_signals)(
	    struct acmp_controller_slots *self,
	    struct acmp_controller_signals *destination_signals);

	/** Ask the object to disconnect the destination_signals */
	void (*disconnect_signals)(
	    struct acmp_controller_slots *self,
	    struct acmp_controller_signals *destination_signals);

	/** Ask the object to start processing */
	void (*start)(struct acmp_controller_slots *self);

	/** Ask the object to stop processing */
	void (*stop)(struct acmp_controller_slots *self);

	/** Ask the object to handle the specified ethernet frame */
	void (*handle_pdu)(struct acmp_controller_slots *self,
			   struct jdksavdecc_frame *frame);

	/** Ask the object to process any periodic timers */
	void (*tick)(struct acmp_controller_slots *self,
		     jdksavdecc_timestamp_in_microseconds current_time);

	/** Ask the object to track a specific stream sink */
	void (*track_stream_sink)(struct acmp_controller_slots *self,
				  struct jdksavdecc_eui64 listener_entity_id,
				  uint16_t listener_unique_id, int enable);

	/** Ask the object to track a specific stream source */
	void (*track_stream_source)(struct acmp_controller_slots *self,
				    struct jdksavdecc_eui64 talker_entity_id,
				    uint16_t talker_unique_id, int enable);

	/** Ask the object to connect a stream source to a stream sink */
	void (*connect_stream)(struct acmp_controller_slots *self,
			       struct jdksavdecc_eui64 talker_entity_id,
			       uint16_t talker_unique_id,
			       struct jdksavdecc_eui64 listener_entity_id,
			       uint16_t listener_unique_id, int enable);
};

/** The signals that an ACMP Controller can send to another object */
struct acmp_controller_signals {
	/** The signals were connected */
	void (*acmp_controller_connected)(
	    struct acmp_controller_signals *self,
	    struct acmp_controller_slots *acmp_controller_slots);

	/** The signals were disconnected */
	void (*acmp_controller_disconnected)(
	    struct acmp_controller_signals *self);

	/** The object was started */
	void (*acmp_controller_started)(struct acmp_controller_signals *self);

	/** The object was stopped */
	void (*acmp_controller_stopped)(struct acmp_controller_signals *self);

	/** The object is asking to send an ethernet frame */
	void (*acmp_controller_send_pdu)(struct acmp_controller_signals *self,
					 struct jdksavdecc_frame *acmpdu);

	/** The object is notifying that a stream source has been discovered or
	 * updated */
	void (*acmp_controller_stream_source_found)(
	    struct acmp_controller_signals *self,
	    struct acmp_controller_stream_source *info);

	/** The object is notifying that a stream sink has been discovered or
	 * updated */
	void (*acmp_controller_stream_sink_found)(
	    struct acmp_controller_signals *self,
	    struct acmp_controller_stream_sink *info);

	/** The object is notifying that a stream connection state changed */
	void (*acmp_controller_connection_changed)(
	    struct acmp_controller_signals *self,
	    struct acmp_controller_connection *info);
};

/** \addtogroup acmp_controller ACMP Controller State Machine */
/*@{*/

struct acmp_controller {
	/** The list of messages that the controller can receive */
	struct acmp_controller_slots incoming_slots;

	/** The list of signals that the controller can send */
	struct acmp_controller_signals *outgoing_signals;

	enum { ACMP_CONTROLLER_STOPPED, ACMP_CONTROLLER_STARTED } state;

	/** The maximum number of stream sources that the stream_sources
	 * parameter can hold */
	int max_stream_sources;

	/** The current number of stream sources that the stream_sources
	 * parameter is holding */
	int num_stream_sources;

	/** The array of stream sources */
	struct acmp_controller_stream_source *stream_sources;

	/** The maximum number of stream sinks that the stream_sinks parameter
	 * can hold */
	int max_stream_sinks;

	/** The current number of stream sources that the stream_sinks parameter
	 * is holding */
	int num_stream_sinks;

	/** The array of stream sinks */
	struct acmp_controller_stream_sink *stream_sinks;

	/** The maximum number of stream connections that the stream_connections
	 * parameter can hold */
	int max_connections;

	/** The current number of stream connections that the stream_connections
	 * parameter is holding */
	int num_connections;

	/** The array of stream connections */
	struct acmp_controller_connection *stream_connections;
};

/** Initialize an ACMP Controller */
void acmp_controller_init(
    struct acmp_controller *self, int max_stream_sources,
    struct acmp_controller_stream_source *stream_sources, int max_stream_sinks,
    struct acmp_controller_stream_sink *stream_sinks, int max_connections,
    struct acmp_controller_connection *stream_connections);

/** Ask the object to to terminate */
void acmp_controller_terminate(struct acmp_controller_slots *self);

/** Ask the object to send its signals to destination_signals */
void acmp_controller_connect_signals(
    struct acmp_controller_slots *self,
    struct acmp_controller_signals *destination_signals);

/** Ask the object to disconnect the destination_signals */
void acmp_controller_disconnect_signals(
    struct acmp_controller_slots *self,
    struct acmp_controller_signals *destination_signals);

/** Ask the object to start processing */
void acmp_controller_start(struct acmp_controller_slots *self);

/** Ask the object to stop processing */
void acmp_controller_stop(struct acmp_controller_slots *self);

/** Ask the object to handle the specified ethernet frame */
void acmp_controller_handle_pdu(struct acmp_controller_slots *self,
				struct jdksavdecc_frame *frame);

/** Ask the object to process any periodic timers */
void acmp_controller_tick(struct acmp_controller_slots *self,
			  jdksavdecc_timestamp_in_microseconds current_time);

/** Ask the object to track a specific stream sink */
void
acmp_controller_track_stream_sink(struct acmp_controller_slots *self,
				  struct jdksavdecc_eui64 listener_entity_id,
				  uint16_t listener_unique_id, int enable);

/** Ask the object to track a specific stream source */
void
acmp_controller_track_stream_source(struct acmp_controller_slots *self,
				    struct jdksavdecc_eui64 talker_entity_id,
				    uint16_t talker_unique_id, int enable);

/** Ask the object to connect a stream source to a stream sink */
void acmp_controller_connect_stream(struct acmp_controller_slots *self,
				    struct jdksavdecc_eui64 talker_entity_id,
				    uint16_t talker_unique_id,
				    struct jdksavdecc_eui64 listener_entity_id,
				    uint16_t listener_unique_id, int enable);

/*@}*/

/*@}*/

#endif /* ACMP_CONTROLLER_H_  */
