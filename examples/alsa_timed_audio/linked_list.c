/******************************************************************************

  Copyright (c) 2018, Intel Corporation
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

#include <linked_list.h>

#include <stdlib.h>

struct linked_list_element {
	struct linked_list_element **addr;
	struct linked_list_element *next;
};

struct linked_list {
	struct linked_list_element *head;
};

struct linked_list *ll_init()
{
	return NULL;
}

struct linked_list *ll_alloc()
{
	struct linked_list *ret;
	ret = (__typeof__(ret)) malloc((size_t) sizeof(*ret));
	ret->head = NULL;
	return ret;
}

bool ll_is_valid( struct linked_list *list )
{
	return list != NULL;
}

bool
ll_add_head( struct linked_list *list, struct linked_list_element **element )
{
	if( *element == NULL )
	{
		*element = (__typeof__(*element))
			malloc((size_t) sizeof(**element));
		if( *element == NULL )
			return false;
	}

	(*element)->next = list->head;
	(*element)->addr = element;
	list->head = *element;

	return true;
}

struct linked_list_element *
ll_get_head( struct linked_list *list )
{
	if( list != NULL )
		return list->head;
	return NULL;
}

void
ll_remove_head( struct linked_list *list )
{
	list->head = list->head->next;
}

void
ll_free_element( struct linked_list_element **element )
{
	free( *element );
	*element = NULL;
}

void
ll_init_element( struct linked_list_element **element )
{
	*element = NULL;
}

struct linked_list_element **
ll_get_addr( struct linked_list_element *element )
{
	return element->addr;
}

void
ll_clean( struct linked_list **list )
{
	struct linked_list *iter = *list;
	struct linked_list_element *tmp;

	if( !ll_is_valid( iter ))
		return;

	while(( tmp = ll_get_head( iter )) != NULL )
	{
		ll_remove_head( iter );
		ll_free_element( ll_get_addr( tmp ));
	}

	free( iter );
}
