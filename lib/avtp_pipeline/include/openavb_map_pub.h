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
* HEADER SUMMARY : Common mapper module public interface
*/

#ifndef OPENAVB_MAP_PUB_H
#define OPENAVB_MAP_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_mediaq_pub.h"

/** \file
  * Common mapper module public interface.
  */

/// Vendor Specific format used in AVTP headers.
#define MAP_NULL_OPENAVB_FORMAT			0x00

/// Vendor Specific format used in AVTP headers.
#define MAP_PIPE_OPENAVB_FORMAT			0x01

/// Vendor Specific CTRL format used in AVTP headers.
#define MAP_CTRL_OPENAVB_FORMAT			0x00

/** Return value of talker callback.
 */
typedef enum {
	TX_CB_RET_PACKET_NOT_READY = 0,     ///< Packet will not be sent on this callback interval
	TX_CB_RET_PACKET_READY,             ///< Packet will be sent on this callback interal.
	TX_CB_RET_MORE_PACKETS              ///< Packet will be sent and the callback called immediately again.
} tx_cb_ret_t;

/** Configuration callback.
 *
 * Each configuration name value pair for this mapping will result in this
 * callback being called.
 * \param pMediaQ A pointer to the media queue for this stream
 * \param name configuration item name
 * \param value configuration item value
 */
typedef void (*openavb_map_cfg_cb_t)(media_q_t *pMediaQ, const char *name, const char *value);

/** AVB subtype callback.
 *
 * \return The AVB subtype for this mapping.
 */
typedef U8(*openavb_map_subtype_cb_t)();

/** AVTP version callback.
 *
 * \return The AVTP version for this mapping.
 */
typedef U8(*openavb_map_avtp_version_cb_t)();

/** Maximum data size callback.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \return The maximum data size that will be used.
 */
typedef U16(*openavb_map_max_data_size_cb_t)(media_q_t *pMediaQ);

/** Transmit interval callback.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \return The intended transmit interval (in frames per second).
 *         0 = default for talker / class.
 */
typedef U32(*openavb_map_transmit_interval_cb_t)(media_q_t *pMediaQ);

/** General initialize callback regardless if a talker or listener.
 *
 * Called once during openavbTLOpen().
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_map_gen_init_cb_t)(media_q_t *pMediaQ);

/** A call to this callback indicates that this mapping module will be a talker.
 *
 * Any talker initialization can be done in this function.
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_map_tx_init_cb_t)(media_q_t *pMediaQ);

/** This talker callback will be called for each AVB observation interval.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param pData pointer to data
 * \param dataLen length of data
 * \return One of enum \ref tx_cb_ret_t values.
 */
typedef tx_cb_ret_t(*openavb_map_tx_cb_t)(media_q_t *pMediaQ, U8 *pData, U32 *datalen);

/** A call to this callback indicates that this mapping module will be
 * a listener.
 *
 * Any listener initialization can be done in this function.
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_map_rx_init_cb_t)(media_q_t *pMediaQ);

/** This callback occurs when running as a listener and data is available.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param pData pointer to data
 * \param dataLen length of data
 */
typedef bool (*openavb_map_rx_cb_t)(media_q_t *pMediaQ, U8 *pData, U32 datalen);

/** This callback will be called when the stream is closing.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_map_end_cb_t)(media_q_t *pMediaQ);

/** General shutdown callback regardless if a talker or listener.
 *
 * Called once during openavbTLClose().
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_map_gen_end_cb_t)(media_q_t *pMediaQ);

/** Set source bitrate callback.
 *
 * Used to inform mapping module on the bitrate of the data source (computed by
 * the interface module). The reported bitrate is used by the
 * openavb_map_get_max_interval_frames_cb_t() callback.
 * \param pMediaQ A pointer to the media queue for this stream
 * \param bitrate Data source bitrate
 *
 * \note This callback is optional, does not need to be implemented in the
 * mapping module.
 */
typedef void (*openavb_map_set_src_bitrate_cb_t)(media_q_t *pMediaQ, unsigned int bitrate);

/** Get max interval frames.
 *
 * Called to get the maximum number of frames per interval for the talker. The
 * calculation is based on the source bitrate provided in the call to
 * openavb_map_set_src_bitrate_cb_t(). Will override ini file "max_interval_frames"
 * configuration option.
 * \param pMediaQ A pointer to the media queue for this stream
 * \param sr_class Stream reservation class
 * \return Maximum number of frames per interval
 *
 * \note This callback is optional, does not need to be implemented in the
 * mapping module.
 */
typedef unsigned int (*openavb_map_get_max_interval_frames_cb_t)(media_q_t *pMediaQ, SRClassIdx_t sr_class);

#if ATL_LAUNCHTIME_ENABLED
/** This talker callback will be called for each AVB observation interval to calculate a launchtime of packet.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param pData pointer to data
 * \param lt launchtime of media q item
 * \return One of enum \ref tx_cb_ret_t values.
 */
typedef bool (*openavb_map_lt_calc_cb_t)(media_q_t *pMediaQ, U64 *lt);
#endif

/** Mapping callbacks structure.
 */
typedef struct {
	/// Configuration callback.
	openavb_map_cfg_cb_t				map_cfg_cb;
	/// AVB subtype callback.
	openavb_map_subtype_cb_t			map_subtype_cb;
	/// AVTP version callback.
	openavb_map_avtp_version_cb_t		map_avtp_version_cb;
	/// Maximum data size callback.
	openavb_map_max_data_size_cb_t		map_max_data_size_cb;
	/// Transmit interval callback.
	openavb_map_transmit_interval_cb_t	map_transmit_interval_cb;
	/// General initialize callback.
	openavb_map_gen_init_cb_t			map_gen_init_cb;
	/// Initialize transmit callback.
	openavb_map_tx_init_cb_t			map_tx_init_cb;
	/// Transmit callback.
	openavb_map_tx_cb_t					map_tx_cb;
	/// Initialize receive callback.
	openavb_map_rx_init_cb_t			map_rx_init_cb;
	/// Receive callback.
	openavb_map_rx_cb_t					map_rx_cb;
	/// Stream end callback.
	openavb_map_end_cb_t				map_end_cb;
	/// General shutdown callback.
	openavb_map_gen_end_cb_t			map_gen_end_cb;
	/// Set source bit rate callback.
	openavb_map_set_src_bitrate_cb_t    map_set_src_bitrate_cb;
	/// Max interval frames callback.
	openavb_map_get_max_interval_frames_cb_t map_get_max_interval_frames_cb;

#if ATL_LAUNCHTIME_ENABLED
	// Launchtime calculation
	openavb_map_lt_calc_cb_t            map_lt_calc_cb;
#endif
} openavb_map_cb_t;

/** Main initialization entry point into the mapping module.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param[out] pMapCB Pointer to the callback structure. All the members of this
 *        structure must be set during this function call.
 * \param inMaxTransitUsec maximum expected latency.
 */
typedef bool (*openavb_map_initialize_fn_t)(media_q_t *pMediaQ, openavb_map_cb_t *pMapCB, U32 inMaxTransitUsec);

#endif  // OPENAVB_MAP_PUB_H

