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

#include "adpdu.h"

#ifdef __cplusplus
extern "C" {
#endif

struct adp_adv_slots;

struct adp_adv_signals
{
    void ( *adp_adv_started )( struct adp_adv_signals *self );

    void ( *adp_adv_stopped )( struct adp_adv_signals *self );

    void ( *adp_adv_send_pdu )( struct adp_adv_signals *self, struct frame *pdu );

    void ( *adp_adv_schedule_next_tick )( struct adp_adv_signals *self,
                                                 timestamp_in_microseconds next_absolute_time );
};



struct adp_adv_slots
{
    void ( *terminate )( struct adp_adv_slots *self );

    void ( *connect_signals )( struct adp_adv_slots *self,
                               struct adp_adv_signals *destination_signals );

    void ( *disconnect_signals )( struct adp_adv_slots *self,
                                  struct adp_adv_signals *destination_signals );

    void ( *set_adpdu )( struct adp_adv_slots *self, struct adpdu adpdu_params );

    void ( *gptp_gm_changed )( struct adp_adv_slots *self,
                               struct eui64 new_gtp_gm_id,
                               uint8_t new_gptp_domain_id );

    void ( *network_port_link_status_changed )( struct adp_adv_slots *self,
                                                struct eui48 mac_addr,
                                                bool up );

    void ( *start )( struct adp_adv_slots *self );

    void ( *stop )( struct adp_adv_slots *self );

    void ( *handle_pdu )( struct adp_adv_slots *self, struct frame *frame );

    void ( *tick )( struct adp_adv_slots *self, timestamp_in_microseconds current_time );
};


/** \addtogroup adpdu_msg ADP Message Types - Clause 6.2.1.5 */
/*@{*/


/// adp object manages scheduling and sending adp entity available messages
/// and responding to adp entity discover messages.
struct adp_adv
{
    struct adp_adv_slots adv_slots;

    struct adp_adv_signals *adv_signals;

    bool stopped;
    bool early_tick;
    void *context;
    struct frame_sender *frame_send;

    /// The current ADPDU that is sent
    struct adpdu adpdu;

    /// The system time in microseconds that the last ADPDU was sent
    timestamp_in_microseconds last_time_in_ms;

    /// A flag used to notify the state machine to trigger the sending of a discover message immediately
    bool do_send_entity_discover;

    /// A flag used to notify the state machine to trigger the sending of an entity available message immediately
    bool do_send_entity_available;

    /// A flag used to notify the state machine to send departing message instead of available
    bool do_send_entity_departing;

    /// The function that the adp calls if it received an entity available or entity available
    /// for some other entity on the network.  May be set to 0 if the user does not care.
    void ( *received_entity_available_or_departing )( struct adp_adv *self,
                                                      void *context,
                                                      void const *source_address,
                                                      int source_address_len,
                                                      struct adpdu *adpdu );
};

/// Initialize an adp with the specified context and frame_send function and
/// received_entity_available_or_departing function
bool adp_adv_init(
    struct adp_adv *self,
    void *context,
    void ( *frame_send )( struct adp_adv *self, void *context, uint8_t const *buf, uint16_t len ),
    void ( *received_entity_available_or_departing )( struct adp_adv *self,
                                                      void *context,
                                                      void const *source_address,
                                                      int source_address_len,
                                                      struct adpdu *adpdu ) );

/// Destroy any resources that the adp uses
void adp_adv_terminate( struct adp_adv *self );

/// Receive an ADPU and process it
bool adp_receive( struct adp_adv *self,
                          timestamp_in_microseconds time_in_microseconds,
                          void const *source_address,
                          int source_address_len,
                          uint8_t const *buf,
                          uint16_t len );

/// Notify the state machine that time has passed. Call asap if early_tick is true.
void adp_tick( struct adp_adv *self, timestamp_in_microseconds cur_time_in_micros );

/// Request the state machine to send an entity discover message on the next tick.
void adp_trigger_send_discover( struct adp_adv *self );

/// Request the state machine to send an entity available message on the next tick.
/// Starts the state machine if is was stopped.
void adp_trigger_send_available( struct adp_adv *self );

/// Request the state machine to send an entity departing message on the next tick and
/// then transition to stopped mode and reset available_index to 0
void adp_trigger_send_departing( struct adp_adv *self );

/*@}*/

/*@}*/

#ifdef __cplusplus
}
#endif
