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
* HEADER SUMMARY : Common interface module public header
*/

#ifndef OPENAVB_INTF_PUB_H
#define OPENAVB_INTF_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_mediaq_pub.h"

/** \file
 * Common interface module public header.
 */

/** Configuration callback into the interface module.
 *
 * This callback function is called during the reading of the configuration file
 * by the talker and listener for any named configuration item starting with
 * "intf_nv".
 * This is a convenient way to store new configuration name/value pairs that are
 * needed in an interface module.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param name The item name from the configuration file
 * \param value The item value from the configuration file
 */
typedef void (*openavb_intf_cfg_cb_t)(media_q_t *pMediaQ, const char *name, const char *value);

/** General initialize callback regardless if a talker or listener.
 *
 * This callback function is called when the openavbTLOpen() function is called for
 * the EAVB SDK API.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_intf_gen_init_cb_t)(media_q_t *pMediaQ);

/** AVDECC initialize callback for both a talker or listener.
 *
 * Entity model based configuration can be processed at this time.
 * This callback is optional and only executed when AVDECC is used to connect
 * streams.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param configIdx current configuration descriptor index for the Entity Model
 * \param descriptorType The descriptorType is expected to be of STREAM_INPUT
 *        for listeners and STREAM_OUTPUT for talkers.
 * \param descriptorIdx descriptor index in the Entity Model
 */
typedef void (*openavb_intf_avdecc_init_cb_t)(media_q_t *pMediaQ, U16 configIdx, U16 descriptorType, U16 descriptorIdx);

/** Initialize transmit callback into the interface module.
 *
 * This callback function is called anytime a stream reservation has completed
 * successfully within a talker process. It does not get called when running
 * within a listener process.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_intf_tx_init_cb_t)(media_q_t *pMediaQ);

/** Transmit callback into the interface module.
 *
 * This is the transmit callback function for the interface module.
 * This function will typically be called thousands of times per second
 * depending the SR class type (A or B). This frequency may also be changed by
 * the mapping module and at times configurable by mapping modules for example
 * with the map_nv_tx_rate configuration value.
 * If pacing is done in the interface module by:
 *
 *     cfg->tx_blocking_in_intf = FALSE;
 *
 * Then this callback will suspend task execution until there is media data
 * available for the mapping module.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef bool (*openavb_intf_tx_cb_t)(media_q_t *pMediaQ);

/** Initialize the receive callback into the interface module.
 *
 * This callback function is called anytime a stream reservation has completed
 * successfully within a listener process. It does not get called when running
 * within a talker process.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_intf_rx_init_cb_t)(media_q_t *pMediaQ);

/** Translate RX data callback.
 *
 * This callback that may be used by mapping modules to allow 
 * interfaces to translate packet data as it arrives and before 
 * it gets packed into the media queue Item. Mapping modules 
 * MUST expose a function pointer var in their public data and 
 * the interface module must set it for the CB to be used. 
 *
 * \param pMediaQ A pointer to the media queue for this stream 
 * \param pPubDataQ A pointer to the data 
 * \param length Length of the data
 */
typedef void (*openavb_intf_rx_translate_cb_t)(media_q_t *pMediaQ, U8 *pPubData, U32 length);

/** Receive callback into the interface module.
 *
 * This callback function is called when AVB packet data is received or when
 * tail data item within the media queue has reached the presentation time
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef bool (*openavb_intf_rx_cb_t)(media_q_t *pMediaQ);

/** Callback when the stream is ending.
 *
 * This callback function is called when a stream is closing.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_intf_end_cb_t)(media_q_t *pMediaQ);

/** General shutdown callback into the interface module.
 *
 * This callback function is called when the openavbTLClose() function is called for
 * the EAVB SDK API
 *
 * \param pMediaQ A pointer to the media queue for this stream
 */
typedef void (*openavb_intf_gen_end_cb_t)(media_q_t *pMediaQ);

/** Query the interface for source bitrate.
 *
 * This callback is called to get the maximum bitrate of the source (in bits per
 * second). For example for a mpeg2ts file interface this callback returns the
 * maximum bitrate of the mpeg2ts file.
 * \param pMediaQ A pointer to the media queue for this stream
 * \return Maximum bitrate of the source.
 *
 * \note This callback is optional, does not need to be implemented in the
 * interface module.
 */
typedef unsigned int (*openavb_intf_get_src_bitrate_t)(media_q_t *pMediaQ);

/** Interface callbacks structure.
 */
typedef struct {
	/// Configuration callback.
	openavb_intf_cfg_cb_t			intf_cfg_cb;
	/// General initialize callback.
	openavb_intf_gen_init_cb_t		intf_gen_init_cb;
	/// AVDECC initialize callback.
	openavb_intf_avdecc_init_cb_t	intf_avdecc_init_cb;
	/// Initialize transmit callback.
	openavb_intf_tx_init_cb_t		intf_tx_init_cb;
	/// Transmit callback.
	openavb_intf_tx_cb_t			intf_tx_cb;
	/// Initialize receive callback.
	openavb_intf_rx_init_cb_t		intf_rx_init_cb;
	// openavb_intf_rx_translate_cb_t	intf_rx_translate_cb;	// Hidden since it is for direct mapping -> interface CBs
	/// Receive callback.
	openavb_intf_rx_cb_t			intf_rx_cb;
	/// Stream end callback.
	openavb_intf_end_cb_t			intf_end_cb;
	/// General shutdown callback.
	openavb_intf_gen_end_cb_t		intf_gen_end_cb;
	/// Pointer to interface specific callbacks that a hosting application may call.
	/// It is pointer to openavb_intf_host_cb_list_t structure.
	void *						intf_host_cb_list;
	/// Source bit rate callback.
	openavb_intf_get_src_bitrate_t  intf_get_src_bitrate_cb;
} openavb_intf_cb_t;

/** Main initialization entry point into the interface module.
 *
 * This is the main entry point into the interface module. Every interface
 * module must define this function. The talker process and listener process
 * call this function directly while the configuration file it being read. The
 * function address is discovered via the settings in the talker and listener
 * configuration structure. For example:
 *
 *     osalCfg.pIntfInitFn = openavbIntfJ6Video;
 *
 * Within this function the callbacks must all be set into the structure pointer
 * parameter.
 * Any interface module initialization can take place during this call such as
 * setting default values into configuration settings.
 * Private interface module should be allocated during this call. This can be
 * used to hold configuration data as well as functional variable data.
 *
 * \param pMediaQ A pointer to the media queue for this stream
 * \param pIntfCB Pointer to the callback structure. All the members of this
 *        structure must be set during this function call
 * \return TRUE on success or FALSE on failure
 */
typedef bool (*openavb_intf_initialize_fn_t)(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB);

/** \example openavb_intf_echo.c
 * \brief A source code for a complete sample interface module.
 */
#endif  // OPENAVB_INTF_PUB_H

