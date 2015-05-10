#pragma once

/*
  Copyright (c) 2013, J.D. Koftinoff Software, Ltd.
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
*/

#include "jdksavdecc_world.h"
#include "jdksavdecc_pdu.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \addtogroup adp ADP - Clause 6 */
/*@{*/

/** \addtogroup adpdu_msg ADP Message Types - Clause 6.2.1.5 */
/*@{*/
#define JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_AVAILABLE ( 0 )
#define JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DEPARTING ( 1 )
#define JDKSAVDECC_ADP_MESSAGE_TYPE_ENTITY_DISCOVER ( 2 )

/*@}*/

/** \addtogroup adpdu ADPDU - Clause 6.2.1 */
/*@{*/

struct jdksavdecc_adpdu_common_control_header
{
    uint32_t cd : 1;
    uint32_t subtype : JDKSAVDECC_SUBTYPE_DATA_SUBTYPE_WIDTH;
    uint32_t sv : 1;
    uint32_t version : JDKSAVDECC_SUBTYPE_DATA_VERSION_WIDTH;
    uint32_t message_type : JDKSAVDECC_SUBTYPE_DATA_CONTROL_DATA_WIDTH;
    uint32_t valid_time : JDKSAVDECC_SUBTYPE_DATA_STATUS_WIDTH;
    uint32_t control_data_length : JDKSAVDECC_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH;
    struct jdksavdecc_eui64 entity_id;
};

static inline ssize_t jdksavdecc_adpdu_common_control_header_read( struct jdksavdecc_adpdu_common_control_header *p,
                                                                   void const *base,
                                                                   ssize_t pos,
                                                                   size_t len )
{
    return jdksavdecc_common_control_header_read( (struct jdksavdecc_common_control_header *)p, base, pos, len );
}

static inline ssize_t jdksavdecc_adpdu_common_control_header_write( struct jdksavdecc_adpdu_common_control_header const *p,
                                                                    void *base,
                                                                    ssize_t pos,
                                                                    size_t len )
{
    return jdksavdecc_common_control_header_write( (struct jdksavdecc_common_control_header const *)p, base, pos, len );
}

#define JDKSAVDECC_ADPDU_OFFSET_ENTITY_MODEL_ID ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 0 )
#define JDKSAVDECC_ADPDU_OFFSET_ENTITY_CAPABILITIES ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 8 )
#define JDKSAVDECC_ADPDU_OFFSET_TALKER_STREAM_SOURCES ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 12 )
#define JDKSAVDECC_ADPDU_OFFSET_TALKER_CAPABILITIES ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 14 )
#define JDKSAVDECC_ADPDU_OFFSET_LISTENER_STREAM_SINKS ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 16 )
#define JDKSAVDECC_ADPDU_OFFSET_LISTENER_CAPABILITIES ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 18 )
#define JDKSAVDECC_ADPDU_OFFSET_CONTROLLER_CAPABILITIES ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 20 )
#define JDKSAVDECC_ADPDU_OFFSET_AVAILABLE_INDEX ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 24 )
#define JDKSAVDECC_ADPDU_OFFSET_GPTP_GRANDMASTER_ID ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 28 )
#define JDKSAVDECC_ADPDU_OFFSET_GPTP_DOMAIN_NUMBER ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 36 )
#define JDKSAVDECC_ADPDU_OFFSET_RESERVED0 ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 37 )
#define JDKSAVDECC_ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 40 )
#define JDKSAVDECC_ADPDU_OFFSET_INTERFACE_INDEX ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 42 )
#define JDKSAVDECC_ADPDU_OFFSET_ASSOCIATION_ID ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 44 )
#define JDKSAVDECC_ADPDU_OFFSET_RESERVED1 ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 52 )
#define JDKSAVDECC_ADPDU_LEN ( JDKSAVDECC_COMMON_CONTROL_HEADER_LEN + 56 )

/*@}*/

/** \addtogroup adp_entity_capability adp_entity_capability :
 * entity_capabilities field - Clause 6.2.1.10 */
/*@{*/

