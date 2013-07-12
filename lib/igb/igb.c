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

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>

#include "e1000_hw.h"
#include "e1000_82575.h"
#include "igb_internal.h"

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static igb_vendor_info_t igb_vendor_info_array[] =
{
	{ 0x8086, E1000_DEV_ID_I210_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_COPPER_IT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_COPPER_OEM1,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_SGMII,	PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{ 0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int	igb_read_mac_addr(struct e1000_hw *hw);
static int	igb_allocate_pci_resources(struct adapter *adapter);
static void	igb_free_pci_resources(struct adapter *adapter);
static void	igb_reset(struct adapter *adapter);
static int	igb_allocate_queues(struct adapter *adapter);
static void	igb_setup_transmit_structures(struct adapter *adapter);
static void	igb_setup_transmit_ring(struct tx_ring *txr);
static void	igb_initialize_transmit_units(struct adapter *adapter);
static void	igb_free_transmit_structures(struct adapter *adapter);
static void	igb_tx_ctx_setup(struct tx_ring *txr, struct igb_packet *packet);

int
igb_probe( device_t *dev )
{
	igb_vendor_info_t *ent;

	if (NULL == dev) return EINVAL;

	if (dev->pci_vendor_id != IGB_VENDOR_ID)
		return (ENXIO);

	ent = igb_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((dev->pci_vendor_id == ent->vendor_id) &&
		    (dev->pci_device_id == ent->device_id) ) {

			return (0);
		}
		ent++;
	}

	return (ENXIO);
}

int
igb_attach(char *dev_path, device_t *pdev)
{
	struct adapter	*adapter;
	struct igb_bind_cmd	bind;
	int		error = 0;

	if (NULL == pdev) return EINVAL;

	adapter = (struct adapter *)pdev->private_data;

	if (NULL != adapter) return EBUSY;

	/* allocate an adapter */
	pdev->private_data = malloc(sizeof(struct adapter));
	if (NULL == pdev->private_data) return ENXIO;

	memset(pdev->private_data, 0, sizeof(struct adapter));

	adapter = (struct adapter *)pdev->private_data;

	adapter->ldev = open("/dev/igb_avb", O_RDWR);
	if (adapter->ldev < 0) {
		free(pdev->private_data);
		pdev->private_data = NULL;
		return ENXIO;
	}

	/* 
	 * dev_path should look something "0000:01:00.0"
	 */

	strncpy(bind.iface, dev_path, IGB_BIND_NAMESZ - 1);

	if (ioctl(adapter->ldev, IGB_BIND, &bind) < 0) {
		close(adapter->ldev);
		free(pdev->private_data);
		pdev->private_data = NULL;
		return ENXIO;
	}

	adapter->csr.paddr = 0;
	adapter->csr.mmap_size = bind.mmap_size;

	/* Determine hardware and mac info */
	adapter->hw.vendor_id = pdev->pci_vendor_id;
	adapter->hw.device_id = pdev->pci_vendor_id;
	adapter->hw.revision_id = 0;
	adapter->hw.subsystem_vendor_id = 0;
	adapter->hw.subsystem_device_id = 0;

	/* Set MAC type early for PCI setup */
	adapter->hw.mac.type = e1000_i210;
	/* Setup PCI resources */
	if (error = igb_allocate_pci_resources(adapter)) {
		goto err_pci;
	}

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	adapter->max_frame_size = 1518;
	adapter->min_frame_size = 64;

	adapter->num_queues = 2;  /* XXX parameterize this */

	/*
	** Allocate and Setup Queues
	*/
	if (error = igb_allocate_queues(adapter)) {
		goto err_pci;
	}

	/*
	** Start from a known state, which means
	** reset the transmit queues we own to a known
	** starting state.
	*/
	igb_reset(adapter);

	/*
	** Copy the permanent MAC address out of the EEPROM
	*/
	if (igb_read_mac_addr(&adapter->hw) < 0) {
		error = EIO;
		goto err_late;
	}

	return (0);

err_late:
	igb_free_transmit_structures(adapter);
	igb_detach(pdev);
err_pci:
	igb_free_pci_resources(adapter);
	close(adapter->ldev);
	free(pdev->private_data);
	pdev->private_data = NULL;

	return (error);
}

int
igb_detach(device_t *dev)
{
	struct adapter	*adapter;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	igb_reset(adapter);

	igb_free_transmit_structures(adapter);
	igb_free_pci_resources(adapter);

	close (adapter->ldev);
	free(dev->private_data);
	dev->private_data = NULL;
	return (0);
}

int
igb_suspend(device_t *dev)
{
	struct adapter	*adapter;
	struct tx_ring	*txr;
	struct e1000_hw *hw;
	u32		txdctl;
	int	i;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	txr = adapter->tx_rings;
	hw = &adapter->hw;

	txdctl = 0;

	/* stop but don't reset the Tx Descriptor Rings */
	for (i = 0; i < adapter->num_queues; i++, txr++) {
		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
		txr->queue_status = IGB_QUEUE_IDLE;
	}

	return (0);
}

int
igb_resume(device_t *dev)
{
	struct adapter	*adapter;
	struct tx_ring	*txr;
	struct e1000_hw *hw;
	u32		txdctl;
	int	i;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	txr = adapter->tx_rings;
	hw = &adapter->hw;

	txdctl = 0;

	/* resume but don't reset the Tx Descriptor Rings */
	for (i = 0; i < adapter->num_queues; i++, txr++) {
		/* idle the queue */
		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		txdctl |= E1000_TXDCTL_PRIORITY;
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
		txr->queue_status = IGB_QUEUE_WORKING;
	}

	return (0);
}

int
igb_init(device_t *dev)
{
	struct adapter	*adapter;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	igb_reset(adapter);

	/* Prepare transmit descriptors and buffers */
	igb_setup_transmit_structures(adapter);
	igb_initialize_transmit_units(adapter);

	return(0);
}

static void 
igb_reset(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32		tctl, txdctl;
	int 	i;

	tctl = txdctl = 0;

	/* Setup the Tx Descriptor Rings, leave queues idle */
	for (i = 0; i < adapter->num_queues; i++, txr++) {
		u64 bus_addr = txr->txdma.paddr;

		/* idle the queue */
		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);

		/* reset the descriptor head/tail */
		E1000_WRITE_REG(hw, E1000_TDLEN(i),
		    adapter->num_tx_desc * sizeof(struct e1000_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
		    (u_int32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i),
		    (u_int32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		txr->queue_status = IGB_QUEUE_IDLE;
	}

}

static int
igb_read_mac_addr(struct e1000_hw *hw)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	rar_high = E1000_READ_REG(hw, E1000_RAH(0));
	rar_low = E1000_READ_REG(hw, E1000_RAL(0));

	for (i = 0; i < E1000_RAL_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i] = (u8)(rar_low >> (i*8));

	for (i = 0; i < E1000_RAH_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i+4] = (u8)(rar_high >> (i*8));

	for (i = 0; i < ETH_ADDR_LEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i];

	return E1000_SUCCESS;

}

static int
igb_allocate_pci_resources(struct adapter *adapter)
{
	int		dev = adapter->ldev;

	adapter->hw.hw_addr = (u8 *)mmap(NULL, \
		adapter->csr.mmap_size, \
		PROT_READ | PROT_WRITE, \
		MAP_SHARED, \
		dev, 
		0);

	if (MAP_FAILED == adapter->hw.hw_addr)
		return (ENXIO);

	return (0);
}

static void
igb_free_pci_resources(struct adapter *adapter)
{
	munmap( adapter->hw.hw_addr, adapter->csr.mmap_size);

	return ;
}

/*
 * Manage DMA'able memory.
 */
int
igb_dma_malloc_page(device_t *dev, struct igb_dma_alloc *dma)
{
	struct adapter	*adapter;
	int error = 0;
	struct igb_buf_cmd      ubuf;

	if (NULL == dev) return EINVAL;
	if (NULL == dma) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	error = ioctl(adapter->ldev, IGB_MAPBUF, &ubuf);
	if (error < 0) { error = ENOMEM; goto err; }

	dma->dma_paddr = ubuf.physaddr;
	dma->mmap_size = ubuf.mmap_size;
	dma->dma_vaddr = (void *)mmap(NULL, \
		ubuf.mmap_size, \
		PROT_READ | PROT_WRITE, \
		MAP_SHARED, \
		adapter->ldev, \
		ubuf.physaddr);

	if (MAP_FAILED == dma->dma_vaddr)
		error = ENOMEM;
err:
	return (error);
}

void
igb_dma_free_page(device_t *dev, struct igb_dma_alloc *dma)
{
	struct adapter	*adapter;
	struct igb_buf_cmd      ubuf;

	if (NULL == dev) return;
	if (NULL == dma) return;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return;

	munmap( dma->dma_vaddr, 
		dma->mmap_size);

	ubuf.physaddr = dma->dma_paddr;

	ioctl(adapter->ldev, IGB_UNMAPBUF, &ubuf);

	dma->dma_paddr = 0;
	dma->dma_vaddr = NULL;
	dma->mmap_size = 0;

	return;
}


/*********************************************************************
 *
 *  Allocate memory for the transmit rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int
igb_allocate_queues(struct adapter *adapter)
{
	int 			dev = adapter->ldev;
	struct igb_buf_cmd	ubuf;
	int 			i, error = E1000_SUCCESS;

	/* allocate the TX ring struct memory */
	adapter->tx_rings = (struct tx_ring *) malloc(sizeof(struct tx_ring) \
		* adapter->num_queues);
	if (NULL == adapter->tx_rings) {
		error = ENOMEM;
		goto tx_fail;
	}

	memset(adapter->tx_rings, 0, sizeof(struct tx_ring) * adapter->num_queues);

	for (i = 0; i < adapter->num_queues; i++) {
		ubuf.queue = i;
		error = ioctl(dev, IGB_MAPRING, &ubuf);
		if (error < 0) {
			error = EBUSY;
			goto tx_desc;
		}
		adapter->tx_rings[i].txdma.paddr = ubuf.physaddr;
		adapter->tx_rings[i].txdma.mmap_size = ubuf.mmap_size;
		adapter->tx_rings[i].tx_base = NULL;
		adapter->tx_rings[i].tx_base = (struct e1000_tx_desc *)mmap(NULL, \
				ubuf.mmap_size, \
				PROT_READ | PROT_WRITE, \
				MAP_SHARED, \
				adapter->ldev, \
				ubuf.physaddr);

		if (MAP_FAILED == adapter->tx_rings[i].tx_base) {
			error = ENOMEM;
			goto tx_desc;
		}
		adapter->tx_rings[i].adapter = adapter;
		adapter->tx_rings[i].me = i;
		/* XXX Initialize a TX lock ?? */
		adapter->num_tx_desc = ubuf.mmap_size / sizeof(union e1000_adv_tx_desc);

		memset((void *)adapter->tx_rings[i].tx_base, 0, ubuf.mmap_size);
		adapter->tx_rings[i].tx_buffers = (struct igb_tx_buffer *) malloc(sizeof(struct igb_tx_buffer) * \
			 adapter->num_tx_desc);

		if (NULL == adapter->tx_rings[i].tx_buffers) {
			error = ENOMEM;
			goto tx_desc;
		}

		memset(adapter->tx_rings[i].tx_buffers, 0, sizeof(struct igb_tx_buffer) * adapter->num_tx_desc);
	}

	return (0);

tx_desc:
	for (i = 0; i < adapter->num_queues; i++) {
		if (adapter->tx_rings[i].tx_base)
			munmap(adapter->tx_rings[i].tx_base, \
				adapter->tx_rings[i].txdma.mmap_size);
		ubuf.queue = i;
		ioctl(dev, IGB_UNMAPRING, &ubuf);
	};
tx_fail:
	free(adapter->tx_rings);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
igb_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;

	/* Clear the old descriptor contents */
	memset((void *)txr->tx_base, \
		0, \
	      (sizeof(union e1000_adv_tx_desc)) * adapter->num_tx_desc);

	memset(txr->tx_buffers, 0, sizeof(struct igb_tx_buffer) * txr->adapter->num_tx_desc);

	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static void
igb_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;
	int i;

	for (i = 0; i < adapter->num_queues; i++, txr++)
		igb_setup_transmit_ring(txr);

	return;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
igb_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32		tctl, txdctl;
	int	i;
	tctl = txdctl = 0;

	/* Setup the Tx Descriptor Rings */
	for (i = 0; i < adapter->num_queues; i++, txr++) {
		txdctl = 0;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);

		/* Setup the HW Tx Head and Tail descriptor pointers */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		txr->queue_status = IGB_QUEUE_IDLE;

		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		txdctl |= E1000_TXDCTL_PRIORITY;
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
	}

}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
igb_free_transmit_structures(struct adapter *adapter)
{
	int i;
	struct igb_buf_cmd	ubuf;

	for (i = 0; i < adapter->num_queues; i++) {
		if (adapter->tx_rings[i].tx_base)
			munmap(adapter->tx_rings[i].tx_base, \
				adapter->tx_rings[i].txdma.mmap_size);
		ubuf.queue = i;
		ioctl(adapter->ldev, IGB_UNMAPRING, &ubuf);
		free(adapter->tx_rings[i].tx_buffers);
	};

	free(adapter->tx_rings);
	adapter->tx_rings = NULL;
}


/*********************************************************************
 *
 *  Context Descriptor setup for VLAN or CSUM
 *
 **********************************************************************/

static void
igb_tx_ctx_setup(struct tx_ring *txr, struct igb_packet *packet)
{
	struct adapter *adapter = txr->adapter;
	struct e1000_adv_tx_context_desc *TXD;
	struct igb_tx_buffer	*tx_buffer;
	u32 type_tucmd_mlhl;
	int  ctxd;
	u_int64_t	remapped_time;

	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct e1000_adv_tx_context_desc *) &txr->tx_base[ctxd];

	type_tucmd_mlhl = E1000_ADVTXD_DCMD_DEXT | E1000_ADVTXD_DTYP_CTXT;

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens = 0;
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);
	TXD->mss_l4len_idx = 0;

	/* remap the 64-bit nsec time to the value represented in the desc */
	remapped_time = packet->attime - ((packet->attime / 1000000000)*1000000000);

	remapped_time /= 32; /* scale to 32 nsec increments */

	TXD->seqnum_seed = remapped_time;

	tx_buffer->packet = NULL;
	tx_buffer->next_eop = -1;


	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

	return;
}


