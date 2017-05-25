/*************************************************************************************
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
*************************************************************************************/

/**
 * @file
 *
 * @brief Support for platform-specific networking operations
 *
 * These functions support the transmission of raw networking packets.
 * The functions are generic, and allow for a platform-specific network implementation.
 *
 * The buffers are expected to be raw Ethernet packets.  In other words,
 * the first 12 bytes should be the destination and source MAC Addresses.
 */

#ifndef MAAP_NET_H
#define MAAP_NET_H

/** Size in bytes of the raw network packets to handle. */
#define MAAP_NET_BUFFER_SIZE 64

/** Structure to hold the networking information.  @b struct @b maap_net will be defined in the platform-specific implementation file. */
typedef struct maap_net Net;

/**
 * Create a new #Net pointer to use for packet transmission support.
 *
 * @return A #Net pointer, or NULL if an error occurred.
 */
Net *Net_newNet(void);

/**
 * Delete a #Net pointer when done with the packet transmission.
 *
 * @param net The pointer to free
 */
void Net_delNet(Net *net);

/**
 * Get a buffer to fill with the packet contents.
 *
 * @param net The #Net pointer from #Net_newNet to use
 *
 * @return The buffer to fill.  The buffer will be #MAAP_NET_BUFFER_SIZE bytes in size.
 */
void *Net_getPacketBuffer(Net *net);

/**
 * Queue a packet buffer for transmission.
 *
 * @param net The #Net pointer from #Net_newNet to use
 * @param buffer The buffer from #Net_getPacketBuffer that was filled
 *
 * @return 0 if the buffer was queued, or -1 if an error occurred.
 */
int Net_queuePacket(Net *net, void *buffer);

/**
 * Get the next packet from the queue.
 *
 * @param net The #Net pointer from #Net_newNet to use
 *
 * @return The buffer with the network data to transmit, or NULL if an error occurred.
 */
void *Net_getNextQueuedPacket(Net *net);

/**
 * Free a packet after it has been transmitted.
 *
 * @param net The #Net pointer from #Net_newNet to use
 * @param buffer The buffer from #Net_getNextQueuedPacket that was filled
 *
 * @return 0 if the buffer was freed, or -1 if an error occurred.
 */
int Net_freeQueuedPacket(Net *net, void *buffer);

#endif
