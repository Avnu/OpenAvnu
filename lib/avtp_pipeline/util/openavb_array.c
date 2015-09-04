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
* MODULE SUMMARY : Implementation for a basic dynamic array abstraction.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "openavb_debug.h"
#include "openavb_array.h"

OPENAVB_CODE_MODULE_PRI

struct openavb_array_elem {
    void *data;
    bool managed;
	S32 idx;
};

struct openavb_array {
	U32 elemSize;
	S32 size;
	S32 count;
	S32 iterIdx;
	openavb_array_elem_t elemArray;
};

openavb_array_t openavbArrayNewArray(U32 size)
{
	openavb_array_t retArray = calloc(1, sizeof(struct openavb_array));
	if (retArray) {
		retArray->elemSize = size;
		retArray->size = 0;
		retArray->count = 0;
		retArray->iterIdx = 0;
	}

	return retArray;
}

bool openavbArraySetInitSize(openavb_array_t array, U32 size)
{
	if (array) {
		if (!array->size) {
			array->elemArray = calloc(size, sizeof(struct openavb_array_elem));
			if (array->elemArray) {
				array->size = size;

				// Initialize each elem				
				S32 i1;
				for (i1 = 0; i1 < array->size; i1++) {
					array->elemArray[i1].data = NULL;
					array->elemArray[i1].managed = FALSE;
					array->elemArray[i1].idx = i1;
				}
				return TRUE;
			}
		}
	}
	return FALSE;
}

void openavbArrayDeleteArray(openavb_array_t array)
{
	if (array && array->elemArray) {
		openavb_array_elem_t elem;
		elem = openavbArrayIterFirst(array);
		while (elem) {
			openavbArrayDelete(array, elem);
			elem = openavbArrayIterNext(array);
		}
		free(array->elemArray);
		array->elemArray = NULL;
		free(array);
	}
}

openavb_array_elem_t openavbArrayAdd(openavb_array_t array, void *data)
{
	openavb_array_elem_t retElem = NULL;

	if (array && data) {
		if (array->elemArray && (array->count < array->size)) {
			// Find the empty slot
			S32 i1;
			for (i1 = 0; i1 < array->size; i1++) {
				if (!array->elemArray[i1].data) {
					retElem = &array->elemArray[i1];
					break;
				}
			}
		}
		else {
			// Need to make room for new element
			openavb_array_elem_t newElemArray = realloc(array->elemArray, (array->size + 1) * sizeof(struct openavb_array_elem));
			if (newElemArray) {
				array->elemArray = newElemArray;
				retElem = &array->elemArray[array->size];
				retElem->data = NULL;
				retElem->managed = FALSE;
				retElem->idx = array->size;
				array->size++;
			}
		}

		if (retElem) {
			retElem->data = data;
			array->count++;
		}
	}

	return retElem;
}

openavb_array_elem_t openavbArrayNew(openavb_array_t array)
{
	openavb_array_elem_t retElem = NULL;

	if (array) {
		void *data = calloc(1, array->elemSize);
		if (data) {
			retElem = openavbArrayAdd(array, data);
			if (retElem) {
				retElem->managed = TRUE;   
			} else {
				free(data);
			}
		}
	}

	return retElem;
}

bool openavbArrayMultiNew(openavb_array_t array, S32 count)
{
	int i1;
	for (i1 = 0; i1 < count; i1++) {
		if (!openavbArrayNew(array))
			return FALSE;
	}
	return TRUE;
}

void openavbArrayDelete(openavb_array_t array, openavb_array_elem_t elem)
{
	if (array && elem) {
		if (elem->managed) {
			free(elem->data);
		}
		elem->data = NULL;
		array->count--;
	}
}

