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

#include "pdu.h"

ssize_t pdu_validate_range(ssize_t bufpos, size_t buflen, size_t elem_size)
{
	return ((size_t)(bufpos) + (size_t)elem_size <= (size_t)buflen)
		   ? (ssize_t)(bufpos + elem_size)
		   : (ssize_t)-1;
}

uint16_t pdu_endian_reverse_uint16(const uint16_t *vin)
{
	uint8_t const *p = (uint8_t const *)(vin);
	return ((uint16_t)(p[1]) << 0) + ((uint16_t)(p[0]) << 8);
}

uint32_t pdu_endian_reverse_uint32(const uint32_t *vin)
{
	uint8_t const *p = (uint8_t const *)vin;
	return ((uint32_t)(p[3]) << 0) + ((uint32_t)(p[2]) << 8) +
	       ((uint32_t)(p[1]) << 16) + ((uint32_t)(p[0]) << 24);
}

uint64_t pdu_endian_reverse_uint64(const uint64_t *vin)
{
	uint8_t const *p = (uint8_t const *)vin;
	return ((uint64_t)(p[7]) << 0) + ((uint64_t)(p[6]) << 8) +
	       ((uint64_t)(p[5]) << 16) + ((uint64_t)(p[4]) << 24) +
	       ((uint64_t)(p[3]) << 32) + ((uint64_t)(p[2]) << 40) +
	       ((uint64_t)(p[1]) << 48) + ((uint64_t)(p[0]) << 56);
}

ssize_t pdu_uint8_read(uint8_t *host_value, const void *base, ssize_t pos,
		       ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 1);
	if (r >= 0) {
		uint8_t const *b = (uint8_t const *)base;
		*host_value = b[pos];
	}
	return r;
}

uint8_t pdu_uint8_get(const void *base, ssize_t pos)
{
	uint8_t const *b = (uint8_t const *)base;
	return b[pos];
}

ssize_t pdu_uint8_write(uint8_t v, void *base, ssize_t pos, ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 1);
	if (r >= 0) {
		uint8_t const *host_value = (uint8_t const *)&v;
		uint8_t *b = ((uint8_t *)base) + pos;

		b[0] = *host_value;
	}
	return r;
}

void pdu_uint8_set(uint8_t v, void *base, ssize_t pos)
{
	uint8_t *b = (uint8_t *)base;
	b[pos] = v;
}

ssize_t uint16_read(uint16_t *host_value, const void *base, ssize_t pos,
		    ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 2);
	if (r >= 0) {
		uint8_t const *b = ((uint8_t const *)base) + pos;
		*host_value = (((uint16_t)b[0]) << (8 * 1)) + b[1];
	}
	return r;
}

uint16_t pdu_uint16_get(const void *base, ssize_t pos)
{
	uint8_t const *b = ((uint8_t const *)base) + pos;
	return (((uint16_t)b[0]) << 8) + b[1];
}

ssize_t pdu_uint16_write(uint16_t v, void *base, ssize_t pos, ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 2);
	if (r >= 0) {
		uint16_t const *host_value = (uint16_t const *)&v;
		uint8_t *b = ((uint8_t *)base) + pos;

		b[0] = (uint8_t)((*host_value) >> 8) & 0xff;
		b[1] = (uint8_t)((*host_value) >> 0) & 0xff;
	}
	return r;
}

void pdu_uint16_set(uint16_t v, void *base, ssize_t pos)
{
	uint8_t *b = ((uint8_t *)base) + pos;
	b[0] = (uint8_t)((v) >> 8) & 0xff;
	b[1] = (uint8_t)((v) >> 0) & 0xff;
}

ssize_t pdu_uint32_read(uint32_t *host_value, const void *base, ssize_t pos,
			ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 4);
	if (r >= 0) {
		uint8_t const *b = ((uint8_t const *)base) + pos;
		*host_value = (((uint32_t)b[0]) << (8 * 3)) +
			      (((uint32_t)b[1]) << (8 * 2)) +
			      (((uint32_t)b[2]) << (8 * 1)) + b[3];
	}
	return r;
}

uint32_t pdu_uint32_get(const void *base, ssize_t pos)
{
	uint8_t const *b = ((uint8_t const *)base) + pos;
	return (((uint32_t)b[0]) << 24) + (((uint32_t)b[1]) << 16) +
	       (((uint32_t)b[2]) << 8) + b[3];
}

ssize_t pdu_uint32_write(uint32_t v, void *base, ssize_t pos, ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 4);
	if (r >= 0) {
		uint32_t const *host_value = (uint32_t const *)&v;
		uint8_t *b = ((uint8_t *)base) + pos;

		b[0] = (uint8_t)((*host_value) >> 24) & 0xff;
		b[1] = (uint8_t)((*host_value) >> 16) & 0xff;
		b[2] = (uint8_t)((*host_value) >> 8) & 0xff;
		b[3] = (uint8_t)((*host_value) >> 0) & 0xff;
	}
	return r;
}

