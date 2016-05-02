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
  
   3. Neither the name of the AudioScience, Inc nor the names of its 
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
#ifndef _OAVB_IPC_H_
#define _OAVB_IPC_H_

#ifdef __cplusplus

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif

#ifdef _STDINT_H
#undef _STDINT_H
#endif
#endif

#if defined __linux__ /* for struct sockaddr and socklen_t */
#include <sys/socket.h>
#endif

#if !defined UNDER_RTSS
#if defined _MSC_VER  /* for struct sockaddr and socklen_t */
#include <winsock2.h>
#include <ws2tcpip.h>
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Abstract interface for communicating between AVB/TSN modules.
 */
struct oavb_ipc {
	/** pointer to private data pertaining to the connection */
	void *priv;
	/** close any communication channels */
	int (*close)(struct oavb_ipc *ipc, void *flags);
	/** open any required communication channels */
	int (*open) (struct oavb_ipc *ipc, void *flags);
	/** bind communication channels */
	int (*bind) (struct oavb_ipc *ipc, void *flags);
	/** receive from the communication channel */
	int (*recv) (struct oavb_ipc *ipc, void *buf, int buflen);
	/** send to the communication channel */
	int (*send) (struct oavb_ipc *ipc, void *buf, int buflen);
#if defined __linux__
	/** custom linux opertation for implementing polling */
	int (*get_fd) (struct oavb_ipc *ipc);
#endif
};

#ifdef __cplusplus
}
#endif
#endif /* _OAVB_IPC_H_ */

