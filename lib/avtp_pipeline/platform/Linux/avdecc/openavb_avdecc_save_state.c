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
* MODULE SUMMARY : Support for ACMP saved state
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "openavb_avdecc_cfg.h"
#include "openavb_avdecc_save_state.h"
#include "openavb_trace.h"

#define	AVB_LOG_COMPONENT	"AVDECC Cfg"
#include "openavb_log.h"

#define MAX_SAVED_STATES 4
static openavb_saved_state_t s_sSavedStateInfo[MAX_SAVED_STATES];
static int s_nNumSavedStates = -1;


static int get_hex_value(char c)
{
	if (c >= '0' && c <= '9') return ((int) c - (int) '0');
	if (c >= 'A' && c <= 'F') return ((int) c - (int) 'A' + 10);
	if (c >= 'a' && c <= 'f') return ((int) c - (int) 'a' + 10);
	return -1;
}

static bool get_entity_id(const char *input, U8 output[8])
{
	int i;
	for (i = 0; i < 8; ++i) {
		// Get the hexadecimal number
		int n1 = get_hex_value(*input++);
		if (n1 < 0) return false;
		int n2 = get_hex_value(*input++);
		if (n2 < 0) return false;
		output[i] = (U8) (n1 << 4 | n2);

		// Verify the colon separator
		if (i < 7) {
			if (*input++ != ':') {
				return false;
			}
		}
	}

	// Make sure anything trailing the Entity ID is not interesting.
	return (*input == '\0' || isspace(*input));
}

