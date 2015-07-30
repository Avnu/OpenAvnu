/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : Interface for a basic doubly link list abstraction
* - Nodes can be added to the end of the list.
* - Nodes can be removed at any location in the list.
*/

#ifndef OPENAVB_LIST_H
#define OPENAVB_LIST_H 1

#include "openavb_types.h"

typedef struct openavb_list_node * openavb_list_node_t;
typedef struct openavb_list * openavb_list_t;

// Create a link list. Returns NULL on failure
openavb_list_t openavbListNewList(void);

// Delete a link list.
void openavbListDeleteList(openavb_list_t list);

// Add a data element as a node to a link list. Returns NULL on failure.
openavb_list_node_t openavbListAdd(openavb_list_t list, void *data);

// Allocate and manage data element and add as a node to a link list. Returns NULL on failure. 
openavb_list_node_t openavbListNew(openavb_list_t list, U32 size);

// Remove (delete) node. Returns NULL on failure otherwise next node. 
openavb_list_node_t openavbListDelete(openavb_list_t list, openavb_list_node_t node);

// Gets the next node. Returns FALSE on error or if already at the tail.
openavb_list_node_t openavbListNext(openavb_list_t list, openavb_list_node_t node);

// Gets the previous node. Returns FALSE on error or if already at the head.
openavb_list_node_t openavbListPrev(openavb_list_t list, openavb_list_node_t node);

// Gets the first (head) node. Returns FALSE on error or empty list.
openavb_list_node_t openavbListFirst(openavb_list_t list);

// Gets the lastt (tail) node. Returns FALSE on error or empty list.
openavb_list_node_t openavbListLast(openavb_list_t list);

// Gets the first node and preps for iteration. Returns FALSE on error or empty list.
openavb_list_node_t openavbListIterFirst(openavb_list_t list);

// Gets the last node and preps for iteration. Returns FALSE on error or empty list.
openavb_list_node_t openavbListIterLast(openavb_list_t list);

// Gets the next node in the iteration. Returns FALSE on error or end of list.
openavb_list_node_t openavbListIterNext(openavb_list_t list);

// Gets the prev node in the iteration. Returns FALSE on error or beginning of list.
openavb_list_node_t openavbListIterPrev(openavb_list_t list);

// Get data element. Returns NULL on failure.
void *openavbListData(openavb_list_node_t node);

// Returns TRUE is node is the head.
bool openavbListIsFirst(openavb_list_t list, openavb_list_node_t node);

// Returns TRUE is node is the tail.
bool openavbListIsLast(openavb_list_t list, openavb_list_node_t node);

// Returns TRUE is node data element memory is being managed by the list.
bool openavbListIsManaged(openavb_list_node_t node);

// Dump the contents of the list to the console. Debugging purposes only.
void openavbListDump(openavb_list_t list, U32 dataSize);

#endif // OPENAVB_LIST_H
