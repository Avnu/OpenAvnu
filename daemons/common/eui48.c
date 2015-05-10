


void pdu_eui48_init(pdu_eui48 *self)
{
    size_t i;
    for (i = 0; i < sizeof(self->value); ++i) {
        self->value[i] = 0xff;
    }
}


void pdu_eui48_zero(pdu_eui48 *self)
{
    size_t i;
    for (i = 0; i < sizeof(self->value); ++i) {
        self->value[i] = 0x00;
    }
}


void pdu_eui48_init_from_uint64(pdu_eui48 *self, uint64_t other)
{
    self->value[0] = (uint8_t) ((other >> (5 * 8)) & 0xff);
    self->value[1] = (uint8_t) ((other >> (4 * 8)) & 0xff);
    self->value[2] = (uint8_t) ((other >> (3 * 8)) & 0xff);
    self->value[3] = (uint8_t) ((other >> (2 * 8)) & 0xff);
    self->value[4] = (uint8_t) ((other >> (1 * 8)) & 0xff);
    self->value[5] = (uint8_t) ((other >> (0 * 8)) & 0xff);
}


uint64_t pdu_eui48_convert_to_uint64(const pdu_eui48 *self)
{
    uint64_t v = 0;
    v |= ((uint64_t) self->value[0]) << (5 * 8);
    v |= ((uint64_t) self->value[1]) << (4 * 8);
    v |= ((uint64_t) self->value[2]) << (3 * 8);
    v |= ((uint64_t) self->value[3]) << (2 * 8);
    v |= ((uint64_t) self->value[4]) << (1 * 8);
    v |= ((uint64_t) self->value[5]) << (0 * 8);
    return v;
}


void pdu_eui48_copy(pdu_eui48 *self, const pdu_eui48 *other)
{
    size_t i;
    for (i = 0; i < sizeof(self->value); ++i) {
        self->value[i] = other->value[i];
    }
}


int pdu_eui48_compare(const pdu_eui48 *self, const pdu_eui48 *other)
{
    int r = memcmp(self->value, other->value, sizeof(self->value));
    return r;
}


ssize_t pdu_eui48_read(pdu_eui48 *host_value, const void *base, ssize_t pos, size_t len)
{
    ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
    if (r >= 0) {
        memcpy(host_value->value, ((uint8_t const *)base) + pos,
               sizeof(host_value->value));
    }
    return r;
}


pdu_eui48 pdu_eui48_get(const void *base, ssize_t pos)
{
    struct pdu_eui48 v;
    memcpy(v.value, ((uint8_t const *)base) + pos, sizeof(v.value));
    return v;
}
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



#include "eui48.h"

ssize_t pdu_eui48_write(const pdu_eui48 *host_value, void *base, ssize_t pos, size_t len)
{
    ssize_t r = pdu_validate_range(pos, len, sizeof(host_value->value));
    if (r >= 0) {
        memcpy(((uint8_t *) base) + pos, host_value->value,
               sizeof(host_value->value));
    }
    return r;
}


void pdu_eui48_set(pdu_eui48 v, void *base, ssize_t pos)
{
    memcpy(((uint8_t *) base) + pos, v.value, sizeof(v.value));
}


int pdu_eui48_is_unset(pdu_eui48 v)
{
    int r = 0;
    if (v.value[0] == 0xff && v.value[1] == 0xff && v.value[2] == 0xff
            && v.value[3] == 0xff && v.value[4] == 0xff && v.value[5] == 0xff) {
        r = 1;
    }
    return r;
}


int pdu_eui48_is_set(pdu_eui48 v)
{
    int r = 0;
    if (v.value[0] != 0xff || v.value[1] != 0xff || v.value[2] != 0xff
            || v.value[3] != 0xff || v.value[4] != 0xff || v.value[5] != 0xff) {
        r = 1;
    }
    return r;
}


int pdu_eui48_is_zero(pdu_eui48 v)
{
    int r = 0;
    if (v.value[0] == 0x00 && v.value[1] == 0x00 && v.value[2] == 0x00
            && v.value[3] == 0x00 && v.value[4] == 0x00 && v.value[5] == 0x00) {
        r = 1;
    }
    return r;
}


int pdu_eui48_is_not_zero(pdu_eui48 v)
{
    int r = 0;
    if (v.value[0] != 0x00 || v.value[1] != 0x00 || v.value[2] != 0x00
            || v.value[3] != 0x00 || v.value[4] != 0x00 || v.value[5] != 0x00) {
        r = 1;
    }
    return r;
}