#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_EFU_MODE_BIT ( 31 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_EFU_MODE ( 0x00000001UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_EFU_MODE_MASK ( ~( 0x00000001UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ADDRESS_ACCESS_SUPPORTED_BIT ( 30 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ADDRESS_ACCESS_SUPPORTED ( 0x00000002UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ADDRESS_ACCESS_SUPPORTED_MASK ( ~( 0x00000002UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GATEWAY_ENTITY_BIT ( 29 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GATEWAY_ENTITY ( 0x00000004UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GATEWAY_ENTITY_MASK ( ~( 0x00000004UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_SUPPORTED_BIT ( 28 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_SUPPORTED ( 0x00000008UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_SUPPORTED_MASK ( ~( 0x00000008UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_LEGACY_AVC_BIT ( 27 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_LEGACY_AVC ( 0x00000010UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_LEGACY_AVC_MASK ( ~( 0x00000010UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_SUPPORTED_BIT ( 26 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_SUPPORTED ( 0x00000020UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_SUPPORTED_MASK ( ~( 0x00000020UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_VALID_BIT ( 25 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_VALID ( 0x00000040UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ASSOCIATION_ID_VALID_MASK ( ~( 0x00000040UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_VENDOR_UNIQUE_SUPPORTED_BIT ( 24 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_VENDOR_UNIQUE_SUPPORTED ( 0x00000080UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_VENDOR_UNIQUE_SUPPORTED_MASK ( ~( 0x00000080UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_A_SUPPORTED_BIT ( 23 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_A_SUPPORTED ( 0x00000100UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_A_SUPPORTED_MASK ( ~( 0x00000100UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_B_SUPPORTED_BIT ( 22 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_B_SUPPORTED ( 0x00000200UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_CLASS_B_SUPPORTED_MASK ( ~( 0x00000200UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GPTP_SUPPORTED_BIT ( 21 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GPTP_SUPPORTED ( 0x00000400UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GPTP_SUPPORTED_MASK ( ~( 0x00000400UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_SUPPORTED_BIT ( 20 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_SUPPORTED ( 0x00000800UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_SUPPORTED_MASK ( ~( 0x00000800UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_REQUIRED_BIT ( 19 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_REQUIRED ( 0x00001000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_AUTHENTICATION_REQUIRED_MASK ( ~( 0x00001000UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_PERSISTENT_ACQUIRE_SUPPORTED_BIT ( 18 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_PERSISTENT_ACQUIRE_SUPPORTED ( 0x00002000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_PERSISTENT_ACQUIRE_SUPPORTED_MASK ( ~( 0x00002000UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_IDENTIFY_CONTROL_INDEX_VALID_BIT ( 17 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_IDENTIFY_CONTROL_INDEX_VALID ( 0x00004000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_IDENTIFY_CONTROL_INDEX_VALID_MASK ( ~( 0x00004000UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_INTERFACE_INDEX_VALID_BIT ( 16 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_INTERFACE_INDEX_VALID ( 0x00008000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_AEM_INTERFACE_INDEX_VALID_MASK ( ~( 0x00008000UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GENERAL_CONTROLLER_IGNORE_BIT ( 15 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GENERAL_CONTROLLER_IGNORE ( 0x00010000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_GENERAL_CONTROLLER_IGNORE_MASK ( ~( 0x00010000UL ) )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ENTITY_NOT_READY_BIT ( 15 )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ENTITY_NOT_READY ( 0x00020000UL )
#define JDKSAVDECC_ADP_ENTITY_CAPABILITY_ENTITY_NOT_READY_MASK ( ~( 0x00020000UL ) )

/*@}*/

/** \addtogroup adp_controller_capability adp_controller_capability :
 * adp_controller_capabilities field - Clause 6.2.1.15  */
/*@{*/

#define JDKSAVDECC_ADP_CONTROLLER_CAPABILITY_IMPLEMENTED_BIT ( 31 )
#define JDKSAVDECC_ADP_CONTROLLER_CAPABILITY_IMPLEMENTED ( 0x00000001UL )
#define JDKSAVDECC_ADP_CONTROLLER_CAPABILITY_IMPLEMENTED_MASK ( ~( 0x00000001UL ) )

/*@}*/

