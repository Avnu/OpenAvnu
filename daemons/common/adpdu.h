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

#ifndef ADPDU_H_
#define ADPDU_H_

#include "avtp.h"
#include "adp.h"

/** \addtogroup adpdu ADPDU - Clause 6.2.1 */
/*@{*/

#define ADPDU_OFFSET_ENTITY_MODEL_ID (AVTP_COMMON_CONTROL_HEADER_LEN + 0)
#define ADPDU_OFFSET_ENTITY_CAPABILITIES (AVTP_COMMON_CONTROL_HEADER_LEN + 8)
#define ADPDU_OFFSET_TALKER_STREAM_SOURCES (AVTP_COMMON_CONTROL_HEADER_LEN + 12)
#define ADPDU_OFFSET_TALKER_CAPABILITIES (AVTP_COMMON_CONTROL_HEADER_LEN + 14)
#define ADPDU_OFFSET_LISTENER_STREAM_SINKS (AVTP_COMMON_CONTROL_HEADER_LEN + 16)
#define ADPDU_OFFSET_LISTENER_CAPABILITIES (AVTP_COMMON_CONTROL_HEADER_LEN + 18)
#define ADPDU_OFFSET_CONTROLLER_CAPABILITIES                                   \
	(AVTP_COMMON_CONTROL_HEADER_LEN + 20)
#define ADPDU_OFFSET_AVAILABLE_INDEX (AVTP_COMMON_CONTROL_HEADER_LEN + 24)
#define ADPDU_OFFSET_GPTP_GRANDMASTER_ID (AVTP_COMMON_CONTROL_HEADER_LEN + 28)
#define ADPDU_OFFSET_GPTP_DOMAIN_NUMBER (AVTP_COMMON_CONTROL_HEADER_LEN + 36)
#define ADPDU_OFFSET_RESERVED0 (AVTP_COMMON_CONTROL_HEADER_LEN + 37)
#define ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX                                    \
	(AVTP_COMMON_CONTROL_HEADER_LEN + 40)
#define ADPDU_OFFSET_INTERFACE_INDEX (AVTP_COMMON_CONTROL_HEADER_LEN + 42)
#define ADPDU_OFFSET_ASSOCIATION_ID (AVTP_COMMON_CONTROL_HEADER_LEN + 44)
#define ADPDU_OFFSET_RESERVED1 (AVTP_COMMON_CONTROL_HEADER_LEN + 52)
#define ADPDU_LEN (AVTP_COMMON_CONTROL_HEADER_LEN + 56)

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
 * @return the eui64 entity_model_id value
 */
static inline struct eui64 adpdu_get_entity_model_id(void const *base,
						     ssize_t pos)
{
	return pdu_eui64_get(base, pos + ADPDU_OFFSET_ENTITY_MODEL_ID);
}

/**
 * Store an eui64 value to the entity_model_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The eui64 entity_model_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void adpdu_set_entity_model_id(struct eui64 v, void *base,
					     ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ADPDU_OFFSET_ENTITY_MODEL_ID);
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
static inline uint32_t adpdu_get_entity_capabilities(void const *base,
						     ssize_t pos)
{
	return pdu_uint32_get(base, pos + ADPDU_OFFSET_ENTITY_CAPABILITIES);
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
static inline void adpdu_set_entity_capabilities(uint32_t v, void *base,
						 ssize_t pos)
{
	pdu_uint32_set(v, base, pos + ADPDU_OFFSET_ENTITY_CAPABILITIES);
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
static inline uint16_t adpdu_get_talker_stream_sources(void const *base,
						       ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_TALKER_STREAM_SOURCES);
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
static inline void adpdu_set_talker_stream_sources(uint16_t v, void *base,
						   ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_TALKER_STREAM_SOURCES);
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
static inline uint16_t adpdu_get_talker_capabilities(void const *base,
						     ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_TALKER_CAPABILITIES);
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
static inline void adpdu_set_talker_capabilities(uint16_t v, void *base,
						 ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_TALKER_CAPABILITIES);
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
static inline uint16_t adpdu_get_listener_stream_sinks(void const *base,
						       ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_LISTENER_STREAM_SINKS);
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
static inline void adpdu_set_listener_stream_sinks(uint16_t v, void *base,
						   ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_LISTENER_STREAM_SINKS);
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
static inline uint16_t adpdu_get_listener_capabilities(void const *base,
						       ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_LISTENER_CAPABILITIES);
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
static inline void adpdu_set_listener_capabilities(uint16_t v, void *base,
						   ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_LISTENER_CAPABILITIES);
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
static inline uint32_t adpdu_get_controller_capabilities(void const *base,
							 ssize_t pos)
{
	return pdu_uint32_get(base, pos + ADPDU_OFFSET_CONTROLLER_CAPABILITIES);
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
static inline void adpdu_set_controller_capabilities(uint32_t v, void *base,
						     ssize_t pos)
{
	pdu_uint32_set(v, base, pos + ADPDU_OFFSET_CONTROLLER_CAPABILITIES);
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
static inline uint32_t adpdu_get_available_index(void const *base, ssize_t pos)
{
	return pdu_uint32_get(base, pos + ADPDU_OFFSET_AVAILABLE_INDEX);
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
static inline void adpdu_set_available_index(uint32_t v, void *base,
					     ssize_t pos)
{
	pdu_uint32_set(v, base, pos + ADPDU_OFFSET_AVAILABLE_INDEX);
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
 * @return the struct eui64 as_grandmaster_id value
 */
