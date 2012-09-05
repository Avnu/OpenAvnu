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

#ifndef _E1000_HW_H_
#define _E1000_HW_H_

#include "e1000_osdep.h"
#include "e1000_regs.h"
#include "e1000_defines.h"

struct e1000_hw;

#define E1000_DEV_ID_82576			0x10C9
#define E1000_DEV_ID_82576_FIBER		0x10E6
#define E1000_DEV_ID_82576_SERDES		0x10E7
#define E1000_DEV_ID_82576_QUAD_COPPER		0x10E8
#define E1000_DEV_ID_82576_QUAD_COPPER_ET2	0x1526
#define E1000_DEV_ID_82576_NS			0x150A
#define E1000_DEV_ID_82576_NS_SERDES		0x1518
#define E1000_DEV_ID_82576_SERDES_QUAD		0x150D
#define E1000_DEV_ID_82576_VF			0x10CA
#define E1000_DEV_ID_I350_VF			0x1520
#define E1000_DEV_ID_82575EB_COPPER		0x10A7
#define E1000_DEV_ID_82575EB_FIBER_SERDES	0x10A9
#define E1000_DEV_ID_82575GB_QUAD_COPPER	0x10D6
#define E1000_DEV_ID_82580_COPPER		0x150E
#define E1000_DEV_ID_82580_FIBER		0x150F
#define E1000_DEV_ID_82580_SERDES		0x1510
#define E1000_DEV_ID_82580_SGMII		0x1511
#define E1000_DEV_ID_82580_COPPER_DUAL		0x1516
#define E1000_DEV_ID_82580_QUAD_FIBER		0x1527
#define E1000_DEV_ID_I350_COPPER		0x1521
#define E1000_DEV_ID_I350_FIBER			0x1522
#define E1000_DEV_ID_I350_SERDES		0x1523
#define E1000_DEV_ID_I350_SGMII			0x1524
#define E1000_DEV_ID_I350_DA4			0x1546
#define E1000_DEV_ID_I210_COPPER		0x1533
#define E1000_DEV_ID_I210_COPPER_OEM1		0x1534
#define E1000_DEV_ID_I210_COPPER_IT		0x1535
#define E1000_DEV_ID_I210_FIBER			0x1536
#define E1000_DEV_ID_I210_SERDES		0x1537
#define E1000_DEV_ID_I210_SGMII			0x1538
#define E1000_DEV_ID_I211_COPPER		0x1539
#define E1000_DEV_ID_DH89XXCC_SGMII		0x0438
#define E1000_DEV_ID_DH89XXCC_SERDES		0x043A
#define E1000_DEV_ID_DH89XXCC_BACKPLANE		0x043C
#define E1000_DEV_ID_DH89XXCC_SFP		0x0440
#define E1000_REVISION_0	0
#define E1000_REVISION_1	1
#define E1000_REVISION_2	2
#define E1000_REVISION_3	3
#define E1000_REVISION_4	4

#define E1000_FUNC_0		0
#define E1000_FUNC_1		1
#define E1000_FUNC_2		2
#define E1000_FUNC_3		3

#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN0	0
#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN1	3
#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN2	6
#define E1000_ALT_MAC_ADDRESS_OFFSET_LAN3	9

enum e1000_mac_type {
	e1000_undefined = 0,
	e1000_82575,
	e1000_82576,
	e1000_82580,
	e1000_i350,
	e1000_i210,
	e1000_i211,
	e1000_vfadapt,
	e1000_vfadapt_i350,
	e1000_num_macs  /* List is 1-based, so subtract 1 for TRUE count. */
};

#define __le16 u16
#define __le32 u32
#define __le64 u64
/* Receive Descriptor */
struct e1000_rx_desc {
	__le64 buffer_addr; /* Address of the descriptor's data buffer */
	__le16 length;      /* Length of data DMAed into data buffer */
	__le16 csum; /* Packet checksum */
	u8  status;  /* Descriptor status */
	u8  errors;  /* Descriptor Errors */
	__le16 special;
};

/* Receive Descriptor - Extended */
union e1000_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq; /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;  /* IP id */
					__le16 csum;   /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length;
			__le16 vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

#define MAX_PS_BUFFERS 4
/* Receive Descriptor - Packet Split */
union e1000_rx_desc_packet_split {
	struct {
		/* one buffer for protocol header(s), three data buffers */
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;  /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;    /* IP id */
					__le16 csum;     /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length0;  /* length of buffer 0 */
			__le16 vlan;  /* VLAN tag */
		} middle;
		struct {
			__le16 header_status;
			__le16 length[3];     /* length of buffers 1-3 */
		} upper;
		__le64 reserved;
	} wb; /* writeback */
};

/* Transmit Descriptor */
struct e1000_tx_desc {
	__le64 buffer_addr;   /* Address of the descriptor's data buffer */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 cso;  /* Checksum offset */
			u8 cmd;  /* Descriptor control */
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status; /* Descriptor status */
			u8 css;  /* Checksum start */
			__le16 special;
		} fields;
	} upper;
};

/* Offload Context Descriptor */
struct e1000_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;  /* IP checksum start */
			u8 ipcso;  /* IP checksum offset */
			__le16 ipcse;  /* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;  /* TCP checksum start */
			u8 tucso;  /* TCP checksum offset */
			__le16 tucse;  /* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 hdr_len;  /* Header length */
			__le16 mss;  /* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Offload data descriptor */
struct e1000_data_desc {
	__le64 buffer_addr;  /* Address of the descriptor's buffer address */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 typ_len_ext;
			u8 cmd;
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 popts;  /* Packet Options */
			__le16 special;
		} fields;
	} upper;
};

struct e1000_mac_info {
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];
	enum e1000_mac_type type;

};

struct e1000_hw {
	void *back;
	u8 *hw_addr;
	struct e1000_mac_info  mac;
	u16 device_id;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u16 vendor_id;

	u8  revision_id;
};

#endif
