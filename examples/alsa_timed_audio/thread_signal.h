/******************************************************************************

  Copyright (c) 2018, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
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

#ifndef THREAD_SIGNAL_H
#define THREAD_SIGNAL_H

//! \typedef isaudk_signal_t
//! \brief Signal object handle
typedef struct isaudk_signal* isaudk_signal_t;

//! \enum isaudk_signal_error_t
//! \brief Signal API return code
typedef enum {
	ISAUDK_SIGNAL_OK,		//!< No error
	ISAUDK_SIGNAL_SYSERROR,		//!< Internal error
	ISAUDK_SIGNAL_TIMEDOUT,		//!< Wait timeout
} isaudk_signal_error_t;

// Signal is created in an unsignaled state
// Auto-reset on wait

//! \brief Allocate signal object.
//! \details	Created in unsignalled state
//!
//! \param signal[out]		Reference to signal object
//!
//! \return Return code
isaudk_signal_error_t
isaudk_create_signal( isaudk_signal_t *signal );

//! \brief Free signal object.
//!
//! \param signal[in]		Reference to signal object
//!
//! \return Return code
isaudk_signal_error_t
isaudk_free_signal( isaudk_signal_t signal );

//! \brief Wait for signal.
//! \details	Set to unsignalled state after successful wait operation
//!
//! \param signal[in]		Reference to signal object
//! \param wait[in]		Number of milliseconds to wait (0 = forever)
//!
//! \return Return code
isaudk_signal_error_t
isaudk_signal_wait( isaudk_signal_t signal, unsigned wait );

//! \brief Send/set signal.
//!
//! \param signal[in]		Reference to signal object
//!
//! \return Return code
isaudk_signal_error_t
isaudk_signal_send( isaudk_signal_t signal );

#endif/*THREAD_SIGNAL_H*/
