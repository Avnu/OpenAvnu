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
* HEADER SUMMARY : AVDECC Read INI Public Interface
*/

#ifndef OPENAVB_AVDECC_READ_INI_PUB_H
#define OPENAVB_AVDECC_READ_INI_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_avtp_time_pub.h"
#include "openavb_tl_pub.h"

#define MAX_SAMPLING_RATES_COUNT 91

/** \file
 * AVDECC Read INI Public Interface.
 */

struct _avdecc_msg_state;
typedef struct _avdecc_msg_state avdecc_msg_state_t;

/// Maximum size of the friendly name
#define FRIENDLY_NAME_SIZE 64

/// Structure containing configuration of the host
struct openavb_tl_data_cfg {
	struct openavb_tl_data_cfg *next;

	/// Role of the host
	avb_role_t role;
	/// Initial Talker/Listener state
	tl_init_state_t initial_state;
	/// MAC address of destination - multicast (talker only if SRP is enabled)
	cfg_mac_t dest_addr;
	/// MAC address of the source
	cfg_mac_t stream_addr;
	/// Stream UID (has to be unique)
	U16 stream_uid;
	/// Maximum number of packets sent during one interval (talker only)
	U32 max_interval_frames;
	/// Maximum size of the frame
	U32 max_frame_size;
	/// Setting maximum transit time, on talker value is added to PTP Walltime,
	/// on listener value is validated timestamp range
	U32 max_transit_usec;
	/// Maximum transmit deficit in usec - should be set to expected buffer size
	/// on the listener side (talker only)
	U32 max_transmit_deficit_usec;
	/// Specify manual an internal latency (talker only)
	U32 internal_latency;
	/// Number of microseconds after which late MediaQItem will be purged as too
	/// old (listener only)
	U32 max_stale;
	/// Number of intervals to handle at once (talker only)
	U32 batch_factor;
	/// Statistics reporting frequency
	U32 report_seconds;
	/// Start paused
	bool start_paused;
	/// Class in which host will operate ::SRClassIdx_t (talker only)
	U8 sr_class;
	/// Rank of the stream #SR_RANK_REGULAR or #SR_RANK_EMERGENCY (talker only)
	U8 sr_rank;
	/// Number of raw TX buffers that should be used (talker only)
	U32 raw_tx_buffers;
	/// Number of raw RX buffers (listener only)
	U32 raw_rx_buffers;
	/// Is the interface module blocking in the TX CB.
	bool tx_blocking_in_intf;
	/// VLAN ID
	U16 vlan_id;
	/// When set incoming packets will trigger a signal to the stream task to wakeup.
	bool rx_signal_mode;
	/// Enable fixed timestamping in interface
	U32 fixed_timestamp;
	/// Wait for next observation interval by spinning rather than sleeping
	bool spin_wait;
	/// Bit mask used for CPU pinning
	U32 thread_affinity;
	/// Real time priority of thread.
	U32 thread_rt_priority;
	/// The current sample rate of this Audio Unit
	U32 current_sampling_rate;
	/// The number of sample rates in the sampling_rates field.
	U16 sampling_rates_count;
	/// An array of 4-octet sample rates supported by this Audio Unit
	U32 sampling_rates[MAX_SAMPLING_RATES_COUNT];
	/// intf_nv_audio_rate
	U32 audioRate;
	/// intf_nv_audio_bit_depth
	U8 audioBitDepth;
	/// intf_nv_channels
	U8 audioChannels;
	/// Friendly name for this configuration
	char friendly_name[FRIENDLY_NAME_SIZE];
	/// The name of the initialize function in the mapper
	char map_fn[100];

	// Pointer to the client Talker/Listener.
	// Do not free this item; it is for reference only.
	const avdecc_msg_state_t * client;
};

typedef struct openavb_tl_data_cfg openavb_tl_data_cfg_t;


/** Read an ini file.
 *
 * Parses an input configuration file to populate configuration structures, and
 * name value pairs.  Only used in Operating Systems that have a file system
 *
 * \param fileName Pointer to configuration file name
 * \param pCfg Pointer to configuration structure
 * \return TRUE on success or FALSE on failure
 *
 * \warning Not available on all platforms
 */
bool openavbReadTlDataIniFile(const char *fileName, openavb_tl_data_cfg_t *pCfg);


/** Save the connection to the saved state
 *
 * If fast connect support is enabled, this function is used to save the state
 * of the connection by a Listener when a connection is successfully made to
 * a Talker for possible fast connect support later.
 * #openavbAvdeccClearSavedState() should be called when the connection is closed.
 *
 * \param pListener Pointer to configuration for the Listener
 * \param flags The flags used for the connection (CLASS_B, SUPPORTS_ENCRYPTED, ENCRYPTED_PDU)
 * \param talker_unique_id The unique id for the Talker
 * \param talker_entity_id The binary entity id for the Talker
 * \param controller_entity_id The binary entity id for the Controller that initiated the connection
 *
 * \return TRUE on success or FALSE on failure
 */
bool openavbAvdeccSaveState(const openavb_tl_data_cfg_t *pListener, U16 flags, U16 talker_unique_id, const U8 talker_entity_id[8], const U8 controller_entity_id[8]);

/** Delete a connection with saved state
 *
 * If fast connect support is enabled, this function is used to clear previously
 * saved state information (from a previous call to #openavbAvdeccSaveState).
 * This function should be called from the Listener when a Talker/Listener connection is closed
 * (and fast connects should no longer be attempted in the future).
 *
 * \param pListener Pointer to configuration for the Listener
 *
 * \return TRUE on success or FALSE on failure
 */
bool openavbAvdeccClearSavedState(const openavb_tl_data_cfg_t *pListener);

/** Determine if the connection has a saved state
 *
 * If fast connect support is enabled, this function is used to get the last
 * saved state (from a call to #openavbAvdeccSaveState) for a Listener, if any.
 *
 * \param pListener Pointer to configuration for the Listener
 * \param p_flags Optional pointer to the flags used for the connection (CLASS_B, SUPPORTS_ENCRYPTED, ENCRYPTED_PDU)
 * \param p_talker_unique_id Optional pointer to the unique id for the Talker
 * \param p_talker_entity_id Optional pointer to the buffer to fill in the binary entity id for the Talker
 * \param p_controller_entity_id Optional pointer to the buffer to fill in the binary entity id for the Controller that initiated the connection
 *
 * \return TRUE if there is a saved state, or FALSE otherwise
 */
bool openavbAvdeccGetSaveStateInfo(const openavb_tl_data_cfg_t *pListener, U16 *p_flags, U16 *p_talker_unique_id, U8 (*p_talker_entity_id)[8], U8 (*p_controller_entity_id)[8]);

#endif  // OPENAVB_AVDECC_READ_INI_PUB_H