void pdu_uint32_set(uint32_t v, void *base, ssize_t pos)
{
	uint8_t *b = ((uint8_t *)base) + pos;
	b[0] = (uint8_t)((v) >> 24) & 0xff;
	b[1] = (uint8_t)((v) >> 16) & 0xff;
	b[2] = (uint8_t)((v) >> 8) & 0xff;
	b[3] = (uint8_t)((v) >> 0) & 0xff;
}

ssize_t pdu_uint64_read(uint64_t *host_value, const void *base, ssize_t pos,
			ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 8);
	if (r >= 0) {
		uint8_t const *b = ((uint8_t const *)base) + pos;
		*host_value = (((uint64_t)b[0]) << (8 * 7)) +
			      (((uint64_t)b[1]) << (8 * 6)) +
			      (((uint64_t)b[2]) << (8 * 5)) +
			      (((uint64_t)b[3]) << (8 * 4)) +
			      (((uint64_t)b[4]) << (8 * 3)) +
			      (((uint64_t)b[5]) << (8 * 2)) +
			      (((uint64_t)b[6]) << (8 * 1)) + b[7];
	}
	return r;
}

uint64_t pdu_uint64_get(const void *base, ssize_t pos)
{
	uint8_t const *b = ((uint8_t const *)base) + pos;

	return (((uint64_t)b[0]) << (8 * 7)) + (((uint64_t)b[1]) << (8 * 6)) +
	       (((uint64_t)b[2]) << (8 * 5)) + (((uint64_t)b[3]) << (8 * 4)) +
	       (((uint64_t)b[4]) << (8 * 3)) + (((uint64_t)b[5]) << (8 * 2)) +
	       (((uint64_t)b[6]) << (8 * 1)) + b[7];
}

ssize_t pdu_uint64_write(uint64_t v, void *base, ssize_t pos, ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 8);
	if (r >= 0) {
		uint64_t const *host_value = (uint64_t const *)&v;
		uint8_t *b = ((uint8_t *)base) + pos;

		b[0] = (uint8_t)((*host_value) >> (8 * 7)) & 0xff;
		b[1] = (uint8_t)((*host_value) >> (8 * 6)) & 0xff;
		b[2] = (uint8_t)((*host_value) >> (8 * 5)) & 0xff;
		b[3] = (uint8_t)((*host_value) >> (8 * 4)) & 0xff;
		b[4] = (uint8_t)((*host_value) >> (8 * 3)) & 0xff;
		b[5] = (uint8_t)((*host_value) >> (8 * 2)) & 0xff;
		b[6] = (uint8_t)((*host_value) >> (8 * 1)) & 0xff;
		b[7] = (uint8_t)((*host_value)) & 0xff;
	}
	return r;
}

void pdu_uint64_set(uint64_t v, void *base, ssize_t pos)
{
	uint8_t *b = ((uint8_t *)base) + pos;
	b[0] = (uint8_t)((v) >> (8 * 7)) & 0xff;
	b[1] = (uint8_t)((v) >> (8 * 6)) & 0xff;
	b[2] = (uint8_t)((v) >> (8 * 5)) & 0xff;
	b[3] = (uint8_t)((v) >> (8 * 4)) & 0xff;
	b[4] = (uint8_t)((v) >> (8 * 3)) & 0xff;
	b[5] = (uint8_t)((v) >> (8 * 2)) & 0xff;
	b[6] = (uint8_t)((v) >> (8 * 1)) & 0xff;
	b[7] = (uint8_t)((v)) & 0xff;
}

#if !defined(PDU_DISABLE_FLOAT)

ssize_t pdu_float_read(float *host_value, const void *base, ssize_t pos,
		       size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(*host_value));

	if (r >= 0) {
		union {
			uint32_t host_value_int;
			float f;
		} hp;
		hp.host_value_int = pdu_uint32_get(base, pos);
		*host_value = hp.f;
	}
	return r;
}

float pdu_float_get(const void *base, ssize_t pos)
{
	union {
		uint32_t host_value_int;
		float f;
	} hp;
	hp.host_value_int = pdu_uint32_get(base, pos);
	return hp.f;
}

ssize_t pdu_float_write(float v, void *base, ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 4);

	if (r >= 0) {
		float *fp = &v;
		uint32_t *ip = (uint32_t *)fp;
		uint32_t host_value_int = *ip;
		pdu_uint32_set(host_value_int, base, pos);
	}
	return r;
}

void pdu_float_set(float v, void *base, ssize_t pos)
{
	union {
		uint32_t *host_value_int;
		float *f;
	} hp;
	hp.f = &v;
	pdu_uint32_set(*(hp.host_value_int), base, pos);
}
#endif

#if !defined(PDU_DISABLE_DOUBLE) && !defined(PDU_DISABLE_FLOAT)
ssize_t pdu_double_read(double *host_value, const void *base, ssize_t pos,
			size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(*host_value));

	if (r >= 0) {
		union {
			uint64_t host_value_int;
			float f;
		} hp;
		hp.host_value_int = pdu_uint64_get(base, pos);
		*host_value = hp.f;
	}
	return r;
}

double pdu_double_get(const void *base, ssize_t pos)
{
	union {
		uint64_t host_value_int;
		double d;
	} hp;
	hp.host_value_int = pdu_uint64_get(base, pos);
	return hp.d;
}