static inline struct eui64 adpdu_get_gptp_grandmaster_id(void const *base,
							 ssize_t pos)
{
	return pdu_eui64_get(base, pos + ADPDU_OFFSET_GPTP_GRANDMASTER_ID);
}

/**
 * Store a eui64 value to the gptp_grandmaster_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The struct eui64 as_grandmaster_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void adpdu_set_gptp_grandmaster_id(struct eui64 v, void *base,
						 ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ADPDU_OFFSET_GPTP_GRANDMASTER_ID);
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
static inline uint8_t adpdu_get_gptp_domain_number(void const *base,
						   ssize_t pos)
{
	return pdu_uint8_get(base, pos + ADPDU_OFFSET_GPTP_DOMAIN_NUMBER);
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
static inline void adpdu_set_gptp_domain_number(uint8_t v, void *base,
						ssize_t pos)
{
	pdu_uint8_set(v, base, pos + ADPDU_OFFSET_GPTP_DOMAIN_NUMBER);
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
static inline uint32_t adpdu_get_reserved0(void const *base, ssize_t pos)
{
	return pdu_uint32_get(base, pos + ADPDU_OFFSET_RESERVED0) &
	       0x00ffffffUL;
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
static inline void adpdu_set_reserved0(uint32_t v, void *base, ssize_t pos)
{
	uint32_t top = pdu_uint8_get(base, pos);
	v &= 0x00ffffffUL;
	v |= top << 24;
	pdu_uint32_set(v, base, pos + ADPDU_OFFSET_RESERVED0);
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
static inline uint16_t adpdu_get_identify_control_index(void const *base,
							ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX);
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
static inline void adpdu_set_identify_control_index(uint16_t v, void *base,
						    ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_IDENTIFY_CONTROL_INDEX);
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
static inline uint16_t adpdu_get_interface_index(void const *base, ssize_t pos)
{
	return pdu_uint16_get(base, pos + ADPDU_OFFSET_INTERFACE_INDEX);
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
static inline void adpdu_set_interface_index(uint16_t v, void *base,
					     ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ADPDU_OFFSET_INTERFACE_INDEX);
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
 * @return the struct eui64 association_id value
 */
static inline struct eui64 adpdu_get_association_id(void const *base,
						    ssize_t pos)
{
	return pdu_eui64_get(base, pos + ADPDU_OFFSET_ASSOCIATION_ID);
}

/**
 * Store a eui64 value to the association_id field of the ADPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The struct eui64 association_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void adpdu_set_association_id(struct eui64 v, void *base,
					    ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ADPDU_OFFSET_ASSOCIATION_ID);
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
static inline uint32_t adpdu_get_reserved1(void const *base, ssize_t pos)
{
	return pdu_uint32_get(base, pos + ADPDU_OFFSET_RESERVED1);
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
static inline void adpdu_set_reserved1(uint32_t v, void *base, ssize_t pos)
{
	pdu_uint32_set(v, base, pos + ADPDU_OFFSET_RESERVED1);
}

/*@}*/

/** \addtogroup adpdu ADPDU - Clause 6.2.1 */
/*@{*/

/// ADPDU - Clause 6.2.1
struct adpdu {
	uint32_t subtype : AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH;
	uint32_t sv : 1;
	uint32_t version : AVTP_SUBTYPE_DATA_VERSION_WIDTH;
	uint32_t message_type : AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH;
	uint32_t valid_time : AVTP_SUBTYPE_DATA_STATUS_WIDTH;
	uint32_t control_data_length
	    : AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH;
	struct eui64 entity_id;
	struct eui64 entity_model_id;
	uint32_t entity_capabilities;
	uint16_t talker_stream_sources;
	uint16_t talker_capabilities;
	uint16_t listener_stream_sinks;
	uint16_t listener_capabilities;
	uint32_t controller_capabilities;
	uint32_t available_index;
	struct eui64 gptp_grandmaster_id;
	uint8_t gptp_domain_number;
	uint32_t reserved0;
	uint16_t identify_control_index;
	uint16_t interface_index;
	struct eui64 association_id;
	uint32_t reserved1;
};

/**
 * Extract the adpdu structure from a network buffer.
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
ssize_t adpdu_read(struct adpdu *p, void const *base, ssize_t pos, size_t len);

/**
 * Store the adpdu structure to a network buffer.
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
ssize_t adpdu_write(struct adpdu const *p, void *base, size_t pos, size_t len);

/*@}*/

/*@}*/

#endif /* ADPDU_H_ */