/*********************************************************************
 *
 *  This routine maps a single buffer to an Advanced TX descriptor.
 *  returns ENOSPC if we run low on tx descriptors and the app needs to 
 *  cleanup descriptors.
 *
 *  this is a simplified routine which doesn't do LSO, checksum offloads,
 *  multiple fragments, etc. The provided buffers are assumed to have
 *  been previously mapped with the provided dma_malloc_page routines.
 *  
 **********************************************************************/

int
igb_xmit(device_t *dev, unsigned int queue_index, struct igb_packet *packet)
{
	struct adapter		*adapter;
	struct tx_ring	  *txr;
	struct igb_tx_buffer	*tx_buffer;
	union e1000_adv_tx_desc	*txd = NULL;
	u32			cmd_type_len, olinfo_status = 0;
	int			i, first, last = 0;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	if (queue_index > adapter->num_queues)
		return EINVAL;

	if (NULL == packet)
		return EINVAL;

	packet->next = NULL; /* used for cleanup */

	txr = &adapter->tx_rings[queue_index];

	/* Set basic descriptor constants */
	cmd_type_len = E1000_ADVTXD_DTYP_DATA;
	cmd_type_len |= E1000_ADVTXD_DCMD_IFCS | E1000_ADVTXD_DCMD_DEXT;

	/* cmd_type_len |= E1000_ADVTXD_DCMD_VLE; to enable VLAN insertion */

	/*
	 * Map the packet for DMA
	 *
	 * Capture the first descriptor index,
	 * this descriptor will have the index
	 * of the EOP which is the only one that
	 * now gets a DONE bit writeback.
	 */
	first = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[first];

	/*
	** Make sure we don't overrun the ring,
	** we need nsegs descriptors and one for
	** the context descriptor used for the
	** offloads.
	*/
	if (txr->tx_avail <= 2)
		return (ENOSPC);

	/* 
	 * Set up the context descriptor to specify
	 * launchtimes for the packet.
	 */
	igb_tx_ctx_setup(txr, packet);

	/* 
	 * for performance monitoring, report the DMA time of the tx desc wb
	 */
	olinfo_status |= E1000_TXD_DMA_TXDWB;

	/* set payload length */
	olinfo_status |= packet->len << E1000_ADVTXD_PAYLEN_SHIFT;

	/* Set up our transmit descriptors */
	i = txr->next_avail_desc;

	/* we assume every packet is contiguous */

	tx_buffer = &txr->tx_buffers[i];
	txd = (union e1000_adv_tx_desc *)&txr->tx_base[i];

	txd->read.buffer_addr = htole64(packet->map.paddr + packet->offset);
	txd->read.cmd_type_len = htole32(cmd_type_len | packet->len);
	txd->read.olinfo_status = htole32(olinfo_status);
	last = i;
	if (++i == adapter->num_tx_desc)
		i = 0;
	tx_buffer->packet = NULL;
	tx_buffer->next_eop = -1;

	txr->next_avail_desc = i;
	txr->tx_avail-- ;
	tx_buffer->packet = packet;


	/*
	 * Last Descriptor of Packet
	 * needs End Of Packet (EOP)
	 * and Report Status (RS)
	 */
	txd->read.cmd_type_len |=
	    htole32(E1000_ADVTXD_DCMD_EOP | E1000_ADVTXD_DCMD_RS);

	/*
	 * Keep track in the first buffer which
	 * descriptor will be written back
	 */
	tx_buffer = &txr->tx_buffers[first];
	tx_buffer->next_eop = last;

	/*
	 * Advance the Transmit Descriptor Tail (TDT), this tells the E1000
	 * that this frame is available to transmit.
	 */
	E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), i);
	++txr->tx_packets;

	return(0);

}

