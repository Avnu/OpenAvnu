/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    * Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of Intel Corporation nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <arpa/inet.h>
#include <stddef.h>

#include "avtp.h"
#include "util.h"

#define SHIFT_SUBTYPE			(31 - 7)
#define SHIFT_VERSION			(31 - 11)

#define MASK_SUBTYPE			(BITMASK(8) << SHIFT_SUBTYPE)
#define MASK_VERSION			(BITMASK(3) << SHIFT_VERSION)

int avtp_pdu_get(const struct avtp_common_pdu *pdu, enum avtp_field field,
								uint32_t *val)
{
	uint32_t bitmap, mask;
	uint8_t shift;

	if (!pdu || !val)
		return -EINVAL;

	switch (field) {
	case AVTP_FIELD_SUBTYPE:
		mask = MASK_SUBTYPE;
		shift = SHIFT_SUBTYPE;
		break;
	case AVTP_FIELD_VERSION:
		mask = MASK_VERSION;
		shift = SHIFT_VERSION;
		break;
	default:
		return -EINVAL;
	}

	bitmap = ntohl(pdu->subtype_data);

	*val = BITMAP_GET_VALUE(bitmap, mask, shift);

	return 0;
}

int avtp_pdu_set(struct avtp_common_pdu *pdu, enum avtp_field field,
								uint32_t value)
{
	uint32_t bitmap, mask;
	uint8_t shift;

	if (!pdu)
		return -EINVAL;

	switch (field) {
	case AVTP_FIELD_SUBTYPE:
		mask = MASK_SUBTYPE;
		shift = SHIFT_SUBTYPE;
		break;
	case AVTP_FIELD_VERSION:
		mask = MASK_VERSION;
		shift = SHIFT_VERSION;
		break;
	default:
		return -EINVAL;
	}

	bitmap = ntohl(pdu->subtype_data);

	BITMAP_SET_VALUE(bitmap, value, mask, shift);

	pdu->subtype_data = htonl(bitmap);

	return 0;
}
