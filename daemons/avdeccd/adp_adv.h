#pragma once

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
#include "jdksavb_world.h"

#ifdef __cplusplus
extern "C" {
#endif

struct jdksavb_adp_advertiser_slots;

struct jdksavb_adp_advertiser_signals
{
    void ( *adp_advertiser_started )( struct jdksavb_adp_advertiser_signals *self );

    void ( *adp_advertiser_stopped )( struct jdksavb_adp_advertiser_signals *self );

    void ( *adp_advertiser_send_pdu )( struct jdksavb_adp_advertiser_signals *self, struct jdksavb_frame *pdu );

    void ( *adp_advertiser_schedule_next_tick )( struct jdksavb_adp_advertiser_signals *self,
                                                 jdksavdecc_timestamp_in_microseconds next_absolute_time );
};

#ifdef __cplusplus
}
#endif
#pragma once

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
#include "jdksavb_world.h"
#include "jdksavb_adp_advertiser_signals.h"

#ifdef __cplusplus
extern "C" {
#endif

struct jdksavb_adp_advertiser_slots
{
    void ( *terminate )( struct jdksavb_adp_advertiser_slots *self );

    void ( *connect_signals )( struct jdksavb_adp_advertiser_slots *self,
                               struct jdksavb_adp_advertiser_signals *destination_signals );

    void ( *disconnect_signals )( struct jdksavb_adp_advertiser_slots *self,
                                  struct jdksavb_adp_advertiser_signals *destination_signals );

    void ( *set_adpdu )( struct jdksavb_adp_advertiser_slots *self, struct jdksavdecc_adpdu adpdu_params );

    void ( *gptp_gm_changed )( struct jdksavb_adp_advertiser_slots *self,
                               struct jdksavdecc_eui64 new_gtp_gm_id,
                               uint8_t new_gptp_domain_id );

    void ( *network_port_link_status_changed )( struct jdksavb_adp_advertiser_slots *self,
                                                struct jdksavdecc_eui48 mac_addr,
                                                bool up );

    void ( *start )( struct jdksavb_adp_advertiser_slots *self );

    void ( *stop )( struct jdksavb_adp_advertiser_slots *self );

    void ( *handle_pdu )( struct jdksavb_adp_advertiser_slots *self, struct jdksavb_frame *frame );

    void ( *tick )( struct jdksavb_adp_advertiser_slots *self, jdksavdecc_timestamp_in_microseconds current_time );
};

#ifdef __cplusplus
}
#endif
#pragma once

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
#include "jdksavb_world.h"
#include "jdksavb_frame.h"
#include "jdksavb_adp_advertiser_signals.h"
#include "jdksavb_adp_advertiser_slots.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup adpdu_msg ADP Message Types - Clause 6.2.1.5 */
/*@{*/

#define JDKSAVB_ADP_UNASSIGNED_IPV4 "0.0.0.0"
#define JDKSAVB_ADP_UNASSIGNED_IPV6 "0::0"
#define JDKSAVB_ADP_AVDECC_UDP_PORT "17221"
#define JDKSAVB_ADP_MDNS_MULTICAST_IPV4 "224.0.0.251"
#define JDKSAVB_ADP_MDNS_MULTICAST_IPV6 "ff02::fb"

/// jdksavb_adp object manages scheduling and sending adp entity available messages
/// and responding to adp entity discover messages.
struct jdksavb_adp_advertiser
{
    struct jdksavb_adp_advertiser_slots slots;

    struct jdksavb_adp_advertiser_signals *signals;

    bool stopped;
    bool early_tick;
    void *context;
    struct jdksavb_frame_sender *frame_send;

    /// The current ADPDU that is sent
    struct jdksavdecc_adpdu adpdu;

    /// The system time in microseconds that the last ADPDU was sent
    jdksavdecc_timestamp_in_microseconds last_time_in_ms;

    /// A flag used to notify the state machine to trigger the sending of a discover message immediately
    bool do_send_entity_discover;

    /// A flag used to notify the state machine to trigger the sending of an entity available message immediately
    bool do_send_entity_available;

    /// A flag used to notify the state machine to send departing message instead of available
    bool do_send_entity_departing;

    /// The function that the jdksavb_adp calls if it received an entity available or entity available
    /// for some other entity on the network.  May be set to 0 if the user does not care.
    void ( *received_entity_available_or_departing )( struct jdksavb_adp_advertiser *self,
                                                      void *context,
                                                      void const *source_address,
                                                      int source_address_len,
                                                      struct jdksavdecc_adpdu *adpdu );
};

/// Initialize an jdksavb_adp with the specified context and frame_send function and
/// received_entity_available_or_departing function
bool jdksavb_adp_advertiser_init(
    struct jdksavb_adp_advertiser *self,
    void *context,
    void ( *frame_send )( struct jdksavb_adp_advertiser *self, void *context, uint8_t const *buf, uint16_t len ),
    void ( *received_entity_available_or_departing )( struct jdksavb_adp_advertiser *self,
                                                      void *context,
                                                      void const *source_address,
                                                      int source_address_len,
                                                      struct jdksavdecc_adpdu *adpdu ) );

/// Destroy any resources that the jdksavb_adp uses
void jdksavb_adp_advertiser_terminate( struct jdksavb_adp_advertiser *self );

/// Receive an ADPU and process it
bool jdksavb_adp_receive( struct jdksavb_adp_advertiser *self,
                          jdksavdecc_timestamp_in_microseconds time_in_microseconds,
                          void const *source_address,
                          int source_address_len,
                          uint8_t const *buf,
                          uint16_t len );

/// Notify the state machine that time has passed. Call asap if early_tick is true.
void jdksavb_adp_tick( struct jdksavb_adp_advertiser *self, jdksavdecc_timestamp_in_microseconds cur_time_in_micros );

/// Request the state machine to send an entity discover message on the next tick.
void jdksavb_adp_trigger_send_discover( struct jdksavb_adp_advertiser *self );

/// Request the state machine to send an entity available message on the next tick.
/// Starts the state machine if is was stopped.
void jdksavb_adp_trigger_send_available( struct jdksavb_adp_advertiser *self );

/// Request the state machine to send an entity departing message on the next tick and
/// then transition to stopped mode and reset available_index to 0
void jdksavb_adp_trigger_send_departing( struct jdksavb_adp_advertiser *self );

/*@}*/

/*@}*/

#ifdef __cplusplus
}
#endif
