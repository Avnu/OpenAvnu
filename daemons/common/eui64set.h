/****************************************************************************
  Copyright (c) 2014, J.D. Koftinoff Software, Ltd.
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

#ifndef EUI64SET_H_
#define EUI64SET_H_

#include <stdint.h>

/**
 * The eui64set_entry struct
 */
struct eui64set_entry {
	uint64_t eui64;
	void *p;
};

/**
 * Compare two eui64 values: lhs and rhs
 * Returns -1 if lhs < rhs
 * Returns 0 if lhs == rhs
 * Returns 1 if lhs > rhs
 */
int eui64set_compare(const void *lhs, const void *rhs);

/**
 * eui64_read
 * Read a eui64 in network order
 * Returns uint64_t
 */
uint64_t eui64_read(const uint8_t network_order_buf[8]);

/**
 * eui64_write
 * Converts an eui64 in uint64_t into network byte order
 */
void eui64_write(uint8_t network_order_buf[8], uint64_t v);

struct eui64set {
	/** The memory storage for this set */
	struct eui64set_entry *storage;

	/** The current number of entries in this set */
	int num_entries;

	/** The maximum number of entries in this set */
	int max_entries;
};

/**
 * Initialize an eui64set structure using the specified storage buffer.
 * Returns -1 on error, 0 on success
 */
int eui64set_init(struct eui64set *self, int max_entries);

/**
 * Free memory allocated for the eui64set
 */
void eui64set_free(struct eui64set *self);

/**
 * Clear all entries in a eui64set structure.
 */
void eui64set_clear(struct eui64set *self);

/**
 * Return the number of entries in a eui64set structure.
 */
int eui64set_num_entries(struct eui64set *self);

/**
 * Test if the eui64set is full.
 * Returns 1 if the eui64set is full
 * Returns 0 if the eui64set is not full
 */
int eui64set_is_full(struct eui64set *self);

/**
 * Insert an eui64 into the eui64set structure, without re-sorting it.
 * If you have multiple eui64's to add at a time call this
 * for each eui64 and then sort it once at the end.
 * Returns 1 on success
 * Returns 0 if the storage area was full
 */
int eui64set_insert(struct eui64set *self, uint64_t value, void *p);

/**
 * Sort a eui64set structure
 */
void eui64set_sort(struct eui64set *self);

/**
 * Insert a single eui64 into a eui64set structure and sort it afterwards.
 * Returns 1 on success
 * Returns 0 if the storage area was full
 */
int eui64set_insert_and_sort(struct eui64set *self, uint64_t value, void *p);

/**
 * Find a eui64 in the eui64set structure. Returns a pointer to the
 * eui64set_entry, or 0 if not found.
 */
const struct eui64set_entry *eui64set_find(const struct eui64set *self,
					   uint64_t value);

/**
 * Remove the specified eui64 from the eui64set structure and frees any
 * associated p data
 *
 * Returns 1 if found and removed.
 *
 * Returns 0 if not found.
 */
int eui64set_remove_and_sort(struct eui64set *self, uint64_t value);

#endif				/* EUI64SET_H_ */
