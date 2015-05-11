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

#ifndef ACMPDU_H_
#define ACMPDU_H_

#include "avtp.h"
#include "acmp.h"

/** \addtogroup acmpdu ACMPDU - Clause 8.2.1 */
/*@{*/

/**
 * Extract the eui64 value of the controller_entity_id field of the ACMPDU
 *object from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint64_t controller_entity_id value
 */
static inline struct eui64 acmpdu_get_controller_entity_id(void const *base,
							   ssize_t pos)
{
	return pdu_eui64_get(base, pos + ACMPDU_OFFSET_CONTROLLER_ENTITY_ID);
}

/**
 * Store a eui64 value to the controller_entity_id field of the ACMPDU object to
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The eui64 controller_entity_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_controller_entity_id(struct eui64 v, void *base,
						   ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ACMPDU_OFFSET_CONTROLLER_ENTITY_ID);
}

/**
 * Extract the eui64 value of the talker_entity_id field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint64_t talker_entity_id value
 */
static inline struct eui64 acmpdu_get_talker_entity_id(void const *base,
						       ssize_t pos)
{
	return pdu_eui64_get(base, pos + ACMPDU_OFFSET_TALKER_ENTITY_ID);
}

/**
 * Store a eui64 value to the talker_entity_id field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint64_t talker_entity_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_talker_entity_id(struct eui64 v, void *base,
					       ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ACMPDU_OFFSET_TALKER_ENTITY_ID);
}

/**
 * Extract the eui64 value of the listener_entity_id field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint64_t listener_entity_id value
 */
static inline struct eui64 acmpdu_get_listener_entity_id(void const *base,
							 ssize_t pos)
{
	return pdu_eui64_get(base, pos + ACMPDU_OFFSET_LISTENER_ENTITY_ID);
}

/**
 * Store a eui64 value to the listener_entity_id field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint64_t listener_entity_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_listener_entity_id(struct eui64 v, void *base,
						 ssize_t pos)
{
	pdu_eui64_set(v, base, pos + ACMPDU_OFFSET_LISTENER_ENTITY_ID);
}

/**
 * Extract the uint16 value of the talker_unique_id field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t talker_unique_id value
 */
static inline uint16_t acmpdu_get_talker_unique_id(void const *base,
						   ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_TALKER_UNIQUE_ID);
}

/**
 * Store a uint16 value to the talker_unique_id field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t talker_unique_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_talker_unique_id(uint16_t v, void *base,
					       ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_TALKER_UNIQUE_ID);
}

/**
 * Extract the uint16 value of the listener_unique_id field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t listener_unique_id value
 */
static inline uint16_t acmpdu_get_listener_unique_id(void const *base,
						     ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_LISTENER_UNIQUE_ID);
}

/**
 * Store a uint16 value to the listener_unique_id field of the ACMPDU object to
 *a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t listener_unique_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_listener_unique_id(uint16_t v, void *base,
						 ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_LISTENER_UNIQUE_ID);
}

/**
 * Extract the eui48 value of the stream_dest_mac field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the struct eui48 stream_dest_mac value
 */
static inline struct eui48 acmpdu_get_stream_dest_mac(void const *base,
						      ssize_t pos)
{
	return pdu_eui48_get(base, pos + ACMPDU_OFFSET_STREAM_DEST_MAC);
}

/**
 * Store a eui48 value to the stream_dest_mac field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The struct eui48 stream_dest_mac value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_stream_dest_mac(struct eui48 v, void *base,
					      ssize_t pos)
{
	pdu_eui48_set(v, base, pos + ACMPDU_OFFSET_STREAM_DEST_MAC);
}

/**
 * Extract the uint16 value of the connection_count field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t connection_count value
 */
static inline uint16_t acmpdu_get_connection_count(void const *base,
						   ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_CONNECTION_COUNT);
}

/**
 * Store a uint16 value to the connection_count field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t connection_count value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_connection_count(uint16_t v, void *base,
					       ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_CONNECTION_COUNT);
}

/**
 * Extract the uint16 value of the sequence_id field of the ACMPDU object from a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t sequence_id value
 */
