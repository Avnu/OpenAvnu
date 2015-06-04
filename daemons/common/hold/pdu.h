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

#ifndef PDU_H_
#define PDU_H_

#include "eui48.h"
#include "eui64.h"
#include "frame.h"

/* If platform does not have/want ieee754 format binary32, define
 * PDU_DISABLE_FLOAT */
/* If platform does not have/want ieee754 format binary64, define
 * PDU_DISABLE_DOUBLE */

/** \addtogroup util Utilities */
/*@{*/

/**
 * Validate buffer position in a buffer len for an element of elem_size.
 *
 * @param bufpos position of element in buffer, in octets
 * @param buflen length of buffer, in octets
 * @param elem_size element size, in octets
 * @return -1 if element does not fit, bufpos+elem_size if it does.
 */
int pdu_validate_range(int bufpos, int buflen, int elem_size);

/** \addtogroup endian Endian helpers / data access */
/*@{*/

/// Reverses endian of uint16_t value at *vin and returns it
uint16_t pdu_endian_reverse_uint16(uint16_t const *vin);

/// Reverses endian of uint32_t value at *vin and returns it
uint32_t pdu_endian_reverse_uint32(uint32_t const *vin);

/// Reverses endian of uint64_t value at *vin and returns it
uint64_t pdu_endian_reverse_uint64(uint64_t const *vin);

/// Read the uint8_t value at base[pos] and store it in *host_value. Returns new
/// pos, or -1 if pos is out of bounds.
int pdu_uint8_read(uint8_t *host_value, void const *base, int pos, int len);

/// get the uint8_t value at base[pos] and return it without bounds checking
uint8_t pdu_uint8_get(void const *base, int pos);

/// store the uint8_t value v into base[pos]. Returns -1 if pos is out of
/// bounds.
int pdu_uint8_write(uint8_t v, void *base, int pos, int len);

/// Set the uint8_t value at base[pos] to v without bounds checking
void pdu_uint8_set(uint8_t v, void *base, int pos);

/// Read the network order Doublet value at base[pos] and store it in
/// *host_value. Returns new pos, or -1 if pos is out of bounds.
int uint16_read(uint16_t *host_value, void const *base, int pos, int len);

/// get the uint8_t value at base[pos] and return it without bounds checking
uint16_t pdu_uint16_get(void const *base, int pos);

int pdu_uint16_write(uint16_t v, void *base, int pos, int len);

void pdu_uint16_set(uint16_t v, void *base, int pos);

int pdu_uint32_read(uint32_t *host_value, void const *base, int pos, int len);

uint32_t pdu_uint32_get(void const *base, int pos);

int pdu_uint32_write(uint32_t v, void *base, int pos, int len);

void pdu_uint32_set(uint32_t v, void *base, int pos);

int pdu_uint64_read(uint64_t *host_value, void const *base, int pos, int len);

uint64_t pdu_uint64_get(void const *base, int pos);

int pdu_uint64_write(uint64_t v, void *base, int pos, int len);

void pdu_uint64_set(uint64_t v, void *base, int pos);

#if !defined(PDU_DISABLE_FLOAT)
int pdu_float_read(float *host_value, void const *base, int pos, int len);

float pdu_float_get(void const *base, int pos);

int pdu_float_write(float v, void *base, int pos, int len);

void pdu_float_set(float v, void *base, int pos);

#endif

#if !defined(PDU_DISABLE_DOUBLE) && !defined(PDU_DISABLE_FLOAT)
int pdu_double_read(double *host_value, void const *base, int pos, int len);

double pdu_double_get(void const *base, int pos);

int pdu_double_write(double v, void *base, int pos, int len);

void pdu_double_set(double v, void *base, int pos);
#endif

/*@}*/

/** \addtogroup eui48 EUI48 */
/*@{*/
struct eui48 pdu_eui48_get(void const *base, int pos);

void pdu_eui48_set(struct eui48 v, void *base, int pos);

int pdu_eui48_read(struct eui48 *host_value, void const *base, int pos,
		   int len);

int pdu_eui48_write(struct eui48 const *host_value, void *base, int pos,
		    int len);

/*@}*/

/** \addtogroup eui64 EUI64 */
/*@{*/
struct eui64 pdu_eui64_get(void const *base, int pos);

void pdu_eui64_set(struct eui64 v, void *base, int pos);

int pdu_eui64_read(struct eui64 *host_value, void const *base, int pos,
		   int len);

int pdu_eui64_write(struct eui64 const *host_value, void *base, int pos,
		    int len);

/*@}*/

/** \addtogroup gptp GPTP time */
/*@{*/

struct gptp_seconds {
	uint64_t seconds : 48;
};

void pdu_gptp_seconds_init(struct gptp_seconds *self);

void pdu_gptp_seconds_read(struct gptp_seconds *host_value, void const *base,
			   int pos);

struct gptp_seconds pdu_gptp_seconds_get(void const *base, int pos);

int pdu_gptp_seconds_write(struct gptp_seconds host_value, void *base, int pos,
			   int len);

void pdu_gptp_seconds_set(struct gptp_seconds v, void *base, int pos);

/*@}*/

/*@}*/

#endif /* PDU_H_ */
