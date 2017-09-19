/******************************************************************************

  Copyright (c) 2015, AudioScience, Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the AudioScience, Inc nor the names of its
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef __linux__
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#else
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define SCNu64       "I64u"
#define SCNx64       "I64x"
#endif

#include "CppUTest/TestHarness.h"

extern "C"
{

#include "parse.h"

}


TEST_GROUP(ParseTestGroup)
{
};

/*
* Test parse_null operation.
*/
TEST(ParseTestGroup, TestParse_null)
{
	uint32_t value;
	int err_index;
	int status;
	struct parse_param specs[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_null, &value },
		{ 0, parse_null, 0 }};
	const char strz[] = "C=1234";

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specs, &err_index);
	CHECK(0 != status);

	// parse null is an error
	memset(&value, 0, sizeof(value));
	status = parse(strz, sizeof(strz), specs, &err_index);
	CHECK(0 != status);

}

/*
* Test parse_u8 operation.
*/
TEST(ParseTestGroup, TestParse_u8)
{
	uint8_t value;
	uint8_t ref;
	int err_index;
	int status;
	struct parse_param specs[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_u8, &value },
		{ 0, parse_null, 0 } };
	char strz[64];
	int i;

	sprintf(strz, "C=0");

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specs, &err_index);
	CHECK(0 != status);

	// zero case
	ref = 0;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);

	// iterate over many u8 options
	for (i = 0; i < sizeof(value) * 8; i++)
	{
		ref = (uint8_t)(1 << i);
		sprintf(strz, "C=%u", ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specs, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
	}

	// fullscale case
	ref = 0xff;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);

}

/*
* Test parse_u16 operation.
*/
TEST(ParseTestGroup, TestParse_u16)
{
	uint16_t value;
	uint16_t ref;
	int err_index;
	int status;
	struct parse_param specs[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_u16, &value },
		{ 0, parse_null, 0 } };
	char strz[64];
	int i;

	sprintf(strz, "C=0");

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specs, &err_index);
	CHECK(0 != status);

	// zero case
	ref = 0;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);

	// iterate over many u16 options
	for (i = 0; i < sizeof(value) * 8; i++)
	{
		ref = (uint16_t)(1 << i);
		sprintf(strz, "C=%u", ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specs, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
	}

	// fullscale case
	ref = 0xffff;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
}

/*
* Test parse_u16_04x operation.
*/
TEST(ParseTestGroup, TestParse_u16_04x)
{
	uint16_t value;
	uint16_t ref;
	int err_index;
	int status;
	struct parse_param specs[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_u16_04x, &value },
		{ 0, parse_null, 0 } };
	char strz[64];
	int i;

	sprintf(strz, "C=0");

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specs, &err_index);
	CHECK(0 != status);

	// zero
	ref = 0;
	sprintf(strz, "C=%04x", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);

	// iterate over many u16_04x options
	for (i = 0; i < sizeof(value) * 8; i++)
	{
		ref = (uint16_t)(1 << i);
		sprintf(strz, "C=%04x", ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specs, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
	}

	// fullscale case
	ref = 0xffff;
	sprintf(strz, "C=%04x", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
}

/*
* Test parse_u32 operation.
*/
TEST(ParseTestGroup, TestParse_u32)
{
	uint32_t value;
	uint32_t ref;
	int err_index;
	int status;
	struct parse_param specs[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_u32, &value },
		{ 0, parse_null, 0 } };
	char strz[64];
	int i;

	sprintf(strz, "C=0");

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specs, &err_index);
	CHECK(0 != status);

	// 0 case
	ref = 0;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);

	// iterate over many u32 options
	for (i = 0; i < sizeof(value) * 8; i++)
	{
		ref = (uint32_t)1 << i;
		sprintf(strz, "C=%u", ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specs, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
	}

	// fullscale case
	ref = 0xffffffff;
	sprintf(strz, "C=%u", ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specs, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
}

/*
* Test parse_u64, h64 and c64 operation.
*/
TEST(ParseTestGroup, TestParse_u64)
{
	uint64_t value;
	uint64_t ref;
	uint8_t stream_id[32];
	int err_index;
	int status;
	struct parse_param specsu[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_u64, &value },
		{ 0, parse_null, 0 } };
	struct parse_param specsx[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_h64, &value },
		{ 0, parse_null, 0 } };
	struct parse_param specsc[] = {
		{ (char*)"C" PARSE_ASSIGN, parse_c64, &stream_id },
		{ 0, parse_null, 0 } };
	char strz[64];
	int i, j;

	sprintf(strz, "C=0");

	// error case where strlen() is used instead of sizeof()
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz), specsu, &err_index);
	CHECK(0 != status);

	// 0 case
	ref = 0;
	sprintf(strz, "C=%" SCNu64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsu, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
	sprintf(strz, "C=%" SCNx64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsx, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
	sprintf(strz, "C=%" SCNx64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsc, &err_index);
	CHECK(0 == status);
	for (j = 0; j < 8; j++)
	{
		CHECK(stream_id[j] == (uint8_t)(ref >> (8 * (7 - j))));
	}

	// iterate over many u64, h64 and c64 options
	for (i = 0; i < sizeof(value) * 8; i++)
	{
		ref = (uint64_t)1 << i;
		sprintf(strz, "C=%" SCNu64, ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specsu, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
		sprintf(strz, "C=%" SCNx64, ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specsx, &err_index);
		CHECK(0 == status);
		CHECK(value == ref);
		sprintf(strz, "C=%" SCNx64, ref);
		memset(&value, 0, sizeof(value));
		status = parse(strz, strlen(strz) + 1, specsc, &err_index);
		CHECK(0 == status);
		for (j = 0; j < 8; j++)
		{
			CHECK(stream_id[j] == (uint8_t)(ref >> (8 * (7 - j))));
		}
	}

	// fullscale case
	ref = (uint64_t)-1;
	sprintf(strz, "C=%" SCNu64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsu, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
	sprintf(strz, "C=%" SCNx64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsx, &err_index);
	CHECK(0 == status);
	CHECK(value == ref);
	sprintf(strz, "C=%" SCNx64, ref);
	memset(&value, 0, sizeof(value));
	status = parse(strz, strlen(strz) + 1, specsc, &err_index);
	CHECK(0 == status);
	for (j = 0; j < 8; j++)
	{
		CHECK(stream_id[j] == (uint8_t)(ref >> (8 * (7 - j))));
	}
}
