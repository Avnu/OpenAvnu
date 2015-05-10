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

#ifndef AVTP_H_
#define AVTP_H_

#include "pdu.h"
#include "eui48.h"
#include "eui64.h"
#include "frame.h"

/** \addtogroup pdu AVTP and AVDECC PDU definitions */
/*@{*/

#define AVTP_ETHERTYPE ( 0x22f0 )	/// See IEEE Std 1722-2011 Clause 5.1.2
#define AVTP_PAYLOAD_OFFSET ( 12 )	/// See IEEE Std 1722-2011 Clause 5.2

/// See IEEE Std 1722-2011 Annex B
#define MAAP_MULTICAST_MAC (0x91e0f000ff00ULL)

/** \addtogroup subtype AVTP Rev1 Subtype definitions - See IEEE P1722-rev1 */
/*@{*/

#define AVTP_SUBTYPE_61883_IIDC ( 0x00 )
#define AVTP_SUBTYPE_MMA_STREAM ( 0x01 )
#define AVTP_SUBTYPE_AAF ( 0x02 )
#define AVTP_SUBTYPE_CVF ( 0x03 )
#define AVTP_SUBTYPE_CRF ( 0x04 )
#define AVTP_SUBTYPE_TSCF ( 0x05 )
#define AVTP_SUBTYPE_SVF ( 0x06 )
#define AVTP_SUBTYPE_RVF ( 0x07 )
#define AVTP_SUBTYPE_AEF_CONTINUOUS ( 0x6e )
#define AVTP_SUBTYPE_VSF_STREAM ( 0x6f )
#define AVTP_SUBTYPE_EF_STREAM ( 0x7f )
#define AVTP_SUBTYPE_MMA_CONTROL ( 0x81 )
#define AVTP_SUBTYPE_NTSCF ( 0x82 )
#define AVTP_SUBTYPE_ESCF ( 0xec )
#define AVTP_SUBTYPE_EECF ( 0xed )
#define AVTP_SUBTYPE_AEF_DISCRETE ( 0xee )
#define AVTP_SUBTYPE_ADP ( 0xfa )
#define AVTP_SUBTYPE_AECP ( 0xfb )
#define AVTP_SUBTYPE_ACMP ( 0xfc )
#define AVTP_SUBTYPE_MAAP ( 0xfe )
#define AVTP_SUBTYPE_EF_CONTROL ( 0xff )

/*@}*/

/** \addtogroup subtype_data AVTP subtype_data field - See IEEE Std 1722-2011 Clause 5.3 */
/*@{*/

#define AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH ( 8 )
#define AVTP_SUBTYPE_DATA_VERSION_WIDTH ( 3 )
#define AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH ( 4 )
#define AVTP_SUBTYPE_DATA_STATUS_WIDTH ( 5 )
#define AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH ( 11 )
#define AVTP_SUBTYPE_DATA_STREAM_MR_WIDTH ( 1 )
#define AVTP_SUBTYPE_DATA_STREAM_GV_WIDTH ( 1 )
#define AVTP_SUBTYPE_DATA_STREAM_TV_WIDTH ( 1 )
#define AVTP_SUBTYPE_DATA_STREAM_SEQUENCE_NUM_WIDTH ( 8 )
#define AVTP_SUBTYPE_DATA_STREAM_TU_WIDTH ( 1 )

static inline ssize_t avtp_read_subtype(uint8_t * host_value, void const *base,
					ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 1);
	if (r >= 0) {
		uint8_t const *b = (uint8_t const *)base;
		*host_value = b[pos + 0];
	}

	return r;
}

static inline ssize_t avtp_write_subtype(uint8_t const *host_value, void *base,
					 ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 1);
	if (r >= 0) {
		uint8_t *b = (uint8_t *) base;
		b[pos + 0] = *host_value;
	}
	return r;
}

static inline uint8_t avtp_subtype_data_get_subtype(void const *base,
						    size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return *p;
}

static inline void avtp_subtype_data_set_subtype(uint8_t v, void *base,
						 size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[0] = v;
}

static inline bool subtype_data_get_sv(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (bool) (((p[1] >> 7) & 1) != 0);
}

static inline void avtp_subtype_data_set_sv(bool v, void *base, size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0x7f) | (v ? 0x80 : 0x00);
}

static inline uint8_t avtp_subtype_data_get_version(void const *base,
						    size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (p[1] & 0x70) >> 4;
}