openavb_array_elem_t openavbArrayIdx(openavb_array_t array, S32 idx)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray && idx >= 0 && idx < array->size) {
		if (array->elemArray[idx].data) {
			retElem = &array->elemArray[idx];
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterFirst(openavb_array_t array)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = 0; i1 < array->size; i1++) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				array->iterIdx = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterLast(openavb_array_t array)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = array->size - 1; i1 >= 0; i1--) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				array->iterIdx = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterNext(openavb_array_t array)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = array->iterIdx + 1; i1 < array->size; i1++) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				array->iterIdx = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterPrev(openavb_array_t array)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = array->iterIdx - 1; i1 >= 0; i1--) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				array->iterIdx = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterFirstAlt(openavb_array_t array, U32 *iter)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = 0; i1 < array->size; i1++) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				*iter = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterLastAlt(openavb_array_t array, U32 *iter)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = array->size - 1; i1 >= 0; i1--) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				*iter = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterNextAlt(openavb_array_t array, U32 *iter)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = *iter + 1; i1 < array->size; i1++) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				*iter = i1;
				break;
			}
		}
	}
	return retElem;
}

openavb_array_elem_t openavbArrayIterPrevAlt(openavb_array_t array, U32 *iter)
{
	openavb_array_elem_t retElem = NULL;

	if (array && array->elemArray) {
		S32 i1;
		for (i1 = *iter - 1; i1 >= 0; i1--) {
			if (array->elemArray[i1].data) {
				retElem = &array->elemArray[i1];
				*iter = i1;
				break;
			}
		}
	}
	return retElem;
}

void *openavbArrayData(openavb_array_elem_t elem)
{
	if (elem) {
		return elem->data;
	}
	return NULL;
}

void *openavbArrayDataIdx(openavb_array_t array, S32 idx)
{
	if (array) {
		openavb_array_elem_t elem = openavbArrayIdx(array, idx);

		if (elem) {
			return elem->data;
		}
	}
	return NULL;
}

void *openavbArrayDataNew(openavb_array_t array)
{
	if (array) {
		openavb_array_elem_t elem = openavbArrayNew(array);

		if (elem) {
			return elem->data;
		}
	}
	return NULL;
}

S32 openavbArrayFindData(openavb_array_t array, void *data)
{
	S32 retIdx = -1;
	if (array && data) {
		S32 i1;
		for (i1 = 0; i1 < array->size; i1++) {
			if (array->elemArray[i1].data == data) {
				retIdx = i1;
				break;
			}
		}
	}
	return retIdx;
}


S32 openavbArraySize(openavb_array_t array)
{
	if (array) {
		return array->size;
	}
	return 0;
}

S32 openavbArrayCount(openavb_array_t array)
{
	if (array) {
		return array->count;
	}
	return 0;
}

S32 openavbArrayGetIdx(openavb_array_elem_t elem)
{
	if (elem) {
		return elem->idx;
	}
	return -1;
}

bool openavbArrayIsManaged(openavb_array_elem_t elem)
{
	if (elem) {
		if (elem->managed) {
			return TRUE;
		}
	}
	return FALSE;
}

void openavbArrayDump(openavb_array_t array)
{
	printf("TEST DUMP: Array contents\n");
	if (array) {
		printf("array:                 %p\n", array);
		printf("elemSize:              %u\n", (unsigned int)(array->elemSize));
		printf("size:                  %d\n", (int)(array->size));
		printf("count:                 %d\n", (int)(array->count));
		printf("iterIdx:               %d\n", (int)(array->iterIdx));
		printf("elemArray              %p\n", array->elemArray);

		if (array->elemArray) {
			S32 i1;
			for (i1 = 0; i1 < array->size; i1++) {
				printf("elemArray[%02d]          %p\n", (int)i1, &array->elemArray[i1]);
				printf("elemArray[%02d].data     %p\n", (int)i1, array->elemArray[i1].data);
				printf("elemArray[%02d].managed  %d\n", (int)i1, array->elemArray[i1].managed);
				printf("elemArray[%02d].idx      %d\n", (int)i1, (int)(array->elemArray[i1].idx));
				openavbDebugDumpBuf(array->elemArray[i1].data, array->elemSize);
			}
		}

		return;
	}
	printf("Invalid array pointer\n");
}


