/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
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
* HEADER SUMMARY : Support for ACMP saved state
*
* Defines the saved state data, and the functions
* to read and write the saved state data.
*/

#ifndef AVB_ENDPOINT_AVDECC_SAVE_STATE_H
#define AVB_ENDPOINT_AVDECC_SAVE_STATE_H

#include "openavb_types.h"
#include "openavb_avdecc_pub.h"
#include "openavb_tl_pub.h"

#define DEFAULT_AVDECC_SAVE_INI_FILE "avdecc_save.ini"

struct openavb_saved_state {
	char listener_friendly_name[FRIENDLY_NAME_SIZE];
	U16 flags;
	U16 talker_unique_id;
	U8 talker_entity_id[8];
	U8 controller_entity_id[8];
};
typedef struct openavb_saved_state openavb_saved_state_t;

// Returns a pointer to the saved state information for the index supplied.
// To use, start at index 0 and increment upward.
// If NULL is returned, then no values for that or any higher indexes.
const openavb_saved_state_t * openavbAvdeccGetSavedState(int index);

// Add a saved state to the list of saved states.
// Returns the index for the new saved state, or -1 if an error occurred.
int openavbAvdeccAddSavedState(const char listener_friendly_name[FRIENDLY_NAME_SIZE], U16 flags, U16 talker_unique_id, const U8 talker_entity_id[8], const U8 controller_entity_id[8]);

// Delete the saved state information at the specified index.
// Returns TRUE if successfully deleted, FALSE otherwise.
bool openavbAvdeccDeleteSavedState(int index);

#endif // AVB_ENDPOINT_AVDECC_SAVE_STATE_H
