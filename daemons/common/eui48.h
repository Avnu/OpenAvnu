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

/**
 * @brief eui48_init
 *
 * Initialize an EUI48 value with FF:FF:FF:FF:FF:FF
 *
 * @param self pointer to eui48 to initialize
 */
void eui48_init(struct eui48 *self);

/**
 * @brief eui48_zero
 *
 * Set an EUI48 structure's value with 00:00:00:00:00:00
 *
 * @param self pointer to eui48 to set to 0
 */
void eui48_zero(struct eui48 *self);

/**
 * @brief eui48_init_from_uint64
 *
 * Initialize an EUI48 structure's value based on the lower 48 bits of an uint64_t.
 *
 * @param self pointer to eui48
 * @param other uint64_t value
 */
void eui48_init_from_uint64(struct eui48 *self, uint64_t other);

/**
 * @brief eui48_convert_to_uint64
 *
 * Load the eui48 structure's value into the lower 48 bits of an uint64_t
 *
 * @param self pointer to const eui48 struct
 * @return
 */
uint64_t eui48_convert_to_uint64(struct eui48 const *self);

/**
 * @brief eui48_copy
 *
 * Copy an eui48 structure
 *
 * @param self Destination
 * @param other Source
 */
void eui48_copy(struct eui48 *self, struct eui48 const *other);

/**
 * @brief eui48_compare
 *
 * Compare the value of two eui48 structures
 *
 * @param self Left hand side value
 * @param other Right hand side value
 * @return -1 if LHS < RHS, 0 if LHS == RHS, 1 if LHS > RHS
 */
int eui48_compare(struct eui48 const *self,
                struct eui48 const *other);

int eui48_is_unset(struct eui48 v);

int eui48_is_set(struct eui48 v);

int eui48_is_zero(struct eui48 v);

int eui48_is_not_zero(struct eui48 v);

/*@}*/
#endif