/** \addtogroup talker_capability talker_capability : talker_capabilities field
 * - Clause 6.2.1.12  */
/*@{*/

#define JDKSAVDECC_ADP_TALKER_CAPABILITY_IMPLEMENTED_BIT ( 15 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_IMPLEMENTED ( 0x0001 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_IMPLEMENTED_MASK ( ~0x0001 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_OTHER_SOURCE_BIT ( 6 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_OTHER_SOURCE ( 0x0200 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_OTHER_SOURCE_MASK ( ~0x0200 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_CONTROL_SOURCE_BIT ( 5 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_CONTROL_SOURCE ( 0x0400 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_CONTROL_SOURCE_MASK ( ~0x0400 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MEDIA_CLOCK_SOURCE_BIT ( 4 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MEDIA_CLOCK_SOURCE ( 0x0800 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MEDIA_CLOCK_SOURCE_MASK ( ~0x0800 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_SMPTE_SOURCE_BIT ( 3 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_SMPTE_SOURCE ( 0x1000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_SMPTE_SOURCE_MASK ( ~0x1000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MIDI_SOURCE_BIT ( 2 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MIDI_SOURCE ( 0x2000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_MIDI_SOURCE_MASK ( ~0x2000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_AUDIO_SOURCE_BIT ( 1 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_AUDIO_SOURCE ( 0x4000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_AUDIO_SOURCE_MASK ( ~0x4000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_VIDEO_SOURCE_BIT ( 0 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_VIDEO_SOURCE ( 0x8000 )
#define JDKSAVDECC_ADP_TALKER_CAPABILITY_VIDEO_SOURCE_MASK ( ~0x8000 )

/*@}*/

/** \addtogroup adp_listener_capability adp_listener_capability : Listener
 * capabilities field - Clause 6.2.1.14  */
/*@{*/

#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_IMPLEMENTED_BIT ( 15 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_IMPLEMENTED ( 0x0001 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_IMPLEMENTED_MASK ( ~0x0001 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_OTHER_SINK_BIT ( 6 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_OTHER_SINK ( 0x0200 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_OTHER_SINK_MASK ( ~0x0200 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_CONTROL_SINK_BIT ( 5 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_CONTROL_SINK ( 0x0400 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_CONTROL_SINK_MASK ( ~0x0400 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MEDIA_CLOCK_SINK_BIT ( 4 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MEDIA_CLOCK_SINK ( 0x0800 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MEDIA_CLOCK_SINK_MASK ( ~0x0800 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_SMPTE_SINK_BIT ( 3 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_SMPTE_SINK ( 0x1000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_SMPTE_SINK_MASK ( ~0x1000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MIDI_SINK_BIT ( 2 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MIDI_SINK ( 0x2000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_MIDI_SINK_MASK ( ~0x2000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_AUDIO_SINK_BIT ( 1 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_AUDIO_SINK ( 0x4000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_AUDIO_SINK_MASK ( ~0x4000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_VIDEO_SINK_BIT ( 0 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_VIDEO_SINK ( 0x8000 )
#define JDKSAVDECC_ADP_LISTENER_CAPABILITY_VIDEO_SINK_MASK ( ~0x8000 )

/*@}*/

/** \addtogroup adpdu ADPDU - Clause 6.2.1 */
/*@{*/

/**
 * Extract the eui64 value of the entity_model_id field of the ADPDU object from
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the jdksavdecc_eui64 entity_model_id value
 */
static inline struct jdksavdecc_eui64 jdksavdecc_adpdu_get_entity_model_id( void const *base, ssize_t pos )
{
    return jdksavdecc_eui64_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_ENTITY_MODEL_ID );
}

/**
 * Store an eui64 value to the entity_model_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The jdksavdecc_eui64 entity_model_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_entity_model_id( struct jdksavdecc_eui64 v, void *base, ssize_t pos )
{
    jdksavdecc_eui64_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_ENTITY_MODEL_ID );
}

/**
 * Extract the uint32 value of the entity_capabilities field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint32_t entity_capabilities value
 */
static inline uint32_t jdksavdecc_adpdu_get_entity_capabilities( void const *base, ssize_t pos )
{
    return jdksavdecc_uint32_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_ENTITY_CAPABILITIES );
}

