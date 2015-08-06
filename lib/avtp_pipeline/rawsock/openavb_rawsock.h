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
 * HEADER SUMMARY : API for raw socket library.
 */

#ifndef RAWSOCK_H
#define RAWSOCK_H 1

#include "openavb_types.h"
#include "openavb_osal.h"

// Structure to hold information about a network interface
typedef struct {
	char name[IFNAMSIZ];
	struct ether_addr mac;
	int index;
	int mtu;
} if_info_t;

// Get information about an interface
//  ifname - interface name
//  info   - structure to be filled with info about iface
bool openavbCheckInterface(const char *ifname, if_info_t *info);

// Structure to hold fields from Ethernet header
// Used to set information to be added to TX frames,
//  or to return info parsed from RX frames.
typedef struct {
	U8 *shost;		// Source MAC address
	U8 *dhost; 		// Destination MAC address
	U16 ethertype;	// Ethernet type (protocol)
	bool vlan;		// Include VLAN header?
	U8  vlan_pcp;	// VLAN Priority Code Point
	U16 vlan_vid;	// VLAN ID
} hdr_info_t;
	
	
// Open a raw socket, and setup circular buffer for sending or receiving frames.  
//
// Returns rawsock handle which must be passed back to other rawsock library functions.
//
void *openavbRawsockOpen(const char *ifname,	// network interface name to bind to
					 bool rx_mode,			// TX mode flag
					 bool tx_mode,			// RX mode flag
					 U16 ethertype,			// Ethernet type (protocol)
					 U32 frame_size,		// maximum size of frame to send/receive
					 U32 num_frames);		// number of frames in the circular buffer

// Set signal on RX mode
void openavbSetRxSignalMode(void *rawsock, bool rxSignalMode);

// Close the raw socket and release associated resources.
void openavbRawsockClose(void *rawsock);

// Get the socket used for this rawsock.
// Client can use that socket for poll/select calls.
int openavbRawsockGetSocket(void *rawsock);

// Get the Ethernet address of the interface.
// pBuf should point to "char buf[ETH_ALEN]"
bool  openavbRawsockGetAddr(void *rawsock, U8 addr[ETH_ALEN]);

// Flags that can be passed to GetRxFrame instead of actual timeout values.
#define OPENAVB_RAWSOCK_NONBLOCK  	(0)
#define OPENAVB_RAWSOCK_BLOCK		(-1)

// RX FUNCTIONS
// 
// Get a received frame.
// Returns pointer to received frame, or NULL
U8 *openavbRawsockGetRxFrame(void *rawsock,	// rawsock handle
						 U32 usecTimeout,   // timeout (microseconds)
						 					// or use OPENAVB_RAWSOCK_BLOCK/NONBLOCK
						 U32 *offset,	// offset of frame in the frame buffer
						 U32 *len);		// returns length of received frame

// Parse the frame header.  Returns length of header, or -1 for failure
int openavbRawsockRxParseHdr(void* rawsock, U8 *pBuffer, hdr_info_t *pInfo);

// Release the received frame for re-use.
bool openavbRawsockRelRxFrame(void *rawsock, U8 *pFrame);

// Add (or drop) membership in link-layer multicast group
bool openavbRawsockRxMulticast(void *rawsock, bool add_membership, const U8 buf[ETH_ALEN]);

// Allows for filtering of AVTP subtypes at the rawsock level for rawsock implementations that aren't able to
//  delivery the same packet to multiple sockets. 
bool openavbRawsockRxAVTPSubtype(void *rawsock, U8 subtype);

// TX FUNCTIONS
//
// Setup the header that we'll use on TX Ethernet frames.
// Called once during intialization.
bool openavbRawsockTxSetHdr(void *rawsock, hdr_info_t *pInfo);

// Copy the pre-set Ethernet header into the frame
bool openavbRawsockTxFillHdr(void *rawsock,
						 U8  *pBuffer,
						 U32 *hdrlen);

// Set the SO_MARK option on the socket
// (used to identify packets for FQTSS in kernel)
bool openavbRawsockTxSetMark(void *rawsock, int prio);

// Get a buffer to hold a frame for transmission.
// Returns pointer to frame (or NULL).
U8 *openavbRawsockGetTxFrame(void *rawsock,		// rawsock handle
						   bool blocking,	// TRUE blocks until frame buffer is available.
						   U32 *size);		// size of the frame buffer

// Release Tx buffer without sending it
bool openavbRawsockRelTxFrame(void *rawsock, U8 *pBuffer);

// Submit a frame and mark it "ready to send"
bool openavbRawsockTxFrameReady(void *rawsock,	// rawsock handle
							U8 *pFrame, 	// pointer to frame buffer
							U32 len);		// length of frame to send

// Send all packets that are marked "ready to send".
// Returns count of bytes in sent frames - or < 0 for error.
int openavbRawsockSend(void *rawsock);

// Check Tx buffer level in sockets
int openavbRawsockTxBufLevel(void *rawsock);

// Check Rx buffer level in sockets
int openavbRawsockRxBufLevel(void *rawsock);

// returns number of TX out of buffer events noticed during streaming
unsigned long openavbRawsockGetTXOutOfBuffers(void *pvRawsock);

// returns number of TX out of buffer events noticed from the last reporting period
unsigned long openavbRawsockGetTXOutOfBuffersCyclic(void *pvRawsock);

#endif // RAWSOCK_H
