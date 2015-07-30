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
* MODULE SUMMARY : Implementation for a basic doubly link list abstraction
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openavb_debug.h"
#include "openavb_list.h"

OPENAVB_CODE_MODULE_PRI

struct openavb_list_node {
    void *data;
    bool managed;
    openavb_list_node_t prev;
    openavb_list_node_t next;    
};

struct openavb_list {
    openavb_list_node_t head;
    openavb_list_node_t tail;
	openavb_list_node_t iter;
};

openavb_list_t openavbListNewList()
{
	return calloc(1, sizeof(struct openavb_list));
}

void openavbListDeleteList(openavb_list_t list)
{
    if (list) {
		openavb_list_node_t node;
		while ((node = openavbListFirst(list))) {
			openavbListDelete(list, node);
		}
		free(list);
    }
}

openavb_list_node_t openavbListAdd(openavb_list_t list, void *data)
{
    openavb_list_node_t retNode = NULL;
    if (list) {
        retNode = calloc(1, sizeof(struct openavb_list_node));
        if (retNode) {
            retNode->data = data;
            retNode->prev = NULL;
            if (!list->head) {
                list->head = retNode;
                list->tail = retNode;
            }
            else {
                retNode->prev = list->tail;
                list->tail->next = retNode;
                list->tail = retNode;
            }
        }
    }
    return retNode;
}

openavb_list_node_t openavbListNew(openavb_list_t list, U32 size)
{
    openavb_list_node_t retNode = NULL;
    if (list && size) {
        void *data = calloc(1, size);
        if (data) {
            retNode = openavbListAdd(list, data);
            if (retNode) {
                retNode->managed = TRUE;   
            } else {
                free(data);
            }
        }
    }
    
    return retNode;
}

openavb_list_node_t openavbListDelete(openavb_list_t list, openavb_list_node_t node)
{
    openavb_list_node_t retNode = NULL;
	if (list && node) {
		// Update head and tail if needed
		if (node == list->head) {
			list->head = node->next;
		}
		if (node == list->tail) {
			list->tail = node->prev;
		}

		// Unchain ourselves
		if (node->prev) {
			node->prev->next = node->next;
		}
		if (node->next) {
			node->next->prev = node->prev;
			retNode = node->next;
		}

		// Free managed memory
		if (node->managed && node->data) {
			free(node->data);
			node->data = NULL;
		}
		free(node);
	}
	return retNode;
}

openavb_list_node_t openavbListNext(openavb_list_t list, openavb_list_node_t node)
{
	openavb_list_node_t retNode = NULL;
	if (list && node) {
		retNode = node->next;
	}
	return retNode;
}

openavb_list_node_t openavbListPrev(openavb_list_t list, openavb_list_node_t node)
{
	openavb_list_node_t retNode = NULL;
	if (list && node) {
		retNode = node->prev;
	}
	return retNode;
}

openavb_list_node_t openavbListFirst(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list) {
		return list->head;
	}
	return retNode;
}

openavb_list_node_t openavbListLast(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list) {
		return list->tail;
	}
	return retNode;
}

openavb_list_node_t openavbListIterFirst(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list) {
		list->iter = list->head;
		retNode = list->iter;
	}
	return retNode;
}


openavb_list_node_t openavbListIterLast(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list) {
		list->iter = list->tail;
		retNode = list->iter;
	}
	return retNode;
}

openavb_list_node_t openavbListIterNext(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list && list->iter) {
		list->iter = list->iter->next;
		retNode = list->iter;
	}
	return retNode;
}


openavb_list_node_t openavbListIterPrev(openavb_list_t list)
{
	openavb_list_node_t retNode = NULL;
	if (list && list->iter) {
		list->iter = list->iter->prev;
		retNode = list->iter;
	}
	return retNode;
}

void *openavbListData(openavb_list_node_t node)
{
	if (node) {
		return node->data;
	}
	return NULL;
}

bool openavbListIsFirst(openavb_list_t list, openavb_list_node_t node)
{
	if (list && node) {
		if (list->head == node) {
			return TRUE;
		}
	}
	return FALSE;
}

bool openavbListIsLast(openavb_list_t list, openavb_list_node_t node)
{
	if (list && node) {
		if (list->tail == node) {
			return TRUE;
		}
	}
	return FALSE;
}

bool openavbListIsManaged(openavb_list_node_t node)
{
	if (node) {
		if (node->managed) {
			return TRUE;
		}
	}
	return FALSE;
}

void openavbListDump(openavb_list_t list, U32 dataSize)
{
	U32 count = 0;

	printf("TEST DUMP: List contents\n");
	if (list) {

		printf("listHead            %p\n", list->head);
		printf("listTail            %p\n", list->tail);
		printf("listIter            %p\n", list->iter);

		openavb_list_node_t node = openavbListFirst(list);
		while (node) {
			count++;
			printf("listNode Count      %i\n", (int)count);
			printf("listNodeData        %p\n", node->data);
			printf("listNodeManaged     %i\n", node->managed);
			printf("listNodePrev        %p\n", node->prev);
			printf("listNodeNext        %p\n", node->next);
			openavbDebugDumpBuf(node->data, dataSize);
			node = openavbListNext(list, node);
		}

		return;
	}
	printf("Invalid list pointer\n");
}