static inline void avtp_subtype_data_set_version(uint8_t v, void *base,
						 size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0x8f) | ((v & 0x7) << 4);
}

static inline uint8_t avtp_subtype_data_get_control_data(void const *base,
							 size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (p[1] & 0xf) >> 0;
}

static inline void avtp_avtp_subtype_data_set_control_data(uint8_t v,
							   void *base,
							   size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0xf0) | ((v & 0xf) << 0);
}

static inline uint8_t avtp_subtype_data_get_status(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (p[2] & 0xf8) >> 3;
}

static inline void avtp_subtype_data_set_status(uint8_t v, void *base,
						size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[2] = (p[2] & 0x07) | ((v & 0x1f) << 3);
}

static inline uint16_t avtp_subtype_data_get_control_data_length(void const
								 *base,
								 size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	uint16_t v = ((uint16_t) p[2]) + (((uint16_t) p[3]) & 0x3ff);
	return v;
}

static inline void avtp_subtype_data_set_control_data_length(uint16_t v,
							     void *base,
							     size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[2] = (p[2] & 0xf8) + (uint8_t) ((v >> 8) & 0x07);
	p[3] = (uint8_t) (v & 0xff);
}

static inline bool avtp_subtype_data_get_mr(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (bool) ((p[1] >> 3) & 1);
}

static inline void avtp_subtype_data_set_mr(bool v, void *base, size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0xf7) | (v ? 0x08 : 0x00);
}

static inline bool avtp_subtype_data_get_gv(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (bool) ((p[1] >> 1) & 1);
}

static inline void avtp_subtype_data_set_gv(bool v, void *base, size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0xfd) | (v ? 0x02 : 0x00);
}

static inline bool avtp_subtype_data_get_tv(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (bool) ((p[1] >> 0) & 1);
}

static inline void avtp_subtype_data_set_tv(bool v, void *base, size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[1] = (p[1] & 0xfe) | (v ? 0x01 : 0x00);
}

static inline uint8_t avtp_subtype_data_get_sequence_num(void const *base,
							 size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return p[2];
}

static inline void avtp_subtype_data_set_sequence_num(uint8_t v, void *base,
						      size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[2] = v;
}

static inline bool avtp_subtype_data_get_tu(void const *base, size_t pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (bool) ((p[3] >> 0) & 1);
}

static inline void avtp_subtype_data_set_tu(bool v, void *base, size_t pos)
{
	uint8_t *p = ((uint8_t *) base) + pos;
	p[3] = (p[3] & 0xfe) | (v ? 0x01 : 0x00);
}

/*@}*/

/** \addtogroup avtp_common_control_header AVTP common control header - See IEEE 1722-2011 Clause 5.3 */
/*@{*/

#define AVTP_COMMON_CONTROL_HEADER_OFFSET_SUBTYPE_DATA ( 0 )
#define AVTP_COMMON_CONTROL_HEADER_OFFSET_STREAM_ID ( 4 )
#define AVTP_COMMON_CONTROL_HEADER_LEN ( 12 )

static inline uint32_t avtp_common_control_header_get_sv(void const *base,
							 size_t pos)
{
    return subtype_data_get_sv(base,pos);
}

static inline void avtp_common_control_header_set_sv(bool v, void *base,
						     size_t pos)
{
	avtp_subtype_data_set_sv(v, base, pos);
}

static inline uint32_t avtp_common_control_header_get_version(void const *base,
							      size_t pos)
{
	return avtp_subtype_data_get_version(base, pos);
}

static inline void avtp_common_control_header_set_version(uint8_t v, void *base,
							  size_t pos)
{
	avtp_subtype_data_set_version(v, base, pos);
}

static inline uint8_t avtp_common_control_header_get_control_data(void const
								  *base,
								  size_t pos)
{
	return avtp_subtype_data_get_control_data(base, pos);
}

static inline void avtp_common_control_header_set_control_data(uint8_t v,
							       void *base,
							       size_t pos)
{
	avtp_avtp_subtype_data_set_control_data(v, base, pos);
}

static inline uint32_t avtp_common_control_header_get_status(void const *base,
							     size_t pos)
{
	return avtp_subtype_data_get_status(base, pos);
}

static inline void avtp_common_control_header_set_status(uint8_t v, void *base,
							 size_t pos)
{
	avtp_subtype_data_set_status(v, base, pos);
}

static inline uint16_t avtp_common_control_header_get_control_data_length(void
									  const
									  *base,
									  size_t
									  pos)
{
	uint8_t const *p = ((uint8_t const *)base) + pos;
	return (((uint16_t) (p[2] & 0x07)) << 8) + p[3];
}