void
igb_trigger(device_t *dev, u_int32_t data) 
{
	struct adapter	*adapter;

	if (NULL == dev) return;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return;

	E1000_WRITE_REG(&(adapter->hw), E1000_WUS, data);
}

void
igb_writereg(device_t *dev, u_int32_t reg, u_int32_t data) 
{
	struct adapter	*adapter;

	if (NULL == dev) return;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return;

	E1000_WRITE_REG(&(adapter->hw), reg, data);
}

void
igb_readreg(device_t *dev, u_int32_t reg, u_int32_t *data) 
{
	struct adapter	*adapter;

	if (NULL == dev) return;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return;

	if (NULL == data) return;

	*data = E1000_READ_REG(&(adapter->hw), reg);
}

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then return the linked list of associated resources. 
 *
 **********************************************************************/
void
igb_clean(device_t *dev, struct igb_packet **cleaned_packets)
{
	struct adapter	*adapter;
	struct tx_ring	  *txr;
	int first, last, done, processed;
	struct igb_tx_buffer *tx_buffer;
	struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct igb_packet *last_reclaimed;
	int i;

	if (NULL == dev) return;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return;

	if (NULL == cleaned_packets) return;

	*cleaned_packets = NULL; /* nothing reclaimed yet */

	for (i = 0; i < adapter->num_queues; i++) {
		txr = &adapter->tx_rings[i];

		if (txr->tx_avail == adapter->num_tx_desc) {
			txr->queue_status = IGB_QUEUE_IDLE;
			return;
		}
	
		processed = 0;
		first = txr->next_to_clean;
		tx_desc = &txr->tx_base[first];
		tx_buffer = &txr->tx_buffers[first];
		last = tx_buffer->next_eop;
		eop_desc = &txr->tx_base[last];
	
		/*
		 * What this does is get the index of the
		 * first descriptor AFTER the EOP of the 
		 * first packet, that way we can do the
		 * simple comparison on the inner while loop.
		 */
		if (++last == adapter->num_tx_desc)
	 		last = 0;
		done = last;
	
		while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
			/* We clean the range of the packet */
			while (first != done) {
				if (tx_buffer->packet) {
					tx_buffer->packet->dmatime = (0xffffffff) & tx_desc->buffer_addr;
					/* tx_buffer->packet->dmatime += (tx_desc->buffer_addr >> 32) * 1000000000; */
					txr->bytes +=
					    tx_buffer->packet->len;
					if (*cleaned_packets == NULL) 
						*cleaned_packets = tx_buffer->packet;
					else 
						last_reclaimed->next = tx_buffer->packet;
					last_reclaimed = tx_buffer->packet;
					
					tx_buffer->packet = NULL;
				}
				tx_buffer->next_eop = -1;
				tx_desc->upper.data = 0;
				tx_desc->lower.data = 0;
				tx_desc->buffer_addr = 0;
				++txr->tx_avail;
				++processed;
	
	
				if (++first == adapter->num_tx_desc)
					first = 0;
	
				tx_buffer = &txr->tx_buffers[first];
				tx_desc = &txr->tx_base[first];
			}
			++txr->packets;
			/* See if we can continue to the next packet */
			last = tx_buffer->next_eop;
			if (last != -1) {
				eop_desc = &txr->tx_base[last];
				/* Get new done point */ if (++last == adapter->num_tx_desc) last = 0;
				done = last;
			} else
				break;
		}
	
		txr->next_to_clean = first;
	
		if (txr->tx_avail >= IGB_QUEUE_THRESHOLD)	  
			txr->queue_status &= ~IGB_QUEUE_DEPLETED;
	}	
	return;
}

