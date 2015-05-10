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

#ifndef ACMP_TALKER_H_
#define ACMP_TALKER_H_

#include "acmpdu.h"
#include "srp_info.h"

struct acmp_talker_signals;

struct acmp_talker_slots {

	void (*start) (struct acmp_talker_slots * self,
		       struct acmp_talker_signals * sender);

	void (*stop) (struct acmp_talker_slots * self,
		      struct acmp_talker_signals * sender);

	void (*handle_pdu) (struct acmp_talker_slots * self,
			    struct acmp_talker_signals * sender,
			    struct frame * frame);

	void (*tick) (struct acmp_talker_slots * self,
		      struct acmp_talker_signals * sender,
		      timestamp_in_microseconds current_time);

	void (*acmp_talker_stream_info_assigned) (struct acmp_talker_slots *
						  self,
						  struct acmp_talker_signals *
						  sender,
                          struct srp_info_first_value_talker *
						  srp_info);
};

struct acmp_talker_signals {

	void (*acmp_talker_started) (struct acmp_talker_signals * self,
				     struct acmp_talker_slots * source);

	void (*acmp_talker_stopped) (struct acmp_talker_signals * self,
				     struct acmp_talker_slots * source);

	void (*acmp_talker_send_pdu) (struct acmp_talker_signals * self,
				      struct acmp_talker_slots * source,
				      struct frame * pdu);

	void (*acmp_talker_start_request) (struct acmp_talker_signals * self,
					   struct acmp_talker_slots * source,
                       struct srp_info_first_value_talker * srp_info);

	void (*acmp_talker_listener_added) (struct acmp_talker_signals * self,
					    struct acmp_talker_slots * source,
					    uint64_t listener_entity_id,
					    uint16_t listener_unique_id,
					    uint16_t current_connection_count);

	void (*acmp_talker_listener_removed) (struct acmp_talker_signals * self,
					      struct acmp_talker_slots * source,
					      uint64_t listener_entity_id,
					      uint16_t listener_unique_id,
					      uint16_t
					      current_connection_count);
};

/** \addtogroup acmp_talker ACMP Talker State Machine */
/*@{*/

#ifndef ACMP_TALKER_MAX_LISTENERS_PER_STREAM
#define ACMP_TALKER_MAX_LISTENERS_PER_STREAM ( 16 )
#endif

#ifndef ACMP_TALKER_MAX_STREAMS
#define ACMP_TALKER_MAX_STREAMS ( 8 )
#endif

/**
 * @brief The acmp_talker_stream_source struct
 *
 * Contains the current state of a talker stream source and a context for additional
 * Entity specific information
 */
struct acmp_talker_stream_source {
    /**
     * @brief context The context for entity specific info
     */
	void *context;

    /**
     * @brief The stream source's unique id
     */
	uint16_t talker_unique_id;

    /**
     * @brief The stream sources's currently assigned destination_mac_address, or FF:FF:FF:FF:FF:FF if none assigned.
     */
    struct eui48 destination_mac_address;

    /**
     * @brief The stream source's currently assigned stream_id, or FF:FF:FF:FF:FF:FF:FF:FF is none assigned
     */
    struct eui64 stream_id;

    /**
     * @brief The current talker stream flags, See IEEE Std 1722.1-2013 Clause 8.2.1.17 and ACMP_FLAG_*
     *
     */
	uint16_t flags;

    /**
     * @brief The currently assigned stream_vlan_id, as assigned by higher layers or MVRP
     */
	uint16_t stream_vlan_id;

    /**
     * @brief The current count of active listeners
     */
	uint16_t connection_count;

    /**
     * @brief The current list the Entity ID's of the listeners that are registered to listen via ACMP
     */
    struct eui64 listener_entity_id[ACMP_TALKER_MAX_LISTENERS_PER_STREAM];

    /**
     * @brief The current list of listener unique ID's for each listener in listener_entity_id
     */
	uint16_t listener_unique_id[ACMP_TALKER_MAX_LISTENERS_PER_STREAM];
};

/**
 * @brief acmp_talker_stream_source_init initializes a talker stream source object
 * @param self raw memory to initialize
 * @param context void pointer to entity specific data
 * @param talker_unique_id the talker unique ID of this stream source
 */
void acmp_talker_stream_source_init(struct acmp_talker_stream_source *self,
				    void *context, uint16_t talker_unique_id);

/**
 * @brief acmp_talker_stream_source_update Update stream ID, destination MAC address and vlan ID.
 * @param self The talker stream source object
 * @param new_stream_id
 * @param new_destination_mac_address
 * @param new_stream_vlan_id
 */
void acmp_talker_stream_source_update(struct acmp_talker_stream_source *self,
                      struct eui64 new_stream_id,
                      struct eui48 new_destination_mac_address,
				      uint16_t new_stream_vlan_id);

/**
 * @brief acmp_talker_stream_source_clear_listeners clears the list of active listeners for a stream source
 * @param self
 */
void acmp_talker_stream_source_clear_listeners(struct acmp_talker_stream_source
					       *self);

/**
 * @brief acmp_talker_stream_source_add_listener adds a listener to the list of active listeners for a stream source
 * @param self The talker stream source object
 * @param listener_entity_id
 * @param listener_unique_id
 * @return true on success, false if there is no room to add it
 */
bool acmp_talker_stream_source_add_listener(struct acmp_talker_stream_source
                        *self, struct eui64 listener_entity_id,
					    uint16_t listener_unique_id);

/**
 * @brief acmp_talker_stream_source_remove_listener removes the listener from the list of active listeners for a stream
 *source
 *
 * Re-Orders list to be contiguous
 *
 * @param self The talker stream source object
 * @param listener_entity_id
 * @param listener_unique_id
 * @return true on success, false if the listener was not in the list
 */
bool acmp_talker_stream_source_remove_listener(struct acmp_talker_stream_source
					       *self,
                           struct eui64 listener_entity_id,
					       uint16_t listener_unique_id);

struct acmp_talker {
	struct acmp_talker_signals outgoing_signals;
	struct acmp_talker_slots incoming_slots;

	uint16_t talker_stream_sources;
	struct acmp_talker_stream_source talker_source[ACMP_TALKER_MAX_STREAMS];
};

bool acmp_talker_init(struct acmp_talker *self,
		      struct acmp_talker_signals const *outgoing_signals);

/// Destroy any resources that the acmp_talker uses
void acmp_talker_terminate(struct acmp_talker *self);

/// Receive an ACMPDU and process it
bool acmp_talker_rx_frame(struct acmp_talker *self, struct frame *rx_frame,
			  size_t pos);

/// Notify the state machine that time has passed. Call asap if early_tick is true.
void acmp_talker_tick(struct acmp_talker *self,
		      timestamp_in_microseconds timestamp);

/*@}*/

#endif				/* ACMP_TALKER_H_ */