static inline void avtp_common_control_header_set_control_data_length(uint16_t
								      v, void
								      *base,
								      size_t
								      pos)
{
	avtp_subtype_data_set_control_data_length(v, base, pos);
}

static inline uint64_t avtp_common_control_header_get_stream_id(void const
								*base,
								size_t pos)
{
    const uint8_t *pb = (const uint8_t*)base;
    const struct eui64 *p = (const struct eui64 *)&pb[pos+AVTP_COMMON_CONTROL_HEADER_OFFSET_STREAM_ID];

    return eui64_convert_to_uint64(p);
}

static inline void avtp_common_control_header_set_stream_id(uint64_t v,
							    void *base,
							    size_t pos)
{
    uint8_t *pb = (uint8_t*)base;
    struct eui64 *p = (struct eui64 *)&pb[pos+AVTP_COMMON_CONTROL_HEADER_OFFSET_STREAM_ID];

    eui64_init_from_uint64(p,v);
}

struct avtp_common_control_header {
    uint32_t subtype:AVTP_SUBTYPE_DATA_SUBTYPE_WIDTH;
	uint32_t sv:1;
    uint32_t version:AVTP_SUBTYPE_DATA_VERSION_WIDTH;
    uint32_t control_data:AVTP_SUBTYPE_DATA_CONTROL_DATA_WIDTH;
    uint32_t status:AVTP_SUBTYPE_DATA_STATUS_WIDTH;
	 uint32_t
        control_data_length:AVTP_SUBTYPE_DATA_CONTROL_DATA_LENGTH_WIDTH;
	uint64_t stream_id;
};

ssize_t avtp_common_control_header_read(struct avtp_common_control_header *p,
					void const *base,
					ssize_t pos, size_t len);
ssize_t avtp_common_control_header_write(struct avtp_common_control_header const
					 *p, void *base, ssize_t pos,
					 size_t len);

/*@}*/

/** \addtogroup avtp_avtp_common_stream_header AVTP common stream header - See IEEE Std 1722-2011 Clause 5.4 */
/*@{*/

#define AVTP_COMMON_STREAM_HEADER_OFFSET_SUBTYPE_DATA ( 0 )
#define AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_ID ( 4 )
#define AVTP_COMMON_STREAM_HEADER_OFFSET_TIMESTAMP ( 12 )
#define AVTP_COMMON_STREAM_HEADER_OFFSET_GATEWAY_INFO ( 16 )
#define AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_DATA_LENGTH ( 20 )
#define AVTP_COMMON_STREAM_HEADER_OFFSET_PROTOCOL_SPECIFIC_HEADER ( 22 )
#define AVTP_COMMON_STREAM_HEADER_LEN ( 24 )

static inline uint32_t avtp_common_stream_header_get_subtype_data(void const
								  *base,
								  size_t pos)
{
	uint32_t subtype_data;
	pdu_uint32_read(&subtype_data,
		    base,
		    pos + AVTP_COMMON_STREAM_HEADER_OFFSET_SUBTYPE_DATA,
		    AVTP_COMMON_STREAM_HEADER_OFFSET_SUBTYPE_DATA);
	return subtype_data;
}

static inline void avtp_common_stream_header_set_subtype_data(uint32_t
							      subtype_data,
							      void *base,
							      size_t pos)
{
	pdu_uint32_set(subtype_data, base,
		   pos + AVTP_COMMON_STREAM_HEADER_OFFSET_SUBTYPE_DATA);
}

static inline uint32_t avtp_common_stream_header_get_subtype(void const *base,
							     size_t pos)
{
	return avtp_subtype_data_get_subtype(base, pos);
}

static inline void avtp_common_stream_header_set_subtype(uint8_t v, void *base,
							 size_t pos)
{
	avtp_subtype_data_set_subtype(v, base, pos);
}

static inline uint32_t avtp_common_stream_header_get_sv(void const *base,
							size_t pos)
{
    return subtype_data_get_sv(base, pos);
}

static inline void avtp_common_stream_header_set_sv(bool v, void *base,
						    size_t pos)
{
	avtp_subtype_data_set_sv(v, base, pos);
}

static inline uint32_t avtp_common_stream_header_get_version(void const *base,
							     size_t pos)
{
	return avtp_subtype_data_get_version(base, pos);
}