ssize_t pdu_double_write(double v, void *base, ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 8);

	if (r >= 0) {
		double *dp = &v;
		uint64_t *ip = (uint64_t *)dp;
		uint64_t host_value_int = *ip;
		pdu_uint64_set(host_value_int, base, pos);
	}
	return r;
}

void pdu_double_set(double v, void *base, ssize_t pos)
{
	union {
		uint64_t *host_value_int;
		double *d;
	} hp;
	hp.d = &v;
	pdu_uint64_set(*(hp.host_value_int), base, pos);
}
#endif

ssize_t pdu_eui48_read(struct eui48 *host_value, const void *base, ssize_t pos,
		       size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
	if (r >= 0) {
		memcpy(host_value->value, ((uint8_t const *)base) + pos,
		       sizeof(host_value->value));
	}
	return r;
}

ssize_t pdu_eui48_write(const struct eui48 *host_value, void *base, ssize_t pos,
			size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
	if (r >= 0) {
		memcpy(((uint8_t *)base) + pos, host_value->value,
		       sizeof(host_value->value));
	}
	return r;
}

struct eui48 pdu_eui48_get(const void *base, ssize_t pos)
{
	struct eui48 v;
	memcpy(v.value, ((uint8_t const *)base) + pos, sizeof(v.value));
	return v;
}

ssize_t pdu_eui64_read(struct eui64 *host_value, const void *base, ssize_t pos,
		       size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
	if (r >= 0) {
		memcpy(host_value->value, ((uint8_t const *)base) + pos,
		       sizeof(host_value->value));
		r = pos + sizeof(host_value->value);
	}
	return r;
}

struct eui64 pdu_eui64_get(const void *base, ssize_t pos)
{
	struct eui64 v;
	memcpy(v.value, ((uint8_t const *)base) + pos, sizeof(v.value));
	return v;
}

ssize_t pdu_eui64_write(const struct eui64 *host_value, void *base, ssize_t pos,
			size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
	if (r >= 0) {
		memcpy(((uint8_t *)base) + pos, host_value->value,
		       sizeof(host_value->value));
		r = pos + sizeof(host_value->value);
	}
	return r;
}

void pdu_eui64_set(struct eui64 v, void *base, ssize_t pos)
{
	memcpy(((uint8_t *)base) + pos, v.value, sizeof(v.value));
}

void pdu_gptp_seconds_init(struct gptp_seconds *self) { self->seconds = 0; }

void pdu_gptp_seconds_read(struct gptp_seconds *host_value, const void *base,
			   ssize_t pos)
{
	uint8_t const *b = ((uint8_t const *)base) + pos;
	host_value->seconds =
	    (((uint64_t)b[0]) << (8 * 5)) + (((uint64_t)b[1]) << (8 * 4)) +
	    (((uint64_t)b[2]) << (8 * 3)) + (((uint64_t)b[3]) << (8 * 2)) +
	    (((uint64_t)b[4]) << (8 * 1)) + b[5];
}

struct gptp_seconds pdu_gptp_seconds_get(const void *base, ssize_t pos)
{
	struct gptp_seconds v;
	uint8_t const *b = ((uint8_t const *)base) + pos;

	v.seconds =
	    (((uint64_t)b[0]) << (8 * 5)) + (((uint64_t)b[1]) << (8 * 4)) +
	    (((uint64_t)b[2]) << (8 * 3)) + (((uint64_t)b[3]) << (8 * 2)) +
	    (((uint64_t)b[4]) << (8 * 1)) + b[5];

	return v;
}

ssize_t pdu_gptp_seconds_write(struct gptp_seconds host_value, void *base,
			       ssize_t pos, ssize_t len)
{
	ssize_t r = pdu_validate_range(pos, len, 6);
	if (r >= 0) {
		uint8_t *b = ((uint8_t *)base) + pos;

		b[0] = (uint8_t)((host_value.seconds) >> (8 * 5)) & 0xff;
		b[1] = (uint8_t)((host_value.seconds) >> (8 * 4)) & 0xff;
		b[2] = (uint8_t)((host_value.seconds) >> (8 * 3)) & 0xff;
		b[3] = (uint8_t)((host_value.seconds) >> (8 * 2)) & 0xff;
		b[4] = (uint8_t)((host_value.seconds) >> (8 * 1)) & 0xff;
		b[5] = (uint8_t)((host_value.seconds)) & 0xff;
	}
	return r;
}

void pdu_gptp_seconds_set(struct gptp_seconds v, void *base, ssize_t pos)
{
	uint8_t *b = ((uint8_t *)base) + pos;
	b[0] = (uint8_t)((v.seconds) >> (8 * 5)) & 0xff;
	b[1] = (uint8_t)((v.seconds) >> (8 * 4)) & 0xff;
	b[2] = (uint8_t)((v.seconds) >> (8 * 3)) & 0xff;
	b[3] = (uint8_t)((v.seconds) >> (8 * 2)) & 0xff;
	b[4] = (uint8_t)((v.seconds) >> (8 * 1)) & 0xff;
	b[5] = (uint8_t)((v.seconds)) & 0xff;
}
