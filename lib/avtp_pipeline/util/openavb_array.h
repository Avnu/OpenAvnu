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
* MODULE SUMMARY : Interface for a basic dynamic array abstraction.
* 
* - Array dynamically grows as needed.
* - Elements can be removed. This leaves an empty slot.
* - Elements can not be moved once placed in the array.
*/

#ifndef OPENAVB_ARRAY_H
#define OPENAVB_ARRAY_H 1

#include "openavb_types.h"

typedef struct openavb_array_elem * openavb_array_elem_t;
typedef struct openavb_array * openavb_array_t;

// Create an array. Returns NULL on failure.
openavb_array_t openavbArrayNewArray(U32 elemSize);

// Sets the initial number of array slots. Can only be used before adding elements to the array.
bool openavbArraySetInitSize(openavb_array_t array, U32 size);

// Delete an array.
void openavbArrayDeleteArray(openavb_array_t array);

// Add a data element to the array. Returns NULL on failure.
openavb_array_elem_t openavbArrayAdd(openavb_array_t array, void *data);

// Allocate and manage data element and add to the array. Returns NULL on failure. 
openavb_array_elem_t openavbArrayNew(openavb_array_t array);

// Allocate and manage multiple data element and add to the array. Returns FALSE on failure. 
bool openavbArrayMultiNew(openavb_array_t array, S32 count);

// Remove (delete) element. Will result in an empty slot. 
void openavbArrayDelete(openavb_array_t array, openavb_array_elem_t elem);

// Gets the element at index. Returns FALSE on error.
openavb_array_elem_t openavbArrayIdx(openavb_array_t array, S32 idx);

// Gets the first element. Returns FALSE on error or empty array.
openavb_array_elem_t openavbArrayIterFirst(openavb_array_t array);

// Gets the last element. Returns FALSE on error or empty array.
openavb_array_elem_t openavbArrayIterLast(openavb_array_t array);

// Gets the next element. Returns FALSE on error or last element.
openavb_array_elem_t openavbArrayIterNext(openavb_array_t array);

// Gets the previous element. Returns FALSE on error or first element.
openavb_array_elem_t openavbArrayIterPrev(openavb_array_t array);

// Gets the first element. Returns FALSE on error or empty array. Alternate version with iter passed in for possible multi-threaded use.
openavb_array_elem_t openavbArrayIterFirstAlt(openavb_array_t array, U32 *iter);

// Gets the last element. Returns FALSE on error or empty array. Alternate version with iter passed in for possible multi-threaded use.
openavb_array_elem_t openavbArrayIterLastAlt(openavb_array_t array, U32 *iter);

// Gets the next element. Returns FALSE on error or last element. Alternate version with iter passed in for possible multi-threaded use.
openavb_array_elem_t openavbArrayIterNextAlt(openavb_array_t array, U32 *iter);

// Gets the previous element. Returns FALSE on error or first element. Alternate version with iter passed in for possible multi-threaded use.
openavb_array_elem_t openavbArrayIterPrevAlt(openavb_array_t array, U32 *iter);

// Get data of the element. Returns NULL on failure.
void *openavbArrayData(openavb_array_elem_t elem);

// Get data of the element at the Idx. Returns NULL on failure.
void *openavbArrayDataIdx(openavb_array_t array, S32 idx);

// Get data of a new element. Returns NULL on failure.
void *openavbArrayDataNew(openavb_array_t array);

// Find the index of the element data. Returns -1 if not found.
S32 openavbArrayFindData(openavb_array_t array, void *data);

// Get the size of the array. Allocated element slots
S32 openavbArraySize(openavb_array_t array);

// Get the count of the array. Valid elements in the array.
S32 openavbArrayCount(openavb_array_t array);

// Returns the index of the array element. -1 on error.
S32 openavbArrayGetIdx(openavb_array_elem_t elem);

// Returns TRUE is node data element memory is being managed by the list.
bool openavbArrayIsManaged(openavb_array_elem_t elem);

// Dump array details to console
void openavbArrayDump(openavb_array_t array);

#endif // OPENAVB_ARRAY_H
