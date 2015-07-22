/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#ifndef STC_SOCKETS_H
#define STC_SOCKETS_H 1

#include <stdint.h>
#include <stddef.h>
#include "stc_avb_platform.h"
#include "stc_avb_time_osal.h"

#ifndef IFNAMSIZ
#define IFNAMSIZ	1
#endif

typedef struct {
	U16 length;
	U8 data[];				// Empty array points to data. struct memory must be manually allocated.
} eth_frame_t;

typedef struct ethFrameStruct {
	uint8_t destAddr[6];
	uint8_t srcAddr[6];
	uint16_t type;
	uint8_t payload;
} ethFrameStruct;

typedef struct ethFrameVlanStruct {
	uint8_t destAddr[6];
	uint8_t srcAddr[6];
	uint8_t tag[4];
	uint16_t type;
	uint8_t payload;
}ethFrameVlanStruct;

typedef struct rxSockBufStruct {
	struct rxSockBufStruct *next;
	uint8_t *packet; // pointer to start of packet (which starts with length)
}rxSockBufStruct;

#define AF_INET						2
#define	PF_PACKET					17	// Packet family.

// Socket protocol types (TCP/UDP/RAW)
#define SOCK_STREAM					1
#define SOCK_DGRAM					2
#define SOCK_RAW					3

struct sockaddr {
  uint8_t sa_len;
  uint8_t sa_family;
  char sa_data[14];
};

#define PACKET_MR_MULTICAST			0
#define PACKET_MR_PROMISC			1
#define PACKET_MR_ALLMULTI			2

#define PACKET_ADD_MEMBERSHIP		1
#define PACKET_DROP_MEMBERSHIP		2
#define	PACKET_RECV_OUTPUT			3
#define	PACKET_STATISTICS			6

#define	IFF_UP						0x1		// interface is up

#define TPACKET_ALIGNMENT			16
#define TPACKET_ALIGN(x)			(((x)+TPACKET_ALIGNMENT-1)&~(TPACKET_ALIGNMENT-1))

#ifndef socklen_t
#  define socklen_t int
#endif

#define SOL_SOCKET					1
#define SOL_PACKET					263
#define SO_MARK						36
#define	ETHERTYPE_VLAN				0x8100	// IEEE 802.1Q VLAN tagging
#define MSG_DONTWAIT				0x08    // Nonblocking i/o for this operation only
#define EINTR						4		// Interrupted system call

// Supported error numbers
#define  ENOMEM						12		// Out of memory
#define  EINVAL						22		// Invalid argument

// initialize sockets and the Rx buffer task
bool osalInitSockets(U32 maxSockets);

// Create a new socket of type TYPE in domain DOMAIN, using
// protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
// Returns a file descriptor for the new socket, or -1 for errors.
int osalSocket(int domain, int type, int protocol);

// Set socket options
int osalSetsockopt(int sk, int level, int optname, const void *optval, socklen_t optlen);

// Set the AVTP subtype for filtering
int osalSetAvtpSubtype(int sk, uint8_t avtpSubtype);

// Enable Poll Mode
int osalEnablePoll(int sk);

// Set socket as RX
int osalSetRxMode(int sk, int bufCount, int bufSize);

// Set socket signal on RX mode
int osalSetRxSignalMode(int sk, bool rxSignalMode);

// Set socket as TX
int osalSetTxMode(int sk, int bufCount, int bufSize);

// bind, allows transmission, and reception of socket data
int osalBind(int sk);

// close socket
int osalCloseSocket(int sk);

// Get the PollFd for the socket
STC_POLLFD_PTR osalGetSocketPollFd(int sk);

// get unprocessed Rx buffers
int osalGetRxSockLevel(int sk);

// get unprocessed Tx buffers
int osalGetTxSockLevel(int sk);

// Get the next Rx buffer locking it. Null if none available
eth_frame_t *osalSocketGetRxBuf(int sk);

// Release (unlock) the Rx buffer for a socket keeping it in the queue. Returns TRUE on success
bool osalSocketRelRxBuf(int sk);

// Pulls the Rx buffer off the queue. Returns TRUE on success.
bool osalSocketPullRxBuf(int sk);

// Get the next Tx buffer locking it. Null if none available
eth_frame_t *osalSocketGetTxBuf(int sk);

// Release (unlock) the Tx buffer. Returns TRUE on success
bool osalSocketRelTxBuf(int sk);

// Pushes the Tx buffer onto the queue. Returns TRUE on success.
bool osalSocketPushTxBuf(int sk);

// Direct call (not in task) to process a AVTP packet or packets
void socketRXDirectAVTP(void);

#endif // STC_SOCKETS_H
