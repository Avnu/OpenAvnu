/****************************************************************************
  Copyright (c) 2015, J.D. Koftinoff Software, Ltd.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of J.D. Koftinoff Software, Ltd. nor the names of its
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

#ifndef SRP_ENCODING_H_
#define SRP_ENCODING_H_

#include "srp.h"
#include "srp_info.h"

#define SRP_ENCODING_MAX_TALKER_ADVERTISE_EVENTS_PER_VECTOR 2048

struct srp_encoding_talker_advertise_vector
{
    struct srp_info_talker first_value;
    uint16_t number_of_values;
    uint8_t fourpackedevents[SRP_ENCODING_MAX_TALKER_ADVERTISE_EVENTS_PER_VECTOR];
};

void srp_encoding_talker_advertise_vector_init(
        struct srp_encoding_talker_advertise_vector *self,
        struct srp_info_talker const *talker );

bool srp_encoding_talker_advertise_vector_add_event(
        struct srp_encoding_talker_advertise_vector *self,
        uint8_t fourpackedevent
        );

uint8_t srp_encoding_talker_advertise_vector_get_threepacked_event_for_item(
        struct srp_encoding_talker_advertise_vector const *self,
        size_t item
        );

#define SRP_ENCODING_MAX_TALKER_FAILED_EVENTS_PER_VECTOR 2048

struct srp_encoding_talker_failed_vector
{
    struct srp_info_talker first_value;
    uint16_t number_of_values;
    uint8_t fourpackedevents[SRP_ENCODING_MAX_TALKER_FAILED_EVENTS_PER_VECTOR];
};

void srp_encoding_talker_failed_vector_init(
        struct srp_encoding_talker_advertise_vector *self,
        struct srp_info_talker const *talker );

bool srp_encoding_talker_failed_vector_add_event(
        struct srp_encoding_talker_failed_vector *self,
        uint8_t fourpackedevent
        );

uint8_t srp_encoding_talker_failed_vector_get_threepacked_event_for_item(
        struct srp_encoding_talker_failed_vector const *self,
        size_t item
        );

#define SRP_ENCODING_MAX_LISTENER_EVENTS_PER_VECTOR 2048

struct srp_encoding_listener_vector
{
    struct srp_info_listener first_value;
    uint16_t number_of_values;
    uint8_t threepackedevents[SRP_ENCODING_MAX_LISTENER_EVENTS_PER_VECTOR];
    uint8_t fourpackedevents[SRP_ENCODING_MAX_LISTENER_EVENTS_PER_VECTOR];
};

void srp_encoding_listener_vector_init(
        struct srp_encoding_listener_vector *self,
        struct srp_info_listener const *first_value
        );

bool srp_encoding_listener_vector_add_event(
        struct srp_encoding_listener_vector *self,
        uint8_t threepackedevent,
        uint8_t fourpackedevent
        );

uint8_t srp_encoding_listener_vector_get_threepacked_event_for_item(
        struct srp_encoding_listener_vector const *self,
        size_t item
        );

uint8_t srp_encoding_listener_vector_get_fourpacked_event_for_item(
        struct srp_encoding_listener_vector const *self,
        size_t item
        );

#define SRP_ENCODING_MAX_DOMAIN_EVENTS_PER_VECTOR 2

struct srp_encoding_domain_vector
{
    struct srp_info_domain first_value;
    uint16_t number_of_values;
    uint8_t threepackedevents[SRP_ENCODING_MAX_DOMAIN_EVENTS_PER_VECTOR];
};


/**
 * @brief srp_encoding_domain_vector_init
 *
 * Initialize an srp_encoding_domain_vector object with the srp_info_listener object.
 * Start it out with a length of 0
 *
 * @param self
 * @param first_value
 */
void srp_encoding_domain_vector_init(
        struct srp_encoding_domain_vector *self,
        struct srp_info_listener const *first_value
        );

bool srp_encoding_domain_vector_add_event(
        struct srp_encoding_domain_vector *self,
        uint8_t threepackedevent
        );

uint8_t srp_encoding_domain_vector_get_threepacked_event_for_item(
        struct srp_encoding_domain_vector const *self,
        size_t item
        );

/**
 * @brief srp_encoding_talker_inc_first_value
 *
 * Increment the stream_id and destination_address fields of the srp_info_talker object
 *
 * @param self the srp_encoding_info_talker object to increment
 */
void srp_encoding_talker_inc_first_value( struct srp_info_talker *self );

/**
 * @brief srp_encoding_info_listener_inc_first_value
 *
 * Increment the stream_id field of the srp_info_listener object
 *
 * @param self the srp_info_listener object to increment
 */
void srp_encoding_listener_inc_first_value( struct srp_info_listener *self );

/**
 * @brief srp_encoding_domain_inc_first_value
 *
 * Increment the class_id and class_pri fields of the srp_info_domain object
 *
 * @param self the srp_info_domain object to increment
 */
void srp_encoding_domain_inc_first_value( struct srp_info_domain *self );

#endif /* SRP_ENCODING_H_ */
