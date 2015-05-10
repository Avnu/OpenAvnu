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

#include "eui64set.h"
#include "avtp.h"

/** \addtogroup acmp ACMP - Clause 8 */
/*@{*/

/** \addtogroup acmp_flag acmp_flag : flags field - Clause 8.2.1.17  */
/*@{*/

#define ACMP_FLAG_CLASS_B_BIT ( 15 )
#define ACMP_FLAG_CLASS_B ( 0x0001 )
#define ACMP_FLAG_CLASS_B_MASK ( ~0x0001 )
#define ACMP_FLAG_FAST_CONNECT_BIT ( 14 )
#define ACMP_FLAG_FAST_CONNECT ( 0x0002 )
#define ACMP_FLAG_FAST_CONNECT_MASK ( ~0x0002 )
#define ACMP_FLAG_SAVED_STATE_BIT ( 13 )
#define ACMP_FLAG_SAVED_STATE ( 0x0004 )
#define ACMP_FLAG_SAVED_STATE_MASK ( ~0x0004 )
#define ACMP_FLAG_STREAMING_WAIT_BIT ( 12 )
#define ACMP_FLAG_STREAMING_WAIT ( 0x0008 )
#define ACMP_FLAG_STREAMING_WAIT_MASK ( ~0x0008 )
#define ACMP_FLAG_SUPPORTS_ENCRYPTED_BIT ( 11 )
#define ACMP_FLAG_SUPPORTS_ENCRYPTED ( 0x0010 )
#define ACMP_FLAG_SUPPORTS_ENCRYPTED_MASK ( ~0x0010 )
#define ACMP_FLAG_ENCRYPTED_PDU_BIT ( 10 )
#define ACMP_FLAG_ENCRYPTED_PDU ( 0x0020 )
#define ACMP_FLAG_ENCRYPTED_PDU_MASK ( ~0x0020 )
#define ACMP_FLAG_TALKER_FAILED_BIT ( 9 )
#define ACMP_FLAG_TALKER_FAILED ( 0x0040 )
#define ACMP_FLAG_TALKER_FAILED_MASK ( ~0x0040 )

/*@}*/

/** \addtogroup acmpdu ACMPDU - Clause 8.2.1 */
/*@{*/

#define ACMPDU_OFFSET_CONTROLLER_ENTITY_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 0 )
#define ACMPDU_OFFSET_TALKER_ENTITY_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 8 )
#define ACMPDU_OFFSET_LISTENER_ENTITY_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 16 )
#define ACMPDU_OFFSET_TALKER_UNIQUE_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 24 )
#define ACMPDU_OFFSET_LISTENER_UNIQUE_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 26 )
#define ACMPDU_OFFSET_STREAM_DEST_MAC ( AVTP_COMMON_CONTROL_HEADER_LEN + 28 )
#define ACMPDU_OFFSET_CONNECTION_COUNT ( AVTP_COMMON_CONTROL_HEADER_LEN + 34 )
#define ACMPDU_OFFSET_SEQUENCE_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 36 )
#define ACMPDU_OFFSET_FLAGS ( AVTP_COMMON_CONTROL_HEADER_LEN + 38 )
#define ACMPDU_OFFSET_STREAM_VLAN_ID ( AVTP_COMMON_CONTROL_HEADER_LEN + 40 )
#define ACMPDU_OFFSET_RESERVED ( AVTP_COMMON_CONTROL_HEADER_LEN + 42 )
#define ACMPDU_LEN ( AVTP_COMMON_CONTROL_HEADER_LEN + 44 )

/*@}*/

/** \addtogroup acmp_message_type message_type field - Clause 8.2.1.5  */
/*@{*/

#define ACMP_MESSAGE_TYPE_CONNECT_TX_COMMAND ( 0 )
#define ACMP_MESSAGE_TYPE_CONNECT_TX_RESPONSE ( 1 )
#define ACMP_MESSAGE_TYPE_DISCONNECT_TX_COMMAND ( 2 )
#define ACMP_MESSAGE_TYPE_DISCONNECT_TX_RESPONSE ( 3 )
#define ACMP_MESSAGE_TYPE_GET_TX_STATE_COMMAND ( 4 )
#define ACMP_MESSAGE_TYPE_GET_TX_STATE_RESPONSE ( 5 )
#define ACMP_MESSAGE_TYPE_CONNECT_RX_COMMAND ( 6 )
#define ACMP_MESSAGE_TYPE_CONNECT_RX_RESPONSE ( 7 )
#define ACMP_MESSAGE_TYPE_DISCONNECT_RX_COMMAND ( 8 )
#define ACMP_MESSAGE_TYPE_DISCONNECT_RX_RESPONSE ( 9 )
#define ACMP_MESSAGE_TYPE_GET_RX_STATE_COMMAND ( 10 )
#define ACMP_MESSAGE_TYPE_GET_RX_STATE_RESPONSE ( 11 )
#define ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_COMMAND ( 12 )
#define ACMP_MESSAGE_TYPE_GET_TX_CONNECTION_RESPONSE ( 13 )