static inline uint16_t acmpdu_get_sequence_id(void const *base, ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_SEQUENCE_ID);
}

/**
 * Store a uint16 value to the sequence_id field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t sequence_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_sequence_id(uint16_t v, void *base, ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_SEQUENCE_ID);
}

/**
 * Extract the uint16 value of the acmp_flags field of the ACMPDU object from a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t acmp_flags value
 */
static inline uint16_t acmpdu_get_flags(void const *base, ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_FLAGS);
}

/**
 * Store a uint16 value to the acmp_flags field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t acmp_flags value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_flags(uint16_t v, void *base, ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_FLAGS);
}

/**
 * Extract the uint16 value of the stream_vlan_id field of the ACMPDU object
 *from a network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t stream_vlan_id value
 */
static inline uint16_t acmpdu_get_stream_vlan_id(void const *base, ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_STREAM_VLAN_ID);
}

/**
 * Store a uint16 value to the stream_vlan_id field of the ACMPDU object to a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t stream_vlan_id value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_stream_vlan_id(uint16_t v, void *base,
					     ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_STREAM_VLAN_ID);
}

/**
 * Extract the uint16 value of the reserved field of the ACMPDU object from a
 *network buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @return the uint16_t reserved value
 */
static inline uint16_t acmpdu_get_reserved(void const *base, ssize_t pos)
{
	return pdu_uint16_get(base, pos + ACMPDU_OFFSET_RESERVED);
}

/**
 * Store a uint16 value to the reserved field of the ACMPDU object to a network
 *buffer.
 *
 *
 * No bounds checking of the memory buffer is done. It is the caller's
 *responsibility to pre-validate base and pos.
 *
 * @param v The uint16_t reserved value.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 */
static inline void acmpdu_set_reserved(uint16_t v, void *base, ssize_t pos)
{
	pdu_uint16_set(v, base, pos + ACMPDU_OFFSET_RESERVED);
}

/*@}*/

/** \addtogroup acmpdu ACMPDU - Clause 8.2.1 */
/*@{*/

/// ACMPDU - Clause 8.2.1
struct acmpdu {
	uint32_t subtype : AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH;
	uint32_t sv : 1;
	uint32_t version : AVTP_SUBTYPE_DATA_VERSION_WIDTH;
	uint32_t message_type : AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH;
	uint32_t status : AVTP_SUBTYPE_DATA_STATUS_WIDTH;
	uint32_t control_data_length
	    : AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH;
	struct eui64 stream_id;
	struct eui64 controller_entity_id;
	struct eui64 talker_entity_id;
	struct eui64 listener_entity_id;
	uint16_t talker_unique_id;
	uint16_t listener_unique_id;
	struct eui48 stream_dest_mac;
	uint16_t connection_count;
	uint16_t sequence_id;
	uint16_t flags;
	uint16_t stream_vlan_id;
	uint16_t reserved;
};

/**
 * Extract the acmpdu structure from a network buffer.
 *
 *
 *
 * Bounds checking of the buffer size is done.
 *
 * @param p pointer to acmpdu structure to fill in.
 * @param base pointer to raw memory buffer to read from.
 * @param pos offset from base to read the field from;
 * @param len length of the raw memory buffer;
 * @return -1 if the buffer length is insufficent, otherwise the offset of the
 *octet following the structure in the buffer.
 */
ssize_t acmpdu_read(struct acmpdu *p, void const *base, ssize_t pos,
		    size_t len);

/**
 * Store the acmpdu structure to a network buffer.
 *
 *
 *
 * Bounds checking of the buffer size is done.
 *
 * @param p const pointer to acmpdu structure to read from.
 * @param base pointer to raw memory buffer to write to.
 * @param pos offset from base to write the field to;
 * @param len length of the raw memory buffer;
 * @return -1 if the buffer length is insufficent, otherwise the offset of the
 *octet following the structure in the buffer.
 */
ssize_t acmpdu_write(struct acmpdu const *p, void *base, size_t pos,
		     size_t len);

/*@}*/

/*@}*/

#endif /* ACMPDU_H_ */
