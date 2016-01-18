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
* HEADER SUMMARY : Talker Listener Public Interface
*/

#ifndef OPENAVB_TL_PUB_H
#define OPENAVB_TL_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_pub.h"
#include "openavb_intf_pub.h"
#include "openavb_avtp_time_pub.h"

/** \file
 * Talker Listener Public Interface.
 */

/// Handle to a single talker or listener.
typedef void *tl_handle_t;

/// Types of statistics gathered
typedef enum {
	/// Number of TX calls
	TL_STAT_TX_CALLS,
	/// Number of TX frames
	TL_STAT_TX_FRAMES,
	/// NUmber of late TX frames
	TL_STAT_TX_LATE,
	/// Number of bytes send
	TL_STAT_TX_BYTES,
	/// Number of RX calls
	TL_STAT_RX_CALLS,
	/// Number of RX frames
	TL_STAT_RX_FRAMES,
	/// Number of RX frames lost
	TL_STAT_RX_LOST,
	/// Number of bytes received
	TL_STAT_RX_BYTES,
} tl_stat_t;

/// Maximum number of configuration parameters inside INI file a host can have
#define MAX_LIB_CFG_ITEMS 64


/// Maximum size of interface name
#define IFNAMSIZE 16

/// Indicatates that VLAN ID is not set in configuration
#define VLAN_NULL UINT16_MAX

/// Structure containing configuration of the host
typedef struct {
	/// Role of the host
	avb_role_t role;
	/// Structure with callbacks to mapping
	openavb_map_cb_t map_cb;
	/// Structure with callbacks to inteface
	openavb_intf_cb_t intf_cb;
	/// MAC address of destination - multicast (talker only if SRP is enabled)
	cfg_mac_t dest_addr;
	/// MAC address of the source
	cfg_mac_t stream_addr;
	/// Stream UID (has to be unique)
	S32 stream_uid;
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
	/// Class in which host will operatea ::SRClassIdx_t (talker only)
	U8 sr_class;
	/// Rank of the stream #SR_RANK_REGULAR or #SR_RANK_EMERGENCY (talker only)
	U8 sr_rank;
	/// Number of raw TX buffers that should be used (talker only)
	U32 raw_tx_buffers;
	/// Number of raw rx buffers (listener only)
	U32 raw_rx_buffers;
	/// Is the interface module blocking in the TX CB.
	bool tx_blocking_in_intf;
	/// Network interface name. Not used on all platforms.
	char ifname[IFNAMSIZE];
	/// VLAN ID
	U16 vlan_id;
	/// When set incoming packets will trigger a signal to the stream task to wakeup.
	bool rx_signal_mode;

	/// Initialization function in mapper
	openavb_map_initialize_fn_t pMapInitFn;
	/// Initialization function in interface
	openavb_intf_initialize_fn_t pIntfInitFn;
} openavb_tl_cfg_t;

/// Structure holding configuration of mapping and interface modules
typedef struct openavb_tl_cfg_name_value_t {
	/// Configuration parameters Names for interface and mapping modules
	char *libCfgNames[MAX_LIB_CFG_ITEMS];
	/// Configuration parameters Values for interface and mapping modules
	char *libCfgValues[MAX_LIB_CFG_ITEMS];
	/// Number of configuration parameters defined
	U32 nLibCfgItems;
} openavb_tl_cfg_name_value_t; 


/** Initialize the talker listener library.
 *
 * This function must be called first before any other in the openavb_tl  API.
 * Any AVTP wide initialization occurs at this point
 *
 * \param maxTL The maximum number of talkers and listeners that will be started
 * \return TRUE on success or FALSE on failure
 *
 * \warning Must be called prior to using any other TL APIs
 */
bool openavbTLInitialize(U32 maxTL);

/** Final cleanup of the talker listener library.
 *
 * This function must be called last after all talkers and listeners have been closed
 *
 * \return TRUE on success or FALSE on failure
 *
 * \warning Should be called after all Talker and Listeners are closed
 */
bool openavbTLCleanup(void);

/** Get the version of the AVB stack.
 *
 * Fills the major, minor and revision parameters with the values version
 * information of the AVB stack
 *
 * \param major The major part of the version number
 * \param minor The minor part of the version number
 * \param revision The revision part of the version number
 * \return TRUE on success or FALSE on failure
 */
bool openavbGetVersion(U8 *major, U8 *minor, U8 *revision);

/** Open a talker or listener.
 *
 * This will create a talker / listener
 *
 * \return handle of the talker/listener. NULL if the talker or listener could not be loaded
 */
tl_handle_t openavbTLOpen(void);

/** Initialize the configuration to default values.
 * Initializes configuration file to default values
 *
 * \param pCfg Pointer to configuration structure
 */
void openavbTLInitCfg(openavb_tl_cfg_t *pCfg);

