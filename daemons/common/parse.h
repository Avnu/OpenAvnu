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

#ifndef PARSE_H_
#define PARSE_H_

enum parse_types {
	parse_null,
	parse_u8,		/* uint8_t v */
	parse_u16,		/* uint16_t v */
	parse_u16_04x,		/* uint16_t v as 0002 (hex string of 4 digits) */
	parse_u32,		/* uint32_t v */
	parse_u64,		/* uint64_t v */
	parse_h64,		/* uint64_t v */
	parse_c64,		/* uint8_t a[8] */
	parse_mac		/* uint8_t m[6] */
};

#define PARSE_DELIMITER ','
#define PARSE_ASSIGN "="

struct parse_param {
	char *name;
	enum parse_types type;
	void *v;
};

int parse(char *s, int len, struct parse_param *specs, int *err_index);

#endif				/* PARSE_H_ */
