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

#pragma once

#include <errno.h>
#include <stdint.h>

/* AAF PCM 'format' field values. */
#define AVTP_AAF_FORMAT_USER			0x00
#define AVTP_AAF_FORMAT_FLOAT_32BIT		0x01
#define AVTP_AAF_FORMAT_INT_32BIT		0x02
#define AVTP_AAF_FORMAT_INT_24BIT		0x03
#define AVTP_AAF_FORMAT_INT_16BIT		0x04
#define AVTP_AAF_FORMAT_AES3_32BIT		0x05

/* AAF PCM 'nsr' (nominal sample rate) field values. */
#define AVTP_AAF_PCM_NSR_USER			0x00
#define AVTP_AAF_PCM_NSR_8KHZ			0x01
#define AVTP_AAF_PCM_NSR_16KHZ			0x02
#define AVTP_AAF_PCM_NSR_32KHZ			0x03
#define AVTP_AAF_PCM_NSR_44_1KHZ		0x04
#define AVTP_AAF_PCM_NSR_48KHZ			0x05
#define AVTP_AAF_PCM_NSR_88_2KHZ		0x06
#define AVTP_AAF_PCM_NSR_96KHZ			0x07
#define AVTP_AAF_PCM_NSR_176_4KHZ		0x08
#define AVTP_AAF_PCM_NSR_192KHZ			0x09
#define AVTP_AAF_PCM_NSR_24KHZ			0x0A

/* AAF PCM 'sp' (sparse timestamp) field values. */
#define AVTP_AAF_PCM_SP_NORMAL			0x00
#define AVTP_AAF_PCM_SP_SPARSE			0x01

enum avtp_aaf_field {
	AVTP_AAF_FIELD_SV,
	AVTP_AAF_FIELD_MR,
	AVTP_AAF_FIELD_TV,
	AVTP_AAF_FIELD_SEQ_NUM,
	AVTP_AAF_FIELD_TU,
	AVTP_AAF_FIELD_STREAM_ID,
	AVTP_AAF_FIELD_TIMESTAMP,
	AVTP_AAF_FIELD_FORMAT,
	AVTP_AAF_FIELD_NSR,
	AVTP_AAF_FIELD_CHAN_PER_FRAME,
	AVTP_AAF_FIELD_BIT_DEPTH,
	AVTP_AAF_FIELD_STREAM_DATA_LEN,
	AVTP_AAF_FIELD_SP,
	AVTP_AAF_FIELD_EVT,
	AVTP_AAF_FIELD_MAX,
};

/* Get value from AAF AVTPDU field.
 * @pdu: Pointer to PDU struct.
 * @field: PDU field to be retrieved.
 * @val: Pointer to variable which the retrieved value should be saved.
 *
 * Returns:
 *    0: Success.
 *    -EINVAL: If any argument is invalid.
 */
int avtp_aaf_pdu_get(const struct avtp_stream_pdu *pdu,
				enum avtp_aaf_field field, uint64_t *val);

/* Set value from AAF AVTPDU field.
 * @pdu: Pointer to PDU struct.
 * @field: PDU field to be set.
 * @val: Value to be set.
 *
 * Returns:
 *    0: Success.
 *    -EINVAL: If any argument is invalid.
 */
int avtp_aaf_pdu_set(struct avtp_stream_pdu *pdu, enum avtp_aaf_field field,
								uint64_t val);

/* Initialize AAF AVTPDU. All AVTPDU fields are initialized with zero except
 * 'subtype' (which is set to AVTP_SUBTYPE_AAF) and 'sv' (which is set to 1).
 * @pdu: Pointer to PDU struct.
 *
 * Return values:
 *    0: Success.
 *    -EINVAL: If any argument is invalid.
 */
int avtp_aaf_pdu_init(struct avtp_stream_pdu *pdu);