#define MAX_WALLCLOCK_WINDOW 1000
#define rdtscpll(val)    __asm__ __volatile__("rdtsc" : "=A" (val))

int
igb_get_wallclock(device_t *dev, u_int64_t	*curtime, u_int64_t *rdtsc)
{
	u_int64_t	t0 = 0, t1 = -1;
	u_int32_t	timh, timl, tsauxc;
	struct adapter	*adapter;
	struct e1000_hw *hw;
	int iter = 0;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	hw = &adapter->hw;

	/* sample the timestamp bracketed by the RDTSC */
	while( t1 - t0 > MAX_WALLCLOCK_WINDOW && iter < 8 ) {
	    tsauxc = E1000_READ_REG(hw, E1000_TSAUXC);
	    tsauxc &= ~(E1000_TSAUXC_SAMP_AUTO);
	    E1000_WRITE_REG(hw, E1000_TSAUXC, tsauxc);
	    tsauxc |= E1000_TSAUXC_SAMP_AUTO;
	    rdtscpll(t0);
	    E1000_WRITE_REG(hw, E1000_TSAUXC, tsauxc);
	    rdtscpll(t1);
	    ++iter;
	}

	if( t1 - t0 > MAX_WALLCLOCK_WINDOW ) {
	  return ERESTART;
	}

	timl = E1000_READ_REG(hw, E1000_AUXSTMPL0);
	timh = E1000_READ_REG(hw, E1000_AUXSTMPH0);

	if( curtime ) *curtime = (u_int64_t)timh * 1000000000 + (u_int64_t)timl;
	if( rdtsc ) *rdtsc = (t1 - t0) / 2 + t0; /* average */
	
	return(0);
} 
int	
igb_set_class_bandwidth(device_t *dev, 
	u_int32_t class_a,
	u_int32_t class_b,
	u_int32_t tpktsz_a,
	u_int32_t tpktsz_b) 
{
	u_int32_t	tqavctrl;
	u_int32_t	tqavcc0, tqavcc1;
	u_int32_t	tqavhc0, tqavhc1;
	u_int32_t	class_a_idle, class_b_idle;
	u_int32_t	linkrate;
	struct adapter	*adapter;
	struct e1000_hw *hw;
	struct igb_link_cmd	link;
	int	err;
	float		class_a_percent, class_b_percent;

	if (NULL == dev) return EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (NULL == adapter) return ENXIO;

	hw = &adapter->hw;

	/* get current link speed */

	err = ioctl(adapter->ldev, IGB_LINKSPEED, &link);

	if (err) return ENXIO;

	if (0 == link.up) return EINVAL;

	if (link.speed < 100) return EINVAL;

	if (link.duplex != FULL_DUPLEX ) return EINVAL;

	if (tpktsz_a < 64) 
		tpktsz_a = 64; /* minimum ethernet frame size */

	if (tpktsz_a > 1500)
		return EINVAL;

	if (tpktsz_b < 64) 
		tpktsz_b = 64; /* minimum ethernet frame size */

	if (tpktsz_b > 1500)
		return EINVAL;

	tqavctrl = E1000_READ_REG(hw, E1000_TQAVCTRL);

	if ((class_a + class_b) == 0 ) {
		/* disable the Qav shaper */
		tqavctrl &= ~E1000_TQAVCTRL_TX_ARB;
		E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);
		return(0);
	}

	tqavcc0 = E1000_TQAVCC_QUEUEMODE;
	tqavcc1 = E1000_TQAVCC_QUEUEMODE;
	
	if (link.speed == 100) 
	    linkrate = E1000_TQAVCC_LINKRATE / 10;
	else
	    linkrate = E1000_TQAVCC_LINKRATE;

	/* 
	 * class_a and class_b are the packets-per-(respective)observation
	 * interval (125 usec for class A, 250 usec for class B)
	 * these parameters are also used when establishing the MSRP
	 * talker advertise attribute (as well as the tpktsize)
	 *
	 * note that class_a and class_b are independent of the media
	 * rate. For our idle slope calculation, we need to scale the
	 * (tpktsz + (media overhead)) * rate -> percentage of media rate.
	 */

	/* 12=Ethernet IPG, 8=Preamble+Start of Frame, 18=Mac Header with VLAN+Etype, 4=CRC */
	class_a_percent = (float)((tpktsz_a + (12 + 8 + 18 + 4)) * class_a) ;
	class_b_percent = (float)((tpktsz_b + (12 + 8 + 18 + 4)) * class_b) ;

	class_a_percent /= 0.000125; /* class A observation window */
	class_b_percent /= 0.000250; /* class B observation window */

	if (link.speed == 100) {
		class_a_percent /= (100000000.0 / 8); /* bytes-per-sec @ 100Mbps */
		class_b_percent /= (100000000.0 / 8); 
		class_a_idle = (u_int32_t)(class_a_percent * 0.2 * (float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 0.2 * (float)linkrate + 0.5);
	} else {
		class_a_percent /= (1000000000.0 / 8); /* bytes-per-sec @ 1Gbps */
		class_b_percent /= (1000000000.0 / 8); 
		class_a_idle = (u_int32_t)(class_a_percent * 2.0 * (float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 2.0 * (float)linkrate + 0.5);
	}

	if ((class_a_percent + class_b_percent) > 0.75)
		return EINVAL;
	tqavcc0 |= class_a_idle;
	tqavcc1 |= class_b_idle;

	/* 
	 * hiCredit is the number of idleslope credits accumulated due to delay T
	 *
	 * we assume the maxInterferenceSize is 18 + 4 + 1500 (1522).
	 * Note: if EEE is enabled, we should use for maxInterferenceSize
	 * the overhead of link recovery (a media-specific quantity).
	 */
	tqavhc0 = 0x80000000 + (class_a_idle * 1522 / linkrate ); /* L.10 */

	/* 
	 * Class B high credit is is the same, except the delay
	 * is the MaxBurstSize of Class A + maxInterferenceSize of non-SR traffic
	 *
	 * L.41
	 * max Class B delay = (1522 + tpktsz_a) / (linkrate - class_a_idle)
	 */

	tqavhc1 = 0x80000000 + (class_b_idle * ((1522 + tpktsz_a)/ (linkrate - class_a_idle)));

	/* implicitly enable the Qav shaper */
	tqavctrl |= E1000_TQAVCTRL_TX_ARB;
	E1000_WRITE_REG(hw, E1000_TQAVHC(0), tqavhc0);
	E1000_WRITE_REG(hw, E1000_TQAVCC(0), tqavcc0);
	E1000_WRITE_REG(hw, E1000_TQAVHC(1), tqavhc1);
	E1000_WRITE_REG(hw, E1000_TQAVCC(1), tqavcc1);
	E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);

	return(0);
}