/**
 * Store a uint32 value to the entity_capabilities field of the ADPDU object to
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint32_t entity_capabilities value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_entity_capabilities( uint32_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint32_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_ENTITY_CAPABILITIES );
}

/**
 * Extract the uint16 value of the talker_stream_sources field of the ADPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t talker_stream_sources value
 */
static inline uint16_t jdksavdecc_adpdu_get_talker_stream_sources( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_TALKER_STREAM_SOURCES );
}

/**
 * Store a uint16 value to the talker_stream_sources field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t talker_stream_sources value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_talker_stream_sources( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_TALKER_STREAM_SOURCES );
}

/**
 * Extract the uint16 value of the talker_capabilities field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t talker_capabilities value
 */
static inline uint16_t jdksavdecc_adpdu_get_talker_capabilities( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_TALKER_CAPABILITIES );
}

/**
 * Store a uint16 value to the talker_capabilities field of the ADPDU object to
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t talker_capabilities value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_talker_capabilities( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_TALKER_CAPABILITIES );
}

/**
 * Extract the uint16 value of the listener_stream_sinks field of the ADPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t listener_stream_sinks value
 */
static inline uint16_t jdksavdecc_adpdu_get_listener_stream_sinks( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_LISTENER_STREAM_SINKS );
}

/**
 * Store a uint16 value to the listener_stream_sinks field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t listener_stream_sinks value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_listener_stream_sinks( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_LISTENER_STREAM_SINKS );
}

/**
 * Extract the uint16 value of the listener_capabilities field of the ADPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t listener_capabilities value
 */
static inline uint16_t jdksavdecc_adpdu_get_listener_capabilities( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_LISTENER_CAPABILITIES );
}

/**
 * Store a uint16 value to the listener_capabilities field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t listener_capabilities value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_listener_capabilities( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_LISTENER_CAPABILITIES );
}

/**
 * Extract the uint32 value of the controller_capabilities field of the ADPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint32_t controller_capabilities value
 */
static inline uint32_t jdksavdecc_adpdu_get_controller_capabilities( void const *base, ssize_t pos )
{
    return jdksavdecc_uint32_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_CONTROLLER_CAPABILITIES );
}

/**
 * Store a uint32 value to the controller_capabilities field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint32_t controller_capabilities value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_controller_capabilities( uint32_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint32_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_CONTROLLER_CAPABILITIES );
}

/**
 * Extract the uint32 value of the available_index field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint32_t available_index value
 */
static inline uint32_t jdksavdecc_adpdu_get_available_index( void const *base, ssize_t pos )
{
    return jdksavdecc_uint32_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_AVAILABLE_INDEX );
}

/**
 * Store a uint32 value to the available_index field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint32_t available_index value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_available_index( uint32_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint32_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_AVAILABLE_INDEX );
}

/**
 * Extract the eui64 value of the gptp_grandmaster_id field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the struct jdksavdecc_eui64 as_grandmaster_id value
 */
static inline struct jdksavdecc_eui64 jdksavdecc_adpdu_get_gptp_grandmaster_id( void const *base, ssize_t pos )
{
    return jdksavdecc_eui64_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_GPTP_GRANDMASTER_ID );
}

/**
 * Store a eui64 value to the gptp_grandmaster_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The struct jdksavdecc_eui64 as_grandmaster_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_gptp_grandmaster_id( struct jdksavdecc_eui64 v, void *base, ssize_t pos )
{
    jdksavdecc_eui64_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_GPTP_GRANDMASTER_ID );
}

/**
 * Extract the uint8 value of the gptp_domain_number field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint8_t gptp_domain_number value
 */
static inline uint8_t jdksavdecc_adpdu_get_gptp_domain_number( void const *base, ssize_t pos )
{
    return jdksavdecc_uint8_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_GPTP_DOMAIN_NUMBER );
}

/**
 * Store a uint8 value to the gptp_domain_number field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint8 listener_stream_sinks value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_gptp_domain_number( uint8_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint8_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_GPTP_DOMAIN_NUMBER );
}

/**
 * Extract the 24 bit value of the reserved0 field of the ADPDU object from a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint32_t reserved0 value (24 bits)
 */
