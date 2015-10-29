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
* HEADER SUMMARY : Control interface module public interface
*/

#ifndef OPENAVB_INTF_CTRL_PUB_H
#define OPENAVB_INTF_CTRL_PUB_H 1

#include "openavb_types_pub.h"

/** \file
 * Control interface module public interface
 */

/// This callback once registered will be called for each control command received by the listener.
typedef void (*openavb_intf_ctrl_receive_cb_t)(void *pTLHandle, U8 *pData, U32 dataLength);

/// Register a callback for received control commands.
typedef bool (*openavb_intf_ctrl_register_receive_control_fn_t)(void *pIntfHandle, void *pTLHandle, openavb_intf_ctrl_receive_cb_t openavbIntfCtrlReceiveCommandCB);

/// Register a callback for received control commands.
typedef bool (*openavb_intf_ctrl_unregister_receive_control_fn_t)(void *pIntfHandle);

/// Submit a control command for transmission.
typedef bool (*openavb_intf_ctrl_send_control_fn_t)(void *pIntfHandle, U8 *pData, U32 dataLength, U32 usecDelay);

/// Callbacks to control functions
typedef struct {
	/// Registering callback function
	openavb_intf_ctrl_register_receive_control_fn_t 	register_receive_control_cb;
	/// Unregistering callback function
	openavb_intf_ctrl_unregister_receive_control_fn_t 	unregister_receive_control_cb;
	/// Submitting control command
	openavb_intf_ctrl_send_control_fn_t					send_control_cb;
} openavb_intf_host_cb_list_t;

#endif  // OPENAVB_INTF_CTRL_PUB_H
