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

#ifndef EUI48_H_
#define EUI48_H_

#include <stdint.h>
#include <stdbool.h>
#include <memory.h>

/** \addtogroup eui48 eui48 */
/*@{*/

struct eui48 {
    uint8_t value[6];
};

void eui48_init(struct eui48 *self);

void eui48_zero(struct eui48 *self);

void eui48_init_from_uint64(struct eui48 *self, uint64_t other);

uint64_t eui48_convert_to_uint64(struct eui48 const *self);

void eui48_copy(struct eui48 *self, struct eui48 const *other);

int eui48_compare(struct eui48 const *self,
                struct eui48 const *other);

ssize_t eui48_read(struct eui48 *host_value, void const *base,
                 ssize_t pos, size_t len);

struct eui48 eui48_get(void const *base, ssize_t pos);

ssize_t eui48_write(struct eui48 const *host_value, void *base,
                  ssize_t pos, size_t len);

void eui48_set(struct eui48 v, void *base, ssize_t pos);

int eui48_is_unset(struct eui48 v);

int eui48_is_set(struct eui48 v);

int eui48_is_zero(struct eui48 v);

int eui48_is_not_zero(struct eui48 v);

/*@}*/
#endif

