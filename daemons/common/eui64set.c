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

/** Initialize an eui64set structure using the specified storage buffer.
 */
void eui64set_init( struct eui64set *self, int max_entries, uint64_t *storage )
{
    self->storage = storage;
    self->num_entries=0;
    self->max_entries = max_entries;
}

/** Clear all entries in a eui64set structure.
 */
void eui64set_clear( struct eui64set *self )
{
    self->num_entries=0;
}

/** Test if the eui64set is full.
 *  Returns 1 if the eui64set is full
 *  Returns 0 if the eui64set is not full
 */
int eui64set_is_full( struct eui64set *self )
{
    return self->num_entries >= self->max_entries;
}

/** Insert an eui64 into the eui64set structure, without re-sorting it.
 *  If you have multiple eui64's to add at a time call this
 *  for each eui64 and then sort it once at the end.
 *  Returns 1 on success
 *  Returns 0 if the storage area was full
 */
int eui64set_insert( struct eui64set *self, uint64_t value )
{
    int r=0;
    /* Do we have space? */
    if( self->num_entries < self->max_entries )
    {
        self->storage[ self->num_entries++ ] = value;
        r=1;
    }
    return r;
}

/** Sort a eui64set structure
 */
void eui64set_sort( struct eui64set *self )
{
}

/** Insert a single eui64 into a eui64set structure and sort it afterwards.
 */
int eui64set_insert_and_sort( struct eui64set *self, uint64_t value );

/** Find a eui64 in the eui64set structure. Returns 1 if found, 0 if not.
 */
int eui64set_find( struct eui64set *self, uint64_t value );

/** Remove the specified eui64 from the eui64set structure. Returns 1 if found and removed.
 *  returns 0 if not found.
 */
int eui64set_remove( struct eui64set *self, uint64_t value );

/** Get the selected eui64 item from the structure.  Behaviour is unspecified if
 *  item_num >= num_entries
 */
uint64_t eui64set_get( struct eui64set *self, int item_num );