static inline uint32_t jdksavdecc_adpdu_get_reserved0( void const *base, ssize_t pos )
{
    return jdksavdecc_uint32_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_RESERVED0 ) & 0x00ffffffUL;
}

/**
 * Store a uint32 value to the reserved0 field of the ADPDU object to a network
 *buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint32_t reserved0 value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_reserved0( uint32_t v, void *base, ssize_t pos )
{
    uint32_t top = jdksavdecc_uint8_get( base, pos );
    v &= 0x00ffffffUL;
    v |= top << 24;
    jdksavdecc_uint32_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_RESERVED0 );
}

/**
 * Extract the uint16 value of the identify_control_index field of the ADPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t identify_control_index value
 */
static inline uint16_t jdksavdecc_adpdu_get_identify_control_index( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX );
}

/**
 * Store a uint16 value to the identify_control_index field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t identify_control_index value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_identify_control_index( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX );
}

/**
 * Extract the uint16 value of the interface_index field of the ADPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t interface_index value
 */
static inline uint16_t jdksavdecc_adpdu_get_interface_index( void const *base, ssize_t pos )
{
    return jdksavdecc_uint16_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_INTERFACE_INDEX );
}

/**
 * Store a uint16 value to the identify_control_index field of the ADPDU object
 *to a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t interface_index value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_interface_index( uint16_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint16_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_INTERFACE_INDEX );
}

/**
 * Extract the eui64 value of the association_id field of the ADPDU object from
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the struct jdksavdecc_eui64 association_id value
 */
static inline struct jdksavdecc_eui64 jdksavdecc_adpdu_get_association_id( void const *base, ssize_t pos )
{
    return jdksavdecc_eui64_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_ASSOCIATION_ID );
}

/**
 * Store a eui64 value to the association_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The struct jdksavdecc_eui64 association_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_association_id( struct jdksavdecc_eui64 v, void *base, ssize_t pos )
{
    jdksavdecc_eui64_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_ASSOCIATION_ID );
}

/**
 * Extract the uint32 value of the reserved1 field of the ADPDU object from a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint32_t reserved1 value
 */
static inline uint32_t jdksavdecc_adpdu_get_reserved1( void const *base, ssize_t pos )
{
    return jdksavdecc_uint32_get( base, pos + JDKSAVDECC_ADPDU_OFFSET_RESERVED1 );
}

/**
 * Store a uint32 value to the reserved1 field of the ADPDU object to a network
 *buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint32_t reserved2 value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void jdksavdecc_adpdu_set_reserved1( uint32_t v, void *base, ssize_t pos )
{
    jdksavdecc_uint32_set( v, base, pos + JDKSAVDECC_ADPDU_OFFSET_RESERVED1 );
}

/*@}*/

/** \addtogroup adpdu ADPDU - Clause 6.2.1 */
/*@{*/

/// ADPDU - Clause 6.2.1
struct jdksavdecc_adpdu
{
    struct jdksavdecc_adpdu_common_control_header header;
    struct jdksavdecc_eui64 entity_model_id;
    uint32_t entity_capabilities;
    uint16_t talker_stream_sources;
    uint16_t talker_capabilities;
    uint16_t listener_stream_sinks;
    uint16_t listener_capabilities;
    uint32_t controller_capabilities;
    uint32_t available_index;
    struct jdksavdecc_eui64 gptp_grandmaster_id;
    uint8_t gptp_domain_number;
    uint32_t reserved0;
    uint16_t identify_control_index;
    uint16_t interface_index;
    struct jdksavdecc_eui64 association_id;
    uint32_t reserved1;
};

/**
 * Extract the jdksavdecc_adpdu structure from a network buffer.
 *
 *
 *
 * Bounds checking of the buffer size is done.
 *
 * @param p pointer to adpdu structure to fill in.
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @param len length of the raw memory buffer;
 * @return -1 if the buffer length is insufficent, otherwise the offset of the
 *octet following the structure in the buffer.
 */