/** Configure the talker / listener.
 *
 * Configures talker/listener with configuration values from configuration
 * structure and name value pairs
 *
 * \param handle Handle of talker/listener
 * \param pCfg Pointer to configuration structure 
 * \param pNVCfg Pointer to name value pair configuration 
 *  	  structure
 * \return TRUE on success or FALSE on failure
 */
bool openavbTLConfigure(tl_handle_t handle, openavb_tl_cfg_t *pCfg, openavb_tl_cfg_name_value_t *pNVCfg);

/** Run the talker or listener.
 *
 * The talker or listener indicated by handle that was previously loaded with
 * the openavbTLOpen() function will be run. The stream will be opened at this time.
 * Two threads created, one for endpoint IPC and one for stream handling.
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE on success or FALSE on failure
 */
bool openavbTLRun(tl_handle_t handle);

/** Stop a single talker or listener.
 *
 * Stop a single talker or listener. At this point data will not be sent or recieved
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE on success or FALSE on failure
 */
bool openavbTLStop(tl_handle_t handle);

/** Pause or resume as stream.
 *
 * A paused stream will do everything except will toss both tx and rx packets
 *
 * \param handle The handle return from openavbTLOpen()
 * \param bPause TRUE to pause, FALSE to resume
 * \return TRUE on success or FALSE on failure
 */
void openavbTLPauseStream(tl_handle_t handle, bool bPause);

/** Close the talker or listener.
 *
 * The talker or listener indicated by handle that was previously loaded with
 * the openavbTLOpen() function will be closed. The stream will be shutdown at this
 * time and the threads created for this talker or listener will be killed
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE on success or FALSE on failure
 */
bool openavbTLClose(tl_handle_t handle);

/** Get a pointer to a list of interfaces module callbacks.
 *
 * In cases where a host application needs to call directly into an interface
 * module it is preferable to do so with the APIs supplied in this SDK. This
 * will allow passing back into the interface module a handle to its data. This
 * handle is the value returned from openavbTLGetIntfHandle()
 *
 * \param handle The handle return from openavbTLOpen()
 * \return A void * is returned. This is a pointer that can be resolved when
 * combined with a public API defined by the specific interface module
 */
void* openavbTLGetIntfHostCBList(tl_handle_t handle);

/** Get a handle to the interface module data from this talker or listener.
 *
 * Returns a handle to the interface module data. This handle will be used in
 * call backs into the interface module from the host application and allows the
 * interface module to associate the call back with the correct talker /
 * listener (stream)
 *
 * \param handle The handle return from openavbTLOpen() \return Handle as a void *
 * to the interface module data. This returned value is only intended to be
 * passed back to the interface module in call backs from the host application.
 */
void* openavbTLGetIntfHandle(tl_handle_t handle);

/** Check if a talker or listener is running.
 *
 * Checks if the talker or listener indicated by handle is running. The running
 * status will be true after calling openavbTLRun()
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE if running FALSE if not running
 */
bool openavbTLIsRunning(tl_handle_t handle);

/** Checks if a talker or listener is connected to the endpoint.
 *
 * Checks if the talker or listener indicated by handle is connected to the
 * endpoint process
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE if connected FALSE if not connected
 */
bool openavbTLIsConnected(tl_handle_t handle);

/** Checks if a talker or listener has an open stream.
 *
 * Checks if the talker or listener indicated by handle has an open stream
 *
 * \param handle The handle return from openavbTLOpen()
 * \return TRUE if streaming FALSE if not streaming
 */
bool openavbTLIsStreaming(tl_handle_t handle);

/** Return the role of the current stream handle.
 *
 * Returns if the current open stream is a talker or listener
 *
 * \param handle The handle return from openavbTLOpen()
 * \return The current role
 */
avb_role_t openavbTLGetRole(tl_handle_t handle);

/** Allows pulling current stat counters for a running stream.
 *
 * The various stat counters for a stream can be retrieved with this function
 *
 * \param handle The handle return from openavbTLOpen()
 * \param stat Which stat to retrieve
 * \return the requested counter
 */
U64 openavbTLStat(tl_handle_t handle, tl_stat_t stat);

/** Read an ini file. 
 *
 * Parses an input configuration file tp populate configuration structures, and
 * name value pairs.  Only used in Operating Systems that have a file system
 *
 * \param TLhandle Pointer to handle of talker/listener
 * \param fileName Pointer to configuration file name
 * \param pCfg Pointer to configuration structure 
 * \param pNVCfg Pointer to name value pair configuration 
 *  	  structure
 * \return TRUE on success or FALSE on failure
 *
 * \warning Not available on all platforms
 */
bool openavbTLReadIniFileOsal(tl_handle_t TLhandle, const char *fileName, openavb_tl_cfg_t *pCfg, openavb_tl_cfg_name_value_t *pNVCfg);


/** \example openavb_host.c
 * Talker / Listener example host application.
 */
#endif  // OPENAVB_TL_PUB_H