/*@}*/

/** \addtogroup acmp_timeouts ACMP timeouts field - Clause 8.2.2  */
/*@{*/
#define ACMP_TIMEOUT_CONNECT_TX_COMMAND_MS ( 2000 )
#define ACMP_TIMEOUT_DISCONNECT_TX_COMMAND_MS ( 200 )
#define ACMP_TIMEOUT_GET_TX_STATE_COMMAND ( 200 )
#define ACMP_TIMEOUT_CONNECT_RX_COMMAND_MS ( 4500 )
#define ACMP_TIMEOUT_DISCONNECT_RX_COMMAND_MS ( 500 )
#define ACMP_TIMEOUT_GET_RX_STATE_COMMAND_MS ( 200 )
#define ACMP_TIMEOUT_GET_TX_CONNECTION_COMMAND ( 200 )
/*@}*/

/** \addtogroup acmp_status ACMP status field - Clause 8.2.1.6  */
/*@{*/

#define ACMP_STATUS_SUCCESS ( 0 )
#define ACMP_STATUS_LISTENER_UNKNOWN_ID ( 1 )
#define ACMP_STATUS_TALKER_UNKNOWN_ID ( 2 )
#define ACMP_STATUS_TALKER_DEST_MAC_FAIL ( 3 )
#define ACMP_STATUS_TALKER_NO_STREAM_INDEX ( 4 )
#define ACMP_STATUS_TALKER_NO_BANDWIDTH ( 5 )
#define ACMP_STATUS_TALKER_EXCLUSIVE ( 6 )
#define ACMP_STATUS_LISTENER_TALKER_TIMEOUT ( 7 )
#define ACMP_STATUS_LISTENER_EXCLUSIVE ( 8 )
#define ACMP_STATUS_STATE_UNAVAILABLE ( 9 )
#define ACMP_STATUS_NOT_CONNECTED ( 10 )
#define ACMP_STATUS_NO_SUCH_CONNECTION ( 11 )
#define ACMP_STATUS_COULD_NOT_SEND_MESSAGE ( 12 )
#define ACMP_STATUS_TALKER_MISBEHAVING ( 13 )
#define ACMP_STATUS_LISTENER_MISBEHAVING ( 14 )
#define ACMP_STATUS_RESERVED ( 15 )
#define ACMP_STATUS_CONTROLLER_NOT_AUTHORIZED ( 16 )
#define ACMP_STATUS_INCOMPATIBLE_REQUEST ( 17 )

/// New: The AVDECC Listener is being asked to connect to something that it cannot listen to, e.g. it is being asked to
/// listen to it's own AVDECC Talker stream.
#define ACMP_STATUS_LISTENER_INVALID_CONNECTION ( 18 )
#define ACMP_STATUS_NOT_SUPPORTED ( 31 )

/*@}*/

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
static inline uint64_t acmpdu_get_controller_entity_id(void const *base,
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
static inline void acmpdu_set_controller_entity_id(uint64_t v, void *base,
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
static inline uint64_t acmpdu_get_talker_entity_id(void const *base,
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
static inline void acmpdu_set_talker_entity_id(uint64_t v, void *base,
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
static inline uint64_t acmpdu_get_listener_entity_id(void const *base,
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
static inline void acmpdu_set_listener_entity_id(uint64_t v, void *base,
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
static inline struct pdu_eui48 acmpdu_get_stream_dest_mac(void const *base,
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
static inline void acmpdu_set_stream_dest_mac(struct pdu_eui48 v, void *base,
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
	uint32_t cd:1;
	uint32_t subtype:AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH;
	uint32_t sv:1;
	uint32_t version:AVTP_SUBTYPE_DATA_VERSION_WIDTH;
	uint32_t message_type:AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH;
	uint32_t status:AVTP_SUBTYPE_DATA_STATUS_WIDTH;
	 uint32_t
	    control_data_length:AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH;
	uint64_t stream_id;
	uint64_t controller_entity_id;
	uint64_t talker_entity_id;
	uint64_t listener_entity_id;
	uint16_t talker_unique_id;
	uint16_t listener_unique_id;
	uint64_t stream_dest_mac;
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

#endif				/* ACMPDU_H_ */
