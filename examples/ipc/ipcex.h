/******************************************************************************

  Copyright (c) 2016, AudioScience, Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the AudioScience, Inc. nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

/** \file ipcex.h
 * Interprocess communication example.
 * This example header file shows a typical API for a publically exposed
 * interface to a protocol module. The actual functionality the module
 * supports could be anything. The point of this example is to show
 * example calling APIs.
 */

#ifndef _IPCEX_H_
#define _IPCEX_H_

#define IPCEX_MAX_SIZEOF_OUT 64

/**
 * \brief  ipcex Set command.
 * \param  index [in] stream index
 * \param  streamID [in] the streamID to set.
 * \param  vlan [in] the vlanID to set.
 * \param  status [out] Pointer to detailed status code.
 * \param  sizeof_status [in] The size of the memory pointed to by the status pointer.
 * \return 0 on success, -1 on failure.
 */
int ipcex_SetStreamIDandVLAN(uint32_t index, uint64_t streamID,
	uint32_t vlan, void *status, size_t sizeof_status);

/**
 * \brief  ipcex Get command.
 * \param  index [in] stream index
 * \param  streamID [out] the returned streamID.
 * \param  vlan [out] the returned vlanID.
 * \param  status [out] Pointer to detailed status code.
 * \param  sizeof_status [in]The size of the memory pointed to by the status pointer.
 * \return 0 on success, -1 on failure.
 */
int ipcex_GetStreamIDandVLAN(uint32_t index, uint64_t *streamID,
	uint32_t *vlan, void *status, size_t sizeof_status);


#endif





