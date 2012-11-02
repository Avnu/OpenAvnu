/****************************************************************************
  Copyright (c) 2012, AudioScience, Inc
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
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
#include <inttypes.h>
#else
#define SCNu64       "I64u"
#define SCNx64       "I64x"
#endif

#include "parse.h"

#if 0
#define parse_log(a,b) printf(a,b)
#else
#define parse_log(a,b)
#endif

int parse(char *s, int len, struct parse_param *specs, int *err_index)
{
	int err = 0;
	char *param;
	char *data;
	char *delimiter;
	const char *guard;
	unsigned int v_uint;
	uint64_t v_uint64;
	int result;
	int count = 0;

	/* make sure string is null terminated */
	s[len - 1] = 0;
	guard = s + strlen(s);

	parse_log("PARSE: %s\n", s);

	while (specs->name && !err) {
		param = strstr(s, specs->name);
		if (NULL == param) {
			*err_index = count + 1;
			parse_log("PARSE: ERROR - could not find %s\n",
				  specs->name);
			return -1;
		}
		data = param + strlen(specs->name);

		/* temporarily terminate string at next delimiter */
		delimiter = data;
		while ((*delimiter != PARSE_DELIMITER) && (delimiter < guard))
			delimiter++;
		if (delimiter < guard)
			*delimiter = 0;

		switch (specs->type) {
		case parse_null:
			break;
		case parse_u8:
			result = sscanf(data, "%d", &v_uint);
			if (result == 1) {
				*(uint8_t *) specs->v = (uint8_t) v_uint;
			} else {
				parse_log("PARSE: ERROR - parse_u8 %s\n", data);
			}
			break;
		case parse_u16_04x:
			result = sscanf(data, "%04x", &v_uint);
			if (result == 1) {
				*(uint16_t *) specs->v = (uint16_t) v_uint;
			} else {
				parse_log("PARSE: ERROR - parse_u16_04x %s\n",
					  data);
			}
			break;
		case parse_u16:
			result = sscanf(data, "%d", &v_uint);
			if (result == 1) {
				*(uint16_t *) specs->v = (uint16_t) v_uint;
			} else {
				parse_log("PARSE: ERROR - parse_u16 %s\n",
					  data);
			}
			break;
		case parse_u32:
			result = sscanf(data, "%d", &v_uint);
			if (result == 1) {
				*(uint32_t *) specs->v = v_uint;
			} else {
				parse_log("PARSE: ERROR - parse_u32 %s\n",
					  data);
			}
			break;
		case parse_u64:
			result = sscanf(data, "%" SCNu64, &v_uint64);
			if (result == 1) {
				*(uint64_t *) specs->v = v_uint64;
			} else {
				parse_log("PARSE: ERROR - parse_h64 %s\n",
					  data);
			}
			break;
		case parse_h64:
			result = sscanf(data, "%" SCNx64, &v_uint64);
			if (result == 1) {
				*(uint64_t *) specs->v = v_uint64;
			} else {
				parse_log("PARSE: ERROR - parse_h64 %s\n",
					  data);
			}
			break;
		case parse_c64:
			/* read as uint64_t, then unpack to array */
			result = sscanf(data, "%" SCNx64, &v_uint64);
			if (result == 1) {
				int i;
				uint8_t *p = (uint8_t *) specs->v;
				for (i = 0; i < 8; i++) {
					int shift = (7 - i) * 8;
					p[i] = (uint8_t) (v_uint64 >> shift);
				}
			} else {
				parse_log("PARSE: ERROR - parse_c64 %s\n",
					  data);
			}
			break;
		case parse_mac:
			/* read as uint64_t, then unpack to array */
			result = sscanf(data, "%" SCNx64, &v_uint64);
			if (result == 1) {
				int i;
				uint8_t *p = (uint8_t *) specs->v;
				for (i = 0; i < 6; i++) {
					int shift = (5 - i) * 8;
					p[i] = (uint8_t) (v_uint64 >> shift);
				}
			} else {
				parse_log("PARSE: ERROR - parse_mac %s\n",
					  data);
			}
			break;
		}
		if (result != 1) {
			*err_index = count + 1;
			return -1;
		}

		if (delimiter < guard) {
			*delimiter = PARSE_DELIMITER;
			s = delimiter + 1;
		}
		specs++;
		count++;
	}
	*err_index = 0;
	return 0;
}