static inline ssize_t jdksavdecc_adpdu_read( struct jdksavdecc_adpdu *p, void const *base, ssize_t pos, size_t len )
{
    ssize_t r = jdksavdecc_validate_range( pos, len, JDKSAVDECC_ADPDU_LEN );
    if ( r >= 0 )
    {
        jdksavdecc_adpdu_common_control_header_read( &p->header, base, pos, len );
        p->entity_model_id = jdksavdecc_adpdu_get_entity_model_id( base, pos );
        p->entity_capabilities = jdksavdecc_adpdu_get_entity_capabilities( base, pos );
        p->talker_stream_sources = jdksavdecc_adpdu_get_talker_stream_sources( base, pos );
        p->talker_capabilities = jdksavdecc_adpdu_get_talker_capabilities( base, pos );
        p->listener_stream_sinks = jdksavdecc_adpdu_get_listener_stream_sinks( base, pos );
        p->listener_capabilities = jdksavdecc_adpdu_get_listener_capabilities( base, pos );
        p->controller_capabilities = jdksavdecc_adpdu_get_controller_capabilities( base, pos );
        p->available_index = jdksavdecc_adpdu_get_available_index( base, pos );
        p->gptp_grandmaster_id = jdksavdecc_adpdu_get_gptp_grandmaster_id( base, pos );
        p->gptp_domain_number = jdksavdecc_adpdu_get_gptp_domain_number( base, pos );
        p->identify_control_index = jdksavdecc_adpdu_get_identify_control_index( base, pos );
        p->interface_index = jdksavdecc_adpdu_get_interface_index( base, pos );
        p->reserved0 = jdksavdecc_adpdu_get_reserved0( base, pos );
        p->association_id = jdksavdecc_adpdu_get_association_id( base, pos );
        p->reserved1 = jdksavdecc_adpdu_get_reserved1( base, pos );
    }
    return r;
}

/**
 * Store the jdksavdecc_adpdu structure to a network buffer.
 *
 *
 *
 * Bounds checking of the buffer size is done.
 *
 * @param p const pointer to adpdu structure to read from.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 * @param len length of the raw memory buffer;
 * @return -1 if the buffer length is insufficent, otherwise the offset of the
 *octet following the structure in the buffer.
 */
static inline ssize_t jdksavdecc_adpdu_write( struct jdksavdecc_adpdu const *p, void *base, size_t pos, size_t len )
{
    ssize_t r = jdksavdecc_validate_range( pos, len, JDKSAVDECC_ADPDU_LEN );
    if ( r >= 0 )
    {
        jdksavdecc_adpdu_common_control_header_write( &p->header, base, pos, len );
        jdksavdecc_adpdu_set_entity_model_id( p->entity_model_id, base, pos );
        jdksavdecc_adpdu_set_entity_capabilities( p->entity_capabilities, base, pos );
        jdksavdecc_adpdu_set_talker_stream_sources( p->talker_stream_sources, base, pos );
        jdksavdecc_adpdu_set_talker_capabilities( p->talker_capabilities, base, pos );
        jdksavdecc_adpdu_set_listener_stream_sinks( p->listener_stream_sinks, base, pos );
        jdksavdecc_adpdu_set_listener_capabilities( p->listener_capabilities, base, pos );
        jdksavdecc_adpdu_set_controller_capabilities( p->controller_capabilities, base, pos );
        jdksavdecc_adpdu_set_available_index( p->available_index, base, pos );
        jdksavdecc_adpdu_set_gptp_grandmaster_id( p->gptp_grandmaster_id, base, pos );
        jdksavdecc_adpdu_set_gptp_domain_number( p->gptp_domain_number, base, pos );
        jdksavdecc_adpdu_set_reserved0( p->reserved0, base, pos );
        jdksavdecc_adpdu_set_identify_control_index( p->identify_control_index, base, pos );
        jdksavdecc_adpdu_set_interface_index( p->interface_index, base, pos );
        jdksavdecc_adpdu_set_association_id( p->association_id, base, pos );
        jdksavdecc_adpdu_set_reserved1( p->reserved1, base, pos );
    }
    return r;
}

/*@}*/

/*@}*/

#ifdef __cplusplus
}
#endif
