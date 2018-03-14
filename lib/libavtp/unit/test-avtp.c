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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <arpa/inet.h>

#include "avtp.h"

static void get_field_null_pdu(void **state)
{
	int res;
	uint32_t val = AVTP_SUBTYPE_MAAP;

	res = avtp_pdu_get(NULL, AVTP_FIELD_SUBTYPE, &val);

	assert_int_equal(res, -EINVAL);
}

static void get_field_null_val(void **state)
{
	int res;
	struct avtp_common_pdu pdu = { 0 };

	res = avtp_pdu_get(&pdu, AVTP_FIELD_SUBTYPE, NULL);

	assert_int_equal(res, -EINVAL);
}

static void get_field_invalid_field(void **state)
{
	int res;
	uint32_t val = AVTP_SUBTYPE_MAAP;
	struct avtp_common_pdu pdu = { 0 };

	res = avtp_pdu_get(&pdu, AVTP_FIELD_MAX, &val);

	assert_int_equal(res, -EINVAL);
}

static void get_field_subtype(void **state)
{
	int res;
	uint32_t val;
	struct avtp_common_pdu pdu = { 0 };

	/* Set 'subtype' field to 0xFE (AVTP_SUBTYPE_MAAP). */
	pdu.subtype_data = htonl(0xFE000000);

	res = avtp_pdu_get(&pdu, AVTP_FIELD_SUBTYPE, &val);

	assert_int_equal(res, 0);
	assert_true(val == AVTP_SUBTYPE_MAAP);
}

static void get_field_version(void **state)
{
	int res;
	uint32_t val;
	struct avtp_common_pdu pdu = { 0 };

	/* Set 'version' field to 5. */
	pdu.subtype_data = htonl(0x00500000);

	res = avtp_pdu_get(&pdu, AVTP_FIELD_VERSION, &val);

	assert_int_equal(res, 0);
	assert_true(val == 5);
}

static void set_field_null_pdu(void **state)
{
	int res;

	res = avtp_pdu_set(NULL, AVTP_FIELD_SUBTYPE, AVTP_SUBTYPE_MAAP);

	assert_int_equal(res, -EINVAL);
}

static void set_field_invalid_field(void **state)
{
	int res;
	struct avtp_common_pdu pdu = { 0 };

	res = avtp_pdu_set(&pdu, AVTP_FIELD_MAX, 1);

	assert_int_equal(res, -EINVAL);
}

static void set_field_subtype(void **state)
{
	int res;
	struct avtp_common_pdu pdu = { 0 };

	res = avtp_pdu_set(&pdu, AVTP_FIELD_SUBTYPE, AVTP_SUBTYPE_MAAP);

	assert_int_equal(res, 0);
	assert_true(ntohl(pdu.subtype_data) == 0xFE000000);
}

static void set_field_version(void **state)
{
	int res;
	struct avtp_common_pdu pdu = { 0 };

	res = avtp_pdu_set(&pdu, AVTP_FIELD_VERSION, 5);

	assert_int_equal(res, 0);
	assert_true(ntohl(pdu.subtype_data) == 0x00500000);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(get_field_null_pdu),
		cmocka_unit_test(get_field_null_val),
		cmocka_unit_test(get_field_invalid_field),
		cmocka_unit_test(get_field_subtype),
		cmocka_unit_test(get_field_version),
		cmocka_unit_test(set_field_null_pdu),
		cmocka_unit_test(set_field_invalid_field),
		cmocka_unit_test(set_field_subtype),
		cmocka_unit_test(set_field_version),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
