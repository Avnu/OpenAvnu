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

/// Form the entity available message and send it
static void adp_adv_send_entity_available( struct adp_adv *self );

/// Form the entity departing message and send it
static void adp_adv_send_entity_departing( struct adp_adv *self );

/// Form the entity discover message and send it
static void adp_adv_send_entity_discover( struct adp_adv *self );

bool adp_adv_init(
        struct adp_adv *self,
        void *context,
        void ( *frame_send )( struct adp_adv *self, void *context, uint8_t const *buf, uint16_t len ),
        void ( *received_entity_available_or_departing )( struct adp_adv *self,
                                                          void *context,
                                                          void const *source_address,
                                                          int source_address_len,
                                                          struct adpdu *adpdu ) )
{
    self->last_time_in_ms = 0;
    self->do_send_entity_available = true;
    self->do_send_entity_departing = false;
    self->do_send_entity_discover = false;
    self->received_entity_available_or_departing = received_entity_available_or_departing;

    memset( &self->adpdu, 0, sizeof( self->adpdu ) );
    self->adpdu.subtype = AVTP_SUBTYPE_ADP;
    self->adpdu.version = 0;
    self->adpdu.sv = 0;
    self->adpdu.message_type = ADP_MESSAGE_TYPE_ENTITY_AVAILABLE;
    self->adpdu.valid_time = 10;
    self->adpdu.control_data_length = ADPDU_LEN - AVTP_COMMON_CONTROL_HEADER_LEN;

    return true;
}

void adp_adv_terminate( struct adp_adv *self )
{
    (void)self;
}

bool adp_adv_receive( struct adp_adv *self,
                          timestamp_in_microseconds time_in_microseconds,
                          void const *source_address,
                          int source_address_len,
                          uint8_t const *buf,
                          uint16_t len )
{
    struct adpdu incoming;
    bool r = false;
    (void)time_in_microseconds;
    if ( adpdu_read( &incoming, buf, 0, len ) > 0 )
    {
        r = true;
        switch ( incoming.message_type )
        {
        case ADP_MESSAGE_TYPE_ENTITY_DISCOVER:
            // only respond to discover messages if we are not stopped
            if ( !self->stopped )
            {
                // handle the case where the discover message references our entity id or 0
                if ( eui64_compare( &incoming.entity_id, &self->adpdu.entity_id )
                     || eui64_convert_to_uint64( &incoming.entity_id ) == 0 )
                {
                    self->do_send_entity_available = true;
                    self->early_tick = true;
                }
            }
            break;
        case ADP_MESSAGE_TYPE_ENTITY_AVAILABLE:
            // only handle incoming available messages if we have a place to give them to
            if ( self->received_entity_available_or_departing )
            {
                self->received_entity_available_or_departing(
                    self, self->context, source_address, source_address_len, &incoming );
            }
            break;
        case ADP_MESSAGE_TYPE_ENTITY_DEPARTING:
            // only handle incoming departing messages if we have a place to give them to
            if ( self->received_entity_available_or_departing )
            {
                self->received_entity_available_or_departing(
                    self, self->context, source_address, source_address_len, &incoming );
            }
            break;
        default:
            break;
        }
    }
    return r;
}

void adp_adv_tick( struct adp_adv *self, timestamp_in_microseconds cur_time_in_micros )
{

    // calculate the time since the last send
    timestamp_in_microseconds difftime = cur_time_in_micros - self->last_time_in_ms;

    timestamp_in_microseconds valid_time_in_ms = self->adpdu.valid_time;

    // calculate the time in microseconds between sends.
    // header.valid_time is in 2 second increments. We are to send
    // 4 available messages per valid_time.
    valid_time_in_ms = ( valid_time_in_ms * 1000 ) / 2;

    // limit it to be at most one send per second
    if ( valid_time_in_ms < 1000 )
    {
        valid_time_in_ms = 1000;
    }

    // clear any early tick flag
    self->early_tick = false;

    // only send messages available/departing messages if we are not stopped

    // are we departing?
    if ( self->do_send_entity_departing )
    {

        // yes, we are sending an entity departing message.
        // clear any do_send flags

        self->do_send_entity_departing = false;
        self->do_send_entity_available = false;

        // change into pause state

        self->stopped = true;

        // record the time we send it

        self->last_time_in_ms = cur_time_in_micros;

        // send the departing

        adp_adv_send_entity_departing( self );

        // reset the available_index to 0

        self->adpdu.available_index = 0;
    }

    if ( !self->stopped )
    {
        // if we are running and it is time to send an available, set the flag
        if ( difftime > valid_time_in_ms )
        {
            self->do_send_entity_available = true;
        }

        // if the flag is set for whatever reason and we are running then send the available and clear the flag
        if ( self->do_send_entity_available )
        {
            // we are to send entity available message
            // clear the request flag

            self->do_send_entity_available = false;

            // record the time we send it

            self->last_time_in_ms = cur_time_in_micros;

            // send the available

            adp_adv_send_entity_available( self );
        }
    }

    // are we asked to send an entity discover message?

    if ( self->do_send_entity_discover )
    {

        // yes, clear the flag and send it

        self->do_send_entity_discover = false;
        adp_adv_send_entity_discover( self );
    }
}

void adp_adv_trigger_send_discover( struct adp_adv *self )
{

    self->early_tick = true;
    self->do_send_entity_discover = true;
}

void adp_adv_trigger_send_available( struct adp_adv *self )
{
    self->early_tick = true;
    self->do_send_entity_available = true;
    self->stopped = false;
}

void adp_adv_trigger_send_departing( struct adp_adv *self )
{
    self->early_tick = true;
    self->do_send_entity_departing = true;
}

static void adp_adv_send_entity_available( struct adp_adv *self )
{
    struct frame f;
    self->adpdu.message_type = ADP_MESSAGE_TYPE_ENTITY_AVAILABLE;
    f.length = adpdu_write( &self->adpdu, &f.payload, 0, sizeof( f.payload ) );

    if ( f.length > 0 )
    {
        self->frame_send->send( self->frame_send, &f );
        self->adpdu.available_index++;
    }
}

static void adp_adv_send_entity_departing( struct adp_adv *self )
{
    struct frame f;
    self->adpdu.message_type = ADP_MESSAGE_TYPE_ENTITY_DEPARTING;
    f.length = adpdu_write( &self->adpdu, &f.payload, 0, sizeof( f.payload ) );

    if ( f.length > 0 )
    {
        self->frame_send->send( self->frame_send, &f );
        self->adpdu.available_index = 0;
    }
}

static void adp_adv_send_entity_discover( struct adp_adv *self )
{
    struct frame f;
    struct adpdu adpdu;
    memset( &adpdu, 0, sizeof( adpdu ) );
    adpdu.subtype = AVTP_SUBTYPE_ADP;
    adpdu.control_data_length = ADPDU_LEN - AVTP_COMMON_CONTROL_HEADER_LEN;
    adpdu.message_type = ADP_MESSAGE_TYPE_ENTITY_DISCOVER;
    adpdu.sv = 0;
    adpdu.version = 0;
    adpdu.valid_time = 0;
    eui64_init_from_uint64( &adpdu.entity_id, 0 );

    f.length = adpdu_write( &self->adpdu, &f.payload, 0, sizeof( f.payload ) );

    if ( f.length > 0 )
    {
        self->frame_send->send( self->frame_send, &f );
        self->adpdu.available_index++;
    }
}