static inline void avtp_common_stream_header_set_version(uint8_t v, void *base,
							 size_t pos)
{
	avtp_subtype_data_set_version(v, base, pos);
}

static inline bool avtp_common_stream_header_get_mr(void const *base,
						    size_t pos)
{
	return avtp_subtype_data_get_mr(base, pos);
}

static inline void avtp_common_stream_header_set_mr(bool v, void *base,
						    size_t pos)
{
	avtp_subtype_data_set_mr(v, base, pos);
}

static inline bool avtp_common_stream_header_get_gv(void const *base,
						    size_t pos)
{
	return avtp_subtype_data_get_gv(base, pos);
}

static inline void avtp_common_stream_header_set_gv(bool v, void *base,
						    size_t pos)
{
	avtp_subtype_data_set_gv(v, base, pos);
}

static inline bool avtp_common_stream_header_get_tv(void const *base,
						    size_t pos)
{
	return avtp_subtype_data_get_tv(base, pos);
}

static inline void avtp_common_stream_header_set_tv(bool v, void *base,
						    size_t pos)
{
	avtp_subtype_data_set_tv(v, base, pos);
}

static inline uint8_t avtp_common_stream_header_get_sequence_num(void const
								 *base,
								 size_t pos)
{
	return avtp_subtype_data_get_sequence_num(base, pos);
}

static inline void avtp_common_stream_header_set_sequence_num(uint8_t v,
							      void *base,
							      size_t pos)
{
	avtp_subtype_data_set_sequence_num(v, base, pos);
}

static inline bool avtp_common_stream_header_get_tu(void const *base,
						    size_t pos)
{
	return avtp_subtype_data_get_tu(base, pos);
}

static inline void avtp_common_stream_header_set_tu(bool v, void *base,
						    size_t pos)
{
	avtp_subtype_data_set_tu(v, base, pos);
}

static inline uint64_t avtp_common_stream_header_get_stream_id(void const *base,
							       size_t pos)
{
    return pdu_eui64_get(base, pos + AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_ID);
}

static inline void avtp_common_stream_header_set_stream_id(uint64_t v,
							   void *base,
							   size_t pos)
{
    pdu_eui64_set(v, base, pos + AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_ID);
}

static inline uint32_t avtp_common_stream_header_get_avtp_timestamp(void const
								    *base,
								    size_t pos)
{
	return pdu_uint32_get(base,
			  pos + AVTP_COMMON_STREAM_HEADER_OFFSET_TIMESTAMP);
}

static inline void avtp_common_stream_header_set_avtp_timestamp(uint32_t v,
								void *base,
								size_t pos)
{
	pdu_uint32_set(v, base, pos + AVTP_COMMON_STREAM_HEADER_OFFSET_TIMESTAMP);
}

static inline uint32_t avtp_common_stream_header_get_gateway_info(void const
								  *base,
								  size_t pos)
{
	return pdu_uint32_get(base,
			  pos + AVTP_COMMON_STREAM_HEADER_OFFSET_GATEWAY_INFO);
}

static inline void avtp_common_stream_header_set_avtp_gateway_info(uint32_t v,
								   void *base,
								   size_t pos)
{
	pdu_uint32_set(v, base,
		   pos + AVTP_COMMON_STREAM_HEADER_OFFSET_GATEWAY_INFO);
}

static inline uint16_t avtp_common_stream_header_get_stream_data_length(void
									const
									*base,
									size_t
									pos)
{
	return pdu_uint16_get(base,
			  pos +
			  AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_DATA_LENGTH);
}

static inline void avtp_common_stream_header_set_stream_data_length(uint16_t v,
								    void *base,
								    size_t pos)
{
	pdu_uint16_set(v, base,
		   pos + AVTP_COMMON_STREAM_HEADER_OFFSET_STREAM_DATA_LENGTH);
}

static inline uint16_t
avtp_common_stream_header_get_protocol_specific_header(void const *base,
						       size_t pos)
{
	return pdu_uint16_get(base,
			  pos +
			  AVTP_COMMON_STREAM_HEADER_OFFSET_PROTOCOL_SPECIFIC_HEADER);
}

static inline void
avtp_common_stream_header_set_protocol_specific_header(uint16_t v, void *base,
						       size_t pos)
{
	pdu_uint16_set(v, base,
		   pos +
		   AVTP_COMMON_STREAM_HEADER_OFFSET_PROTOCOL_SPECIFIC_HEADER);
}

/*@}*/

/*@}*/

#endif
