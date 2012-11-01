/******************************************************************************

  Copyright (c) 2001-2012, Intel Corporation 
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
/*$FreeBSD$*/

#ifndef _IGB_INTERNAL_H_DEFINED_
#define _IGB_INTERNAL_H_DEFINED_

#include "igb.h"

/*
 * Micellaneous constants
 */
#define IGB_VENDOR_ID			0x8086

#define IGB_DEFAULT_PBA			0x00000030

#define PCI_ANY_ID                      (~0U)
#define ETHER_ALIGN                     2
#define IGB_TX_BUFFER_SIZE		((uint32_t) 1514)

#define IGB_TX_PTHRESH                  8
#define IGB_TX_HTHRESH                  1
#define IGB_TX_WTHRESH                  16

/*
 * TDBA/RDBA should be aligned on 16 byte boundary. But TDLEN/RDLEN should be
 * multiple of 128 bytes. So we align TDBA/RDBA on 128 byte boundary. This will
 * also optimize cache line size effect. H/W supports up to cache line size 128.
 */
#define IGB_DBA_ALIGN			128

#define SPEED_MODE_BIT (1<<21)		/* On PCI-E MACs only */

#define IGB_MAX_SCATTER		64
#define IGB_VFTA_SIZE		128
#define IGB_BR_SIZE		4096	/* ring buf size */
#define IGB_TSO_SIZE		(65535 + sizeof(struct ether_vlan_header))
#define IGB_TSO_SEG_SIZE	4096	/* Max dma segment size */
#define IGB_HDR_BUF		128
#define IGB_PKTTYPE_MASK	0x0000FFF0
#define ETH_ZLEN		60
#define ETH_ADDR_LEN		6


/* Queue bit defines */
#define IGB_QUEUE_IDLE                  1
#define IGB_QUEUE_WORKING               2
#define IGB_QUEUE_HUNG                  4
#define IGB_QUEUE_DEPLETED              8

/*
 * This parameter controls when the driver calls the routine to reclaim
 * transmit descriptors. Cleaning earlier seems a win.
 **/
#define IGB_TX_CLEANUP_THRESHOLD        (adapter->num_tx_desc / 2)
#define IGB_QUEUE_THRESHOLD		(2)

/* Precision Time Sync (IEEE 1588) defines */
#define ETHERTYPE_IEEE1588	0x88F7
#define PICOSECS_PER_TICK	20833
#define TSYNC_PORT		319 /* UDP port for the protocol */

struct igb_tx_buffer {
        int             next_eop;  	/* Index of the desc to watch */
	struct igb_packet *packet;	/* app-relevant handle */
	
};

/*
 * Transmit ring: one per queue
 */
struct tx_ring {
	struct adapter		*adapter;
	u32			me;
	struct resource		txdma;
	struct e1000_tx_desc	*tx_base;
	struct igb_tx_buffer	*tx_buffers;
	u32			next_avail_desc;
	u32			next_to_clean;
	volatile u16		tx_avail;

	u32			bytes;
	u32			packets;

	int			tdt;
	int			tdh;
	u64			no_desc_avail;
	u64			tx_packets;
	int			queue_status;
};

struct adapter {
	struct e1000_hw	hw;

	int 		ldev;	/* file descriptor to igb */

	struct resource	csr;
	int		max_frame_size;
	int		min_frame_size;
	int		igb_insert_vlan_header;
        u16		num_queues;

	/* Interface queues */
	struct igb_queue	*queues;

	/*
	 * Transmit rings
	 */
	struct tx_ring		*tx_rings;
        u16			num_tx_desc;

#ifdef IGB_IEEE1588
	/* IEEE 1588 precision time support */
	struct cyclecounter     cycles;
	struct nettimer         clock;
	struct nettime_compare  compare;
	struct hwtstamp_ctrl    hwtstamp;
#endif

};

/* ******************************************************************************
 * vendor_info_array
 *
 * This array contains the list of Subvendor/Subdevice IDs on which the driver
 * should load.
 *
 * ******************************************************************************/
typedef struct _igb_vendor_info_t {
	unsigned int vendor_id;
	unsigned int device_id;
	unsigned int subvendor_id;
	unsigned int subdevice_id;
	unsigned int index;
} igb_vendor_info_t;

/* external API requirements */
#define IGB_BIND       _IOW('E', 200, int)
#define IGB_UNBIND     _IOW('E', 201, int)
#define IGB_MAPRING    _IOW('E', 202, int)
#define IGB_UNMAPRING  _IOW('E', 203, int)
#define IGB_MAPBUF     _IOW('E', 204, int)
#define IGB_UNMAPBUF   _IOW('E', 205, int)
#define IGB_LINKSPEED  _IOW('E', 206, int)

#define IGB_BIND_NAMESZ 24

struct igb_bind_cmd {
        char    iface[IGB_BIND_NAMESZ];
        unsigned     mmap_size;
};

struct igb_buf_cmd {
        u_int64_t       physaddr; /* dma_addr_t is 64-bit */
        unsigned int    queue;
        unsigned int    mmap_size;
};

struct igb_link_cmd {
        u_int32_t 	up; /* dma_addr_t is 64-bit */
        u_int32_t	speed;
        u_int32_t	duplex;
};


#endif /* _IGB_H_DEFINED_ */


