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

#include <stdlib.h>
#include "eui64set.h"

uint64_t eui64_read(const uint8_t network_order_buf[8])
{
	uint64_t v = 0;

	v = (((uint64_t) network_order_buf[0]) << 56)
	    + (((uint64_t) network_order_buf[1]) << 48)
	    + (((uint64_t) network_order_buf[2]) << 40)
	    + (((uint64_t) network_order_buf[3]) << 32)
	    + (((uint64_t) network_order_buf[4]) << 24)
	    + (((uint64_t) network_order_buf[5]) << 16)
	    + (((uint64_t) network_order_buf[6]) << 8)
	    + (((uint64_t) network_order_buf[7]) << 0);

	return v;
}

void eui64_write(uint8_t network_order_buf[8], uint64_t v)
{
	network_order_buf[0] = (uint8_t) ((v >> 56) & 0xff);
	network_order_buf[1] = (uint8_t) ((v >> 48) & 0xff);
	network_order_buf[2] = (uint8_t) ((v >> 40) & 0xff);
	network_order_buf[3] = (uint8_t) ((v >> 32) & 0xff);
	network_order_buf[4] = (uint8_t) ((v >> 24) & 0xff);
	network_order_buf[5] = (uint8_t) ((v >> 16) & 0xff);
	network_order_buf[6] = (uint8_t) ((v >> 8) & 0xff);
	network_order_buf[7] = (uint8_t) ((v >> 0) & 0xff);
}

int eui64set_compare(const void *lhs, const void *rhs)
{
	int r = 0;
	const struct eui64set_entry *lhsv = (const struct eui64set_entry *)lhs;
	const struct eui64set_entry *rhsv = (const struct eui64set_entry *)rhs;

	if (lhsv->eui64 < rhsv->eui64) {
		r = -1;
	} else if (lhsv->eui64 > rhsv->eui64) {
		r = 1;
	}
	return r;
}

int eui64set_init(struct eui64set *self, int max_entries)
{
	int r = 0;
	self->num_entries = 0;
	self->max_entries = max_entries;
	self->storage = 0;
	/* Are we to allocate storage? */
	if (max_entries > 0) {
		/* Yes, try */
		self->storage =
		    (struct eui64set_entry *)
		    calloc(sizeof(struct eui64set_entry), max_entries);
		if (self->storage == 0) {
			/* Failure to allocate storage */
			r = -1;
		}
	}
	return r;
}

void eui64set_free(struct eui64set *self)
{
	if (self) {
		if (self->storage) {
			free(self->storage);
		}
	}
}

void eui64set_clear(struct eui64set *self)
{
	self->num_entries = 0;
}

int eui64set_num_entries(struct eui64set *self)
{
	return self->num_entries;
}

/** Test if the eui64set is full.
 *  Returns 1 if the eui64set is full
 *  Returns 0 if the eui64set is not full
 */
int eui64set_is_full(struct eui64set *self)
{
	return self->num_entries >= self->max_entries;
}

int eui64set_insert(struct eui64set *self, uint64_t value, void *p)
{
	int r = 0;
	/* Do we have space? */
	if (self->num_entries < self->max_entries) {
		self->storage[self->num_entries].eui64 = value;
		self->storage[self->num_entries].p = p;
		++self->num_entries;
		r = 1;
	}
	return r;
}

void eui64set_sort(struct eui64set *self)
{
	qsort(self->storage,
	      self->num_entries,
	      sizeof(struct eui64set_entry), eui64set_compare);
}

int eui64set_insert_and_sort(struct eui64set *self, uint64_t value, void *p)
{
	int r;

	r = eui64set_insert(self, value, p);
	if (1 == r) {
		eui64set_sort(self);
	}
	return r;
}

const struct eui64set_entry *eui64set_find(const struct eui64set *self,
					   uint64_t value)
{
	const struct eui64set_entry *result;
	struct eui64set_entry key;
	key.eui64 = value;
	key.p = 0;
	result = bsearch(&key,
			 self->storage,
			 self->num_entries, sizeof(key), eui64set_compare);
	return result;
}

int eui64set_remove_and_sort(struct eui64set *self, uint64_t value)
{
	int r = 0;
	struct eui64set_entry *item;
	struct eui64set_entry key;
	key.eui64 = value;
	key.p = 0;
	item = bsearch(&key,
		       self->storage,
		       self->num_entries, sizeof(key), eui64set_compare);
	if (item) {
		// Set value to highest value
		item->eui64 = ~((uint64_t) 0);
		if (item->p) {
			free(item->p);
		}

		eui64set_sort(self);
		--self->num_entries;
		r = 1;
	}
	return r;
}