static bool get_saved_state_info(const char *ini_file)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	FILE* file;
	char temp_buffer[FRIENDLY_NAME_SIZE + 10];
	long temp_int;
	char *pEnd;

	s_nNumSavedStates = -1;

	char *pvtFilename = strdup(ini_file);
	if (!pvtFilename) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return false;
	}

	s_nNumSavedStates = 0;

	char *override = strchr(pvtFilename, ',');
	if (override)
		*override++ = '\0';

	file = fopen(pvtFilename, "r");
	if (!file) {
		// No file to read.  Ignore this error.
		free(pvtFilename);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return true;
	}

	for (s_nNumSavedStates = 0; s_nNumSavedStates < MAX_SAVED_STATES; ++s_nNumSavedStates) {
		// Extract the friendly name.
		while (TRUE) {
			if (fgets(temp_buffer, sizeof(temp_buffer), file) == NULL) {
				if (!feof(file)) {
					AVB_LOGF_ERROR("Error reading from INI file: %s", ini_file);
					fclose(file);
					free(pvtFilename);
					AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
					return false;
				}
				fclose(file);
				free(pvtFilename);
				AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
				return true;
			}

			// Remove any white space from the end of the friendly name.
			int length = strlen(temp_buffer);
			while (length > 0 &&
					(temp_buffer[length - 1] == '\r' ||
					 temp_buffer[length - 1] == '\n')) {
				temp_buffer[--length] = '\0';
			}
			if (length <= 0) {
				// Empty line.  Ignore it.
				continue;
			}

			// Successfully extracted the friendly name.
			strncpy(s_sSavedStateInfo[s_nNumSavedStates].listener_friendly_name, temp_buffer, FRIENDLY_NAME_SIZE);
			s_sSavedStateInfo[s_nNumSavedStates].listener_friendly_name[FRIENDLY_NAME_SIZE - 1] = '\0';
			break;
		}

		// Extract the flags.
		if (fgets(temp_buffer, sizeof(temp_buffer), file) == NULL) {
			AVB_LOGF_ERROR("Error reading from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		errno = 0;
		temp_int = strtol(temp_buffer, &pEnd, 10);
		if (*pEnd != '\n' || errno != 0 || temp_int < 0 || temp_int > 0xFFFF) {
			AVB_LOGF_ERROR("Error getting Flags from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		s_sSavedStateInfo[s_nNumSavedStates].flags = (U16) temp_int;

		// Extract the talker_unique_id.
		if (fgets(temp_buffer, sizeof(temp_buffer), file) == NULL) {
			AVB_LOGF_ERROR("Error reading from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		errno = 0;
		temp_int = strtol(temp_buffer, &pEnd, 10);
		if (*pEnd != '\n' || errno != 0 || temp_int < 0 || temp_int > 0xFFFF) {
			AVB_LOGF_ERROR("Error getting Talker Unique ID from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		s_sSavedStateInfo[s_nNumSavedStates].talker_unique_id = (U16) temp_int;

		// Extract the Talker Entity ID.
		if (fgets(temp_buffer, sizeof(temp_buffer), file) == NULL) {
			AVB_LOGF_ERROR("Error reading from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		if (!get_entity_id(temp_buffer, s_sSavedStateInfo[s_nNumSavedStates].talker_entity_id)) {
			AVB_LOGF_ERROR("Error getting Talker Entity ID from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}

		// Extract the Controller Entity ID.
		if (fgets(temp_buffer, sizeof(temp_buffer), file) == NULL) {
			AVB_LOGF_ERROR("Error reading from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
		if (!get_entity_id(temp_buffer, s_sSavedStateInfo[s_nNumSavedStates].controller_entity_id)) {
			AVB_LOGF_ERROR("Error getting Talker Entity ID from INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}

		AVB_LOGF_DEBUG("Loaded saved state %d from file:  listener_id=%s, talker_entity_id=" ENTITYID_FORMAT ", controller_entity_id=" ENTITYID_FORMAT,
			s_nNumSavedStates,
			s_sSavedStateInfo[s_nNumSavedStates].listener_friendly_name,
			ENTITYID_ARGS(s_sSavedStateInfo[s_nNumSavedStates].talker_entity_id),
			ENTITYID_ARGS(s_sSavedStateInfo[s_nNumSavedStates].controller_entity_id));
	}

	fclose(file);
	free(pvtFilename);

	AVB_LOGF_DEBUG("Extracted %d saved states from INI file: %s", s_nNumSavedStates, ini_file);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);

	return true;
}

static bool write_saved_state_info(const char *ini_file)
{
	AVB_TRACE_ENTRY(AVB_TRACE_AVDECC);

	FILE* file;
	int i;

	char *pvtFilename = strdup(ini_file);
	if (!pvtFilename) {
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return false;
	}

	char *override = strchr(pvtFilename, ',');
	if (override)
		*override++ = '\0';

	file = fopen(pvtFilename, "w");
	if (!file) {
		AVB_LOGF_WARNING("Error saving to INI file: %s", ini_file);
		free(pvtFilename);
		AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
		return false;
	}

	for (i = 0; i < s_nNumSavedStates; ++i) {
		if (fprintf(file, "%s\n%u\n%u\n" ENTITYID_FORMAT "\n" ENTITYID_FORMAT "\n\n",
				s_sSavedStateInfo[i].listener_friendly_name,
				s_sSavedStateInfo[i].flags,
				s_sSavedStateInfo[i].talker_unique_id,
				ENTITYID_ARGS(s_sSavedStateInfo[i].talker_entity_id),
				ENTITYID_ARGS(s_sSavedStateInfo[i].controller_entity_id)) < 0) {
			AVB_LOGF_ERROR("Error writing to INI file: %s", ini_file);
			fclose(file);
			free(pvtFilename);
			AVB_TRACE_EXIT(AVB_TRACE_AVDECC);
			return false;
		}
	}

	fclose(file);
	free(pvtFilename);

	AVB_LOGF_DEBUG("Saved state to INI file: %s", ini_file);
	AVB_TRACE_EXIT(AVB_TRACE_AVDECC);

	return true;
}

// Returns a pointer to the saved state information for the index supplied.
// To use, start at index 0 and increment upward.
// If NULL is returned, then no values for that or any higher indexes.
const openavb_saved_state_t * openavbAvdeccGetSavedState(int index)
{
	// Load the file data, if needed.
	if (s_nNumSavedStates < 0 && !get_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE)) {
		return NULL;
	}

	// Can we return a saved state for the requested index?
	if (s_nNumSavedStates <= 0 || index >= s_nNumSavedStates) {
		return NULL;
	}

	// Return the requested index.
	return &(s_sSavedStateInfo[index]);
}

// Add a saved state to the list of saved states.
// Returns the index for the new saved state, or -1 if an error occurred.
int openavbAvdeccAddSavedState(const char listener_friendly_name[FRIENDLY_NAME_SIZE], U16 flags, U16 talker_unique_id, const U8 talker_entity_id[8], const U8 controller_entity_id[8])
{
	// Load the file data, if needed.
	if (s_nNumSavedStates < 0 && !get_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE)) {
		return -1;
	}

	// If the list is full, delete the oldest item to make space for it.
	while (s_nNumSavedStates >= MAX_SAVED_STATES) {
		openavbAvdeccDeleteSavedState(0);
	}

	// Append the supplied state to the list of states.
	strncpy(s_sSavedStateInfo[s_nNumSavedStates].listener_friendly_name, listener_friendly_name, FRIENDLY_NAME_SIZE);
	s_sSavedStateInfo[s_nNumSavedStates].listener_friendly_name[FRIENDLY_NAME_SIZE - 1] = '\0';
	s_sSavedStateInfo[s_nNumSavedStates].flags = flags;
	s_sSavedStateInfo[s_nNumSavedStates].talker_unique_id = talker_unique_id;
	memcpy(s_sSavedStateInfo[s_nNumSavedStates].talker_entity_id, talker_entity_id, 8);
	memcpy(s_sSavedStateInfo[s_nNumSavedStates].controller_entity_id, controller_entity_id, 8);
	s_nNumSavedStates++;

	// Create a new saved state file with all the previous states, and our state.
	if (!write_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE)) {
		AVB_LOGF_ERROR("Error saving state:  listener_id=%s, talker_entity_id=" ENTITYID_FORMAT ", controller_entity_id=" ENTITYID_FORMAT,
			listener_friendly_name,
			ENTITYID_ARGS(talker_entity_id),
			ENTITYID_ARGS(controller_entity_id));
		return -1;
	}

	// Return the last index, as that is the one we used.
	return (s_nNumSavedStates - 1);
}

// Delete the saved state information at the specified index.
// Returns TRUE if successfully deleted, FALSE otherwise.
bool openavbAvdeccDeleteSavedState(int index)
{
	// Load the file data, if needed.
	if (s_nNumSavedStates < 0 && !get_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE)) {
		return false;
	}

	// Is the index valid?
	if (s_nNumSavedStates <= 0 || index >= s_nNumSavedStates) {
		return false;
	}

	// If the index points to the last item, simply reduce the count.
	if (index == s_nNumSavedStates - 1) {
		s_nNumSavedStates--;
		return (write_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE));
	}

	// Shift the items after the index to where the index is.
	memcpy(&(s_sSavedStateInfo[index]), &(s_sSavedStateInfo[index + 1]), sizeof(openavb_saved_state_t) * (--s_nNumSavedStates - index));
	return (write_saved_state_info(DEFAULT_AVDECC_SAVE_INI_FILE));
}
