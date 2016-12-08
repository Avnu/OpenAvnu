/******************************************************************************

  Copyright (c) 2001-2016, Intel Corporation
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

#include <time.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <stdint.h>
#include <semaphore.h>

#include "e1000_hw.h"
#include "e1000_82575.h"
#include "igb_internal.h"

#undef DEBUG

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static igb_vendor_info_t igb_vendor_info_array[] = {
	{ 0x8086, E1000_DEV_ID_I210_COPPER, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_COPPER_FLASHLESS,
		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_COPPER_IT, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_COPPER_OEM1,
		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_FIBER, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_SERDES, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_SERDES_FLASHLESS,
		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_I210_SGMII, PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{ 0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int igb_read_mac_addr(struct e1000_hw *hw);
static int igb_allocate_pci_resources(struct adapter *adapter);
static void igb_free_pci_resources(struct adapter *adapter);
static void igb_reset(struct adapter *adapter);
static int igb_allocate_queues(struct adapter *adapter);
static int igb_allocate_rx_queues(struct adapter *adapter);
static void igb_setup_transmit_structures(struct adapter *adapter);
static void igb_setup_receive_structures(struct adapter *adapter);
static void igb_setup_transmit_ring(struct tx_ring *txr);
static void igb_initialize_transmit_units(struct adapter *adapter);
static void igb_initialize_receive_units(struct adapter *adapter);
static void igb_free_transmit_structures(struct adapter *adapter);
static void igb_free_receive_structures(struct adapter *adapter);
static void igb_tx_ctx_setup(struct tx_ring *txr, struct igb_packet *packet);
static void igb_free_receive_buffers(struct rx_ring *rxr);

int igb_probe(device_t *dev)
{
	igb_vendor_info_t *ent;

	if (dev == NULL)
		return -EINVAL;

	if (dev->pci_vendor_id != IGB_VENDOR_ID)
		return -ENXIO;

	ent = igb_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((dev->pci_vendor_id == ent->vendor_id) &&
		    (dev->pci_device_id == ent->device_id)) {

			return 0;
		}
		ent++;
	}

	return -ENXIO;
}

#define IGB_SEM "igb_sem"

int igb_attach(char *dev_path, device_t *pdev)
{
	struct adapter *adapter;
	struct igb_bind_cmd	bind = {0};
	int error = 0;

	if (pdev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)pdev->private_data;

	if (adapter != NULL)
		return -EBUSY;

	/* allocate an adapter */
	pdev->private_data = malloc(sizeof(struct adapter));
	if (pdev->private_data == NULL)
		return -ENXIO;

	memset(pdev->private_data, 0, sizeof(struct adapter));

	adapter = (struct adapter *)pdev->private_data;

	adapter->ldev = open("/dev/igb_avb", O_RDWR);

	if (adapter->ldev < 0) {
		error = -ENXIO;
		goto err_prebind;
	}

	adapter->memlock =
		sem_open(IGB_SEM, O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP, 1);

	if (adapter->memlock == ((sem_t *)SEM_FAILED)) {
		error = errno;
		close(adapter->ldev);
		goto err_prebind;
	}

	if (sem_wait(adapter->memlock) != 0) {
		error = errno;
		close(adapter->ldev);
		sem_close(adapter->memlock);
		goto err_prebind;
	}

	/*
	 * dev_path should look something "0000:01:00.0"
	 */

	strncpy(bind.iface, dev_path, IGB_BIND_NAMESZ - 1);

	if (ioctl(adapter->ldev, IGB_BIND, &bind) < 0) {
		error = -ENXIO;
		goto err_bind;
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
	error = igb_allocate_pci_resources(adapter);
	if (error)
		goto err_pci;

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	adapter->max_frame_size = 1518;
	adapter->min_frame_size = 64;

	/*
	 * Copy the permanent MAC address out of the EEPROM
	 */
	if (igb_read_mac_addr(&adapter->hw) < 0) {
		error = -EIO;
		goto err_late;
	}

	if (sem_post(adapter->memlock) != 0) {
		error = -errno;
		goto err_gen;
	}

	return 0;

err_late:
err_pci:
	igb_free_pci_resources(adapter);
err_bind:
	sem_post(adapter->memlock);
	sem_close(adapter->memlock);
	close(adapter->ldev);
err_prebind:
	free(pdev->private_data);
	pdev->private_data = NULL;

err_gen:
	return error;
}

int igb_attach_tx(device_t *pdev)
{
	int error;
	struct adapter *adapter;

	if (pdev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)pdev->private_data;

	if (adapter == NULL)
		return -EINVAL;

	if (sem_wait(adapter->memlock) != 0)
		return -errno;

	/* Allocate and Setup Queues */
	adapter->num_queues = 2;  /* XXX parameterize this */
	error = igb_allocate_queues(adapter);
	if (error) {
		adapter->num_queues = 0;
		goto release;
	}

	/*
	 * Start from a known state, which means
	 * reset the transmit queues we own to a known
	 * starting state.
	 */
	igb_reset(adapter);

release:
	if (sem_post(adapter->memlock) != 0)
		return -errno;

	return error;
}

int igb_attach_rx(device_t *pdev)
{
	int error;
	struct adapter *adapter;

	if (pdev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)pdev->private_data;

	if (adapter == NULL)
		return -EINVAL;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	/*
	 * Allocate and Setup Rx Queues
	 */
	adapter->num_queues = 2;  /* XXX parameterize this */
	error = igb_allocate_rx_queues(adapter);
	if (error) {
		adapter->num_queues = 0;
		goto release;
	}

release:
	if (sem_post(adapter->memlock) != 0)
		return errno;

	return error;
}

int igb_detach(device_t *dev)
{
	struct adapter *adapter;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (sem_wait(adapter->memlock) != 0)
		goto err_nolock;

	igb_reset(adapter);

	sem_post(adapter->memlock);

	igb_free_pci_resources(adapter);

	if (adapter->tx_rings)
		igb_free_transmit_structures(adapter);

	if (adapter->rx_rings)
		igb_free_receive_structures(adapter);

err_nolock:
	sem_close(adapter->memlock);

	close(adapter->ldev);

	free(dev->private_data);
	dev->private_data = NULL;
	return 0;
}

int igb_suspend(device_t *dev)
{
	struct adapter *adapter;
	struct tx_ring *txr;
	struct rx_ring *rxr;
	struct e1000_hw *hw;
	u32 txdctl, srrctl = 0;
	int i;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	txr = adapter->tx_rings;
	rxr = adapter->rx_rings;
	hw = &adapter->hw;

	txdctl = 0;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	/* stop but don't reset the Tx Descriptor Rings */
	for (i = 0; i < adapter->num_queues; i++, txr++) {
		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
		txr->queue_status = IGB_QUEUE_IDLE;
	}

	srrctl |= 2048 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 bus_addr = rxr->rxdma.paddr;
		u32 rxdctl;

		E1000_WRITE_REG(hw, E1000_RDLEN(i),
				adapter->num_rx_desc *
				sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i),
				(uint32_t)bus_addr);
		E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);
		/* Enable this Queue */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
	}

	if (sem_post(adapter->memlock) != 0)
		return errno;

	return 0;
}

int igb_resume(device_t *dev)
{
	struct adapter *adapter;
	struct tx_ring *txr;
	struct rx_ring *rxr;
	struct e1000_hw *hw;
	u32 txdctl, srrctl = 0;
	int i;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	txr = adapter->tx_rings;
	rxr = adapter->rx_rings;
	hw = &adapter->hw;

	txdctl = 0;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

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

	srrctl |= 2048 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 bus_addr = rxr->rxdma.paddr;
		u32 rxdctl;

		E1000_WRITE_REG(hw, E1000_RDLEN(i),
				adapter->num_rx_desc *
				sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i),
				(uint32_t)bus_addr);
		E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);
		/* Enable this Queue */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
	}

	if (sem_post(adapter->memlock) != 0)
		return errno;

	return 0;
}

int igb_init(device_t *dev)
{
	struct adapter *adapter;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	igb_reset(adapter);

	/* Prepare transmit descriptors and buffers */
	if (adapter->tx_rings) {
		igb_setup_transmit_structures(adapter);
		igb_initialize_transmit_units(adapter);
	}

	if (adapter->rx_rings) {
		igb_setup_receive_structures(adapter);
		igb_initialize_receive_units(adapter);
	}

	if (sem_post(adapter->memlock) != 0)
		return errno;

	return 0;
}

static void
igb_reset(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32 txdctl, srrctl;
	int i;

	srrctl = 0;
	txdctl = 0;

	/* Set up the Tx Descriptor Rings, leave queues idle */
	if (adapter->tx_rings == NULL) {
#if DEBUG
		printf("txr null\n");
#endif
	} else {
		for (i = 0; i < adapter->num_queues; i++, txr++) {
			u64 bus_addr = txr->txdma.paddr;

			/* idle the queue */
			txdctl |= IGB_TX_PTHRESH;
			txdctl |= IGB_TX_HTHRESH << 8;
			txdctl |= IGB_TX_WTHRESH << 16;
			E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);

			/* reset the descriptor head/tail */
			E1000_WRITE_REG(hw, E1000_TDLEN(i),
					adapter->num_tx_desc *
					sizeof(struct e1000_tx_desc));
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

	srrctl |= 2048 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

	/* Setup the Base and Length of the Rx Descriptor Rings */
	if (adapter->rx_rings == NULL) {
#if DEBUG
		printf("rxr null\n");
#endif
	} else {

		for (i = 0; i < adapter->num_queues; i++, rxr++) {
			u64 bus_addr = rxr->rxdma.paddr;
			u32 rxdctl;

			/* Disable this Queue */
			E1000_WRITE_REG(hw, E1000_RXDCTL(i), 0);

			E1000_WRITE_REG(hw, E1000_RDLEN(i),
					adapter->num_rx_desc *
					sizeof(union e1000_adv_rx_desc));
			E1000_WRITE_REG(hw, E1000_RDBAH(i),
					(uint32_t)(bus_addr >> 32));
			E1000_WRITE_REG(hw, E1000_RDBAL(i),
					(uint32_t)bus_addr);
			E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);

			/* Enable this Queue */
			rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
			rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
			rxdctl &= 0xFFF00000;
			rxdctl |= IGB_RX_PTHRESH;
			rxdctl |= IGB_RX_HTHRESH << 8;
			rxdctl |= IGB_RX_WTHRESH << 16;
			E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
		}
	}
}

static int igb_read_mac_addr(struct e1000_hw *hw)
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

	return 0;

}

static int igb_allocate_pci_resources(struct adapter *adapter)
{
	int dev = adapter->ldev;

	adapter->hw.hw_addr = (u8 *)mmap(NULL, adapter->csr.mmap_size,
					 PROT_READ | PROT_WRITE, MAP_SHARED,
					 dev, 0);

	if (adapter->hw.hw_addr == MAP_FAILED)
		return -ENXIO;

	return 0;
}

static void igb_free_pci_resources(struct adapter *adapter)
{
	munmap(adapter->hw.hw_addr, adapter->csr.mmap_size);
}

/*
 * Manage DMA'able memory.
 */
int igb_dma_malloc_page(device_t *dev, struct igb_dma_alloc *dma)
{
	struct adapter *adapter;
	int error = 0;
	struct igb_buf_cmd ubuf = {0};

	if (dev == NULL)
		return -EINVAL;
	if (dma == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (sem_wait(adapter->memlock) != 0) {
		error = errno;
		goto err;
	}
	error = ioctl(adapter->ldev, IGB_MAPBUF, &ubuf);
	if (sem_post(adapter->memlock) != 0) {
		error = errno;
		goto err;
	}

	if (error < 0) {
		error = -ENOMEM;
		goto err;
	}

	dma->dma_paddr = ubuf.physaddr;
	dma->mmap_size = ubuf.mmap_size;
	dma->dma_vaddr = (void *)mmap(NULL,
				      ubuf.mmap_size,
				      PROT_READ | PROT_WRITE,
				      MAP_SHARED,
				      adapter->ldev,
				      ubuf.physaddr);

	if (dma->dma_vaddr == MAP_FAILED)
		error = -ENOMEM;
err:
	return error;
}

void igb_dma_free_page(device_t *dev, struct igb_dma_alloc *dma)
{
	struct adapter *adapter;
	struct igb_buf_cmd      ubuf;

	if (dev == NULL)
		return;
	if (dma == NULL)
		return;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return;

	munmap(dma->dma_vaddr,
		dma->mmap_size);

	ubuf.physaddr = dma->dma_paddr;

	if (sem_wait(adapter->memlock) != 0)
		goto err;

	ioctl(adapter->ldev, IGB_UNMAPBUF, &ubuf);
	if (sem_post(adapter->memlock) != 0)
		goto err;

	dma->dma_paddr = 0;
	dma->dma_vaddr = NULL;
	dma->mmap_size = 0;

err:
	return;
}

/*********************************************************************
 *
 *  Allocate memory for the transmit rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int igb_allocate_queues(struct adapter *adapter)
{
	struct igb_buf_cmd ubuf = {0};
	int dev = adapter->ldev;
	int i, error = 0;

	/* allocate the TX ring struct memory */
	adapter->tx_rings = (struct tx_ring *) malloc(sizeof(struct tx_ring) *
						      adapter->num_queues);

	if (adapter->tx_rings == NULL) {
		error = -ENOMEM;
		goto tx_fail;
	}

	memset(adapter->tx_rings, 0, sizeof(struct tx_ring) *
					    adapter->num_queues);

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
		adapter->tx_rings[i].tx_base =
			(struct e1000_tx_desc *)mmap(NULL, ubuf.mmap_size,
						     PROT_READ | PROT_WRITE,
						     MAP_SHARED, adapter->ldev,
						     ubuf.physaddr);

		if (adapter->tx_rings[i].tx_base == MAP_FAILED) {
			error = -ENOMEM;
			goto tx_desc;
		}

		adapter->tx_rings[i].adapter = adapter;
		adapter->tx_rings[i].me = i;
		/* XXX Initialize a TX lock ?? */
		adapter->num_tx_desc = ubuf.mmap_size /
				       sizeof(union e1000_adv_tx_desc);

		memset((void *)adapter->tx_rings[i].tx_base, 0, ubuf.mmap_size);
		adapter->tx_rings[i].tx_buffers =
			(struct igb_tx_buffer *)
				malloc(sizeof(struct igb_tx_buffer) *
				       adapter->num_tx_desc);

		if (adapter->tx_rings[i].tx_buffers == NULL) {
			error = -ENOMEM;
			goto tx_desc;
		}

		memset(adapter->tx_rings[i].tx_buffers, 0,
		       sizeof(struct igb_tx_buffer) * adapter->num_tx_desc);
	}

	return 0;

tx_desc:
	for (i = 0; i < adapter->num_queues; i++) {
		if (adapter->tx_rings[i].tx_base)
			munmap(adapter->tx_rings[i].tx_base,
			       adapter->tx_rings[i].txdma.mmap_size);
		ubuf.queue = i;
		ioctl(dev, IGB_UNMAPRING, &ubuf);
	};
tx_fail:
	free(adapter->tx_rings);
	adapter->tx_rings = NULL;
	return error;
}

/* Initialize a transmit ring. */
static void igb_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;

	/* Clear the old descriptor contents */
	memset((void *)txr->tx_base,  0,
	       (sizeof(union e1000_adv_tx_desc)) * adapter->num_tx_desc);

	memset(txr->tx_buffers, 0, sizeof(struct igb_tx_buffer) *
				   txr->adapter->num_tx_desc);

	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;
}

/*  Initialize all transmit rings. */
static void igb_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;
	int i;

	for (i = 0; i < adapter->num_queues; i++, txr++)
		igb_setup_transmit_ring(txr);
}

/*Enable transmit unit. */
static void igb_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32 txdctl;
	int i;

	txdctl = 0;

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

/* Free all transmit rings. */
static void igb_free_transmit_structures(struct adapter *adapter)
{
	int i;
	struct igb_buf_cmd ubuf = {0};

	for (i = 0; i < adapter->num_queues; i++) {
		if (adapter->tx_rings[i].tx_base)
			munmap(adapter->tx_rings[i].tx_base,
			       adapter->tx_rings[i].txdma.mmap_size);
		ubuf.queue = i;
		ioctl(adapter->ldev, IGB_UNMAPRING, &ubuf);
		free(adapter->tx_rings[i].tx_buffers);
	}

	free(adapter->tx_rings);
	adapter->tx_rings = NULL;
}

/* Context Descriptor setup for VLAN or CSUM */
static void igb_tx_ctx_setup(struct tx_ring *txr, struct igb_packet *packet)
{
	struct adapter *adapter = txr->adapter;
	struct e1000_adv_tx_context_desc *TXD;
	struct igb_tx_buffer *tx_buffer;
	u32 type_tucmd_mlhl;
	int ctxd;
	u_int64_t remapped_time;

	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct e1000_adv_tx_context_desc *) &txr->tx_base[ctxd];

	type_tucmd_mlhl = E1000_ADVTXD_DCMD_DEXT | E1000_ADVTXD_DTYP_CTXT;

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens = 0;
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);
	TXD->mss_l4len_idx = 0;

	/* remap the 64-bit nsec time to the value represented in the desc */
	remapped_time = packet->attime - ((packet->attime / 1000000000) *
					  1000000000);

	remapped_time /= 32; /* scale to 32 nsec increments */

	TXD->seqnum_seed = remapped_time;

	tx_buffer->packet = NULL;
	tx_buffer->next_eop = -1;


	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;
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
int igb_xmit(device_t *dev, unsigned int queue_index, struct igb_packet *packet)
{
	struct adapter *adapter;
	struct tx_ring *txr;
	struct igb_tx_buffer *tx_buffer;
	union e1000_adv_tx_desc *txd = NULL;
	u32 cmd_type_len, olinfo_status = 0;
	int i, first, last = 0;
	int error = 0;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	txr = &adapter->tx_rings[queue_index];
	if (!txr)
		return -EINVAL;

	if (queue_index > adapter->num_queues)
		return -EINVAL;

	if (packet == NULL)
		return -EINVAL;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	packet->next = NULL; /* used for cleanup */

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
	 * Make sure we don't overrun the ring,
	 * we need nsegs descriptors and one for
	 * the context descriptor used for the
	 * offloads.
	 */
	if (txr->tx_avail <= 2) {
		error = ENOSPC;
		goto unlock;
	}

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
	txr->tx_avail--;
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

unlock:
	if (sem_post(adapter->memlock) != 0)
		return errno;

	return error;
}

void igb_trigger(device_t *dev, u_int32_t data)
{
	struct adapter *adapter;

	if (dev == NULL)
		return;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return;

	if (sem_wait(adapter->memlock) != 0)
		return;

	E1000_WRITE_REG(&(adapter->hw), E1000_WUS, data);
	if (sem_post(adapter->memlock) != 0)
		return;
}

void igb_writereg(device_t *dev, u_int32_t reg, u_int32_t data)
{
	struct adapter *adapter;

	if (dev == NULL)
		return;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return;

	E1000_WRITE_REG(&(adapter->hw), reg, data);
}

void igb_readreg(device_t *dev, u_int32_t reg, u_int32_t *data)
{
	struct adapter *adapter;

	if (dev == NULL)
		return;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return;

	if (data == NULL)
		return;

	*data = E1000_READ_REG(&(adapter->hw), reg);
}

int igb_lock(device_t *dev)
{
	struct adapter *adapter;

	if (dev == NULL)
		return -ENODEV;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	return sem_wait(adapter->memlock);
}

int igb_unlock(device_t *dev)
{
	struct adapter *adapter;

	if (dev == NULL)
		return -ENODEV;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	return sem_post(adapter->memlock);
}

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then return the linked list of associated resources.
 *
 **********************************************************************/
void igb_clean(device_t *dev, struct igb_packet **cleaned_packets)
{
	struct e1000_tx_desc *tx_desc, *eop_desc;
	struct igb_packet *last_reclaimed;
	struct igb_tx_buffer *tx_buffer;
	struct adapter *adapter;
	struct tx_ring *txr;
	int first, last, done, processed, i;

	if (dev == NULL)
		return;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return;

	if (cleaned_packets == NULL)
		return;

	*cleaned_packets = NULL; /* nothing reclaimed yet */

	if (sem_wait(adapter->memlock) != 0)
		return;

	for (i = 0; i < adapter->num_queues; i++) {
		txr = &adapter->tx_rings[i];

		if (txr->tx_avail == adapter->num_tx_desc) {
			txr->queue_status = IGB_QUEUE_IDLE;
			continue;
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
			if (tx_buffer->packet)
				last_reclaimed = tx_buffer->packet;

			/* We clean the range of the packet */
			while (first != done) {
				if (tx_buffer->packet) {
					tx_buffer->packet->dmatime =
						(0xffffffff) &
						 tx_desc->buffer_addr;
					/* tx_buffer->packet->dmatime +=
					 *	(tx_desc->buffer_addr >> 32) *
					 *	 1000000000;
					 */
					txr->bytes += tx_buffer->packet->len;
					if (*cleaned_packets == NULL) {
						*cleaned_packets =
							tx_buffer->packet;
					} else {
						last_reclaimed->next =
							tx_buffer->packet;
					}
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
				/* Get new done point */
				if (++last == adapter->num_tx_desc)
					last = 0;
				done = last;
			} else
				break;
		}

		txr->next_to_clean = first;

		if (txr->tx_avail >= IGB_QUEUE_THRESHOLD)
			txr->queue_status &= ~IGB_QUEUE_DEPLETED;
	}
	sem_post(adapter->memlock);
}

/*********************************************************************
 *
 *  Allocate memory for the receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int igb_allocate_rx_queues(struct adapter *adapter)
{
	struct igb_buf_cmd ubuf;
	int dev = adapter->ldev;
	int i, error = 0;

	/* allocate the RX ring struct memory */
	adapter->rx_rings = (struct rx_ring *) malloc(sizeof(struct rx_ring) *
						      adapter->num_queues);

	if (adapter->rx_rings == NULL) {
		error = -ENOMEM;
		goto rx_fail;
	}

	memset(adapter->rx_rings, 0, sizeof(struct rx_ring) *
					    adapter->num_queues);

	for (i = 0; i < adapter->num_queues; i++) {

		if (sem_init(&adapter->rx_rings[i].lock, 0, 1) != 0) {
			error = errno;
			goto rx_desc;
		}

		if (sem_wait(&adapter->rx_rings[i].lock) != 0) {
			error = errno;
			goto rx_desc;
		}

		ubuf.queue = i;
		error = ioctl(dev, IGB_MAP_RX_RING, &ubuf);
		if (error < 0) {
			error = EBUSY;
			goto rx_desc;
		}
		adapter->rx_rings[i].rxdma.paddr = ubuf.physaddr;
		adapter->rx_rings[i].rxdma.mmap_size = ubuf.mmap_size;
		adapter->rx_rings[i].rx_base = NULL;
		adapter->rx_rings[i].rx_base =
			mmap(NULL, ubuf.mmap_size, PROT_READ | PROT_WRITE,
			     MAP_SHARED, adapter->ldev, ubuf.physaddr);

		if (adapter->rx_rings[i].rx_base == MAP_FAILED) {
			error = -ENOMEM;
			goto rx_desc;
		}

		adapter->rx_rings[i].adapter = adapter;
		adapter->rx_rings[i].me = i;

		adapter->num_rx_desc = ubuf.mmap_size /
				       sizeof(union e1000_adv_rx_desc);

		memset((void *)adapter->rx_rings[i].rx_base, 0, ubuf.mmap_size);
		adapter->rx_rings[i].rx_buffers =
			(struct igb_rx_buffer *)
				malloc(sizeof(struct igb_rx_buffer) *
				       adapter->num_rx_desc);

		if (adapter->rx_rings[i].rx_buffers == NULL) {
			error = -ENOMEM;
			goto rx_desc;
		}

		memset(adapter->rx_rings[i].rx_buffers, 0,
		       sizeof(struct igb_rx_buffer) * adapter->num_rx_desc);

		if (sem_post(&adapter->rx_rings[i].lock) != 0) {
			error = errno;
			goto rx_desc;
		}
	}

	return 0;

rx_desc:
	for (i = 0; i < adapter->num_queues; i++) {
		if (adapter->rx_rings[i].rx_base)
			munmap(adapter->rx_rings[i].rx_base,
			       adapter->rx_rings[i].rxdma.mmap_size);
		ubuf.queue = i;
		ioctl(dev, IGB_UNMAP_RX_RING, &ubuf);

		sem_destroy(&adapter->rx_rings[i].lock);
	};
rx_fail:
	free(adapter->rx_rings);
	adapter->rx_rings = NULL;
	return error;
}

static void igb_free_receive_ring(struct rx_ring *rxr)
{
	struct adapter *adapter = rxr->adapter;
	struct igb_rx_buffer *rxbuf;
	int i;

	for (i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		rxbuf->next_eop = 0;
		rxbuf->packet = NULL;
	}
}

/* Initialize a receive ring. */
static void igb_setup_receive_ring(struct rx_ring *rxr)
{
	struct adapter *adapter = rxr->adapter;

	(void)sem_wait(&rxr->lock);

	/* Clear the ring contents */
	memset((void *)rxr->rx_base,  0,
	       (sizeof(union e1000_adv_rx_desc)) * adapter->num_rx_desc);

	memset(rxr->rx_buffers, 0, sizeof(struct igb_rx_buffer) *
				   rxr->adapter->num_rx_desc);

	/* Free current RX buffer structures */
	igb_free_receive_ring(rxr);

	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;
	rxr->rx_split_packets = 0;
	rxr->rx_bytes = 0;

	(void)sem_post(&rxr->lock);
}

/* Initialize all receive rings. */
static void igb_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int i;

	for (i = 0; i < adapter->num_queues; i++, rxr++)
		igb_setup_receive_ring(rxr);
}

/* Enable receive unit. */
static void igb_initialize_receive_units(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32 rctl, rxcsum, srrctl = 0;
	int i;

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	rctl &= ~E1000_RCTL_LPE;
	srrctl |= 2048 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;
	rctl |= E1000_RCTL_SZ_2048;

	/* Setup the Base and Length of the Rx Descriptor Rings */
	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 bus_addr = rxr->rxdma.paddr;
		u32 rxdctl;

		/* Disable this Queue */
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), 0);

		E1000_WRITE_REG(hw, E1000_RDLEN(i),
				adapter->num_rx_desc *
					sizeof(union e1000_adv_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
				(uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i),
				(uint32_t)bus_addr);
		E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);

		/* Enable this Queue */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
	}

	/*
	 * Setup for RX MultiQueue
	 * Non RSS setup
	 */
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
	rxcsum |= E1000_RXCSUM_IPPCSE;
	/*	rxcsum &= ~E1000_RXCSUM_TUOFL; */
	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/* Setup the Receive Control Register */
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		E1000_RCTL_RDMTS_HALF;
	/* Strip CRC bytes. */
	rctl |= E1000_RCTL_SECRC;
	/* Make sure VLAN Filters are off */
	rctl &= ~E1000_RCTL_VFE;
	/* Don't store bad packets */
	rctl &= ~E1000_RCTL_SBP;

	/* Enable Receives */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers
	 *   - needs to be after enable
	 */
	for (i = 0; i < adapter->num_queues; i++) {
		rxr = &adapter->rx_rings[i];
		E1000_WRITE_REG(hw, E1000_RDH(i), rxr->next_to_check);
		E1000_WRITE_REG(hw, E1000_RDT(i), rxr->next_to_refresh);
	}
}

/* Free all receive rings. */
static void igb_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int i;
	struct igb_buf_cmd ubuf;

	for (i = 0; i < adapter->num_queues; i++, rxr++) {
		(void)sem_wait(&adapter->rx_rings[i].lock);

		if (rxr->rx_base) {
			memset(rxr->rx_base, 0, rxr->rxdma.mmap_size);
			munmap(rxr->rx_base, rxr->rxdma.mmap_size);
		}
		ubuf.queue = i;
		ioctl(adapter->ldev, IGB_UNMAP_RX_RING, &ubuf);
		igb_free_receive_buffers(rxr);

		(void)sem_destroy(&adapter->rx_rings[i].lock);
	}

	free(adapter->rx_rings);
	adapter->rx_rings = NULL;
}


/* Free receive ring data structures. */
static void igb_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter *adapter = rxr->adapter;
	struct igb_rx_buffer *rxbuf;
	int i;

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
		for (i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			rxbuf->next_eop = 0;
			rxbuf->packet = NULL;
		}
		if (rxr->rx_buffers != NULL) {
			free(rxr->rx_buffers);
			rxr->rx_buffers = NULL;
		}
	}
}

/*
 *  Refresh mbuf buffers for RX descriptor rings
 *   - now keeps its own state so discards due to resource
 *     exhaustion are unnecessary, if an mbuf cannot be obtained
 *     it just returns, keeping its placeholder, thus it can simply
 *     be recalled to try again.
 *
 */
int igb_refresh_buffers(device_t *dev, u_int32_t idx,
			 struct igb_packet **rxbuf_packets, u_int32_t num_bufs)
{
	struct igb_packet *cur_pkt;
	struct adapter *adapter;
	struct rx_ring *rxr;
	u_int32_t i, j, bufs_used;
	bool refreshed = FALSE;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -EINVAL;

	if (rxbuf_packets == NULL)
		return -EINVAL;

	if (idx > 1)
		return -EINVAL;

	rxr = &adapter->rx_rings[idx];
	if (rxr == NULL)
		return -EINVAL;

	if (sem_trywait(&rxr->lock) != 0)
		return errno; /* EAGAIN */

	i = j = rxr->next_to_refresh;
	cur_pkt = *rxbuf_packets;

	/*
	 * Get one descriptor beyond
	 * our work mark to control
	 * the loop.
	 */
	if (++j == adapter->num_rx_desc)
		j = 0;

	bufs_used = 0;

	while (bufs_used < num_bufs) {
		if (!cur_pkt)
			break;
		rxr->rx_base[i].read.pkt_addr =
			htole64(cur_pkt->map.paddr + cur_pkt->offset);
		rxr->rx_buffers[i].packet = cur_pkt;

		refreshed = TRUE; /* I feel wefreshed :) */

		i = j; /* our next is precalculated */
		rxr->next_to_refresh = i;
		if (++j == adapter->num_rx_desc)
			j = 0;
		bufs_used++;
		cur_pkt = cur_pkt->next;
	}

	if (refreshed) /* update tail */
		E1000_WRITE_REG(&adapter->hw,
				E1000_RDT(rxr->me), rxr->next_to_refresh);

	if (sem_post(&rxr->lock) != 0)
		return errno;

	return 0;
}


/**********************************************************************
 *
 *  Examine each rx_buffer in the used queue. If the hardware is done
 *  processing the packet then return the linked list of associated resources.
 *
 **********************************************************************/
int igb_receive(device_t *dev, unsigned int queue_index, 
					struct igb_packet **received_packets, u_int32_t *count)
{
	struct adapter *adapter;
	struct rx_ring *rxr;
	union e1000_adv_rx_desc *cur;
	bool eop = FALSE;
	u_int32_t staterr = 0;
	u_int32_t desc    = 0;
	u_int32_t max_pkt = 0;
	struct igb_packet *curr_pkt = NULL;
	struct igb_packet *prev_pkt = NULL;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (queue_index > adapter->num_queues)
		return -EINVAL;

	rxr = &(adapter->rx_rings[queue_index]);
	if (!rxr)
		return -EINVAL;

	if (count == NULL)
		return -EINVAL;

	max_pkt = *count;
	*count  = 0;

	if (received_packets == NULL)
		return -EINVAL;

	*received_packets = NULL; /* nothing reclaimed yet */

	if (sem_trywait(&rxr->lock) != 0)
		return errno; /* EAGAIN */

	/* Main clean loop - receive packets until no more
	 * received_packets[]
	 */
	for (desc = rxr->next_to_check; *count < max_pkt;) {
		cur = &(rxr->rx_base[desc]);
#ifdef DEBUG
		if (i%2)
			printf("\033[2A");

		printf("desc.status_error=%x desc.length=%x desc.vlan=%x desc.rss=%x desc.pkt_info=%x desc.hdr_info=%x\n", 
				cur->wb.upper.status_error, 
				cur->wb.upper.length, 
				cur->wb.upper.vlan, 
				cur->wb.lower.hi_dword.rss,
				cur->wb.lower.lo_dword.hs_rss.pkt_info,
				cur->wb.lower.lo_dword.hs_rss.hdr_info);
#endif
		staterr = le32toh(cur->wb.upper.status_error);
		if ((staterr & E1000_RXD_STAT_DD) == 0)
			break;

		cur->wb.upper.status_error = 0;

		eop = ((staterr & E1000_RXD_STAT_EOP) ==
				E1000_RXD_STAT_EOP);

		if (eop) {
			/*
			 * Free the frame (all segments) if we're at EOP and
			 * it's an error.
			 *
			 * The datasheet states that EOP + status is only valid
			 * for the final segment in a multi-segment frame.
			 */
			if (staterr & E1000_RXDEXT_ERR_FRAME_ERR_MASK) {
				++rxr->rx_discarded;
				printf ("discard error packet\n");
				igb_refresh_buffers(dev, queue_index,
						&rxr->rx_buffers[desc].packet, 1);
			} else {
				/*
				 * add new packet to list of received packets
				 * to return
				 */
				curr_pkt = rxr->rx_buffers[desc].packet;
				curr_pkt->len = cur->wb.upper.length;

				if (*received_packets == NULL)
					*received_packets = curr_pkt;
				if (prev_pkt)
					prev_pkt->next = curr_pkt;
				prev_pkt = curr_pkt;
				(*count)++;

				++rxr->rx_packets;
			}
		} else {
			/* multi-segment frame is not supported yet */
			++rxr->rx_discarded;
			printf ("discard non-eop packet\n");
			igb_refresh_buffers(dev, queue_index,
					&rxr->rx_buffers[desc].packet, 1);
		}
next_desc:
		/* Advance our pointers to the next descriptor. */
		if (++desc == adapter->num_rx_desc)
			desc = 0;
	}

	rxr->next_to_check = desc;

	if (sem_post(&rxr->lock) != 0)
		return errno;

	if (*received_packets == NULL) {
		/* nothing reclaimed yet */
		errno = EAGAIN;
		return errno;
	}

	return 0;
}

#define MAX_ITER 32
#define MIN_WALLCLOCK_TSC_WINDOW 80 /* cycles */
#define MIN_SYSCLOCK_WINDOW 72 /* ns */

static inline void rdtscpll(uint64_t *val)
{
	uint32_t high, low;

	__asm__ __volatile__("lfence;"
						  "rdtsc;"
						  : "=d"(high), "=a"(low)
						  :
						  : "memory");
	*val = high;
	*val = (*val << 32) | low;
}

static inline void __sync(void)
{
	__asm__ __volatile__("mfence;"
						  :
						  :
						  : "memory");
}

int igb_get_wallclock(device_t *dev, u_int64_t *curtime, u_int64_t *rdtsc)
{
	u_int64_t t0 = 0, t1 = -1;
	u_int32_t duration = -1;
	u_int32_t timh, timl, tsauxc;
	struct adapter *adapter;
	struct e1000_hw *hw;
	int error = 0;
	int iter = 0;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	hw = &adapter->hw;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	/* sample the timestamp bracketed by the RDTSC */
	for (iter = 0; iter < MAX_ITER && t1 - t0 > MIN_WALLCLOCK_TSC_WINDOW;
	     ++iter) {
		tsauxc = E1000_READ_REG(hw, E1000_TSAUXC);
		tsauxc |= E1000_TSAUXC_SAMP_AUTO;

		/* Invalidate AUXSTMPH/L0 */
		E1000_READ_REG(hw, E1000_AUXSTMPH0);
		rdtscpll(&t0);
		E1000_WRITE_REG(hw, E1000_TSAUXC, tsauxc);
		rdtscpll(&t1);

		if (t1 - t0 < duration) {
			duration = t1 - t0;
			timl = E1000_READ_REG(hw, E1000_AUXSTMPL0);
			timh = E1000_READ_REG(hw, E1000_AUXSTMPH0);

			if (curtime)
				*curtime = (u_int64_t)timh * 1000000000 +
					   (u_int64_t)timl;
			if (rdtsc)
				/* average */
				*rdtsc = (t1 - t0) / 2 + t0;
		}
	}

	if (sem_post(adapter->memlock) != 0) {
		error = errno;
		goto err;
	}

	/* Return the window size * -1 */
	return -duration;

err:
	return error;
}

struct timespec timespec_subtract(struct timespec *a, struct timespec *b)
{
	a->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (a->tv_nsec < 0) {
		/* borrow */
		a->tv_nsec += 1000000000;
		--a->tv_sec;
	}

	a->tv_sec = a->tv_sec - b->tv_sec;
	return *a;
}

struct timespec timespec_addns(struct timespec *a, unsigned long addns)
{
	a->tv_nsec = a->tv_nsec + (addns % 1000000000);
	if (a->tv_nsec > 1000000000) {
		/* carry */
		a->tv_nsec -= 1000000000;
		++a->tv_sec;
	}

	a->tv_sec = a->tv_sec + addns/1000000000;
	return *a;
}

static inline u_int32_t TS2NS(struct timespec ts)
{
    return ((ts.tv_sec*1000000000)+ts.tv_nsec);
}

int igb_gettime(device_t *dev, clockid_t clk_id, u_int64_t *curtime,
		struct timespec *system_time)
{
	struct timespec	t0 = { 0, 0 }, t1 = { .tv_sec = 4, .tv_nsec = 0 };
	u_int32_t timh, timl, tsauxc;
	u_int32_t duration = -1;
	struct adapter *adapter;
	struct e1000_hw *hw;
	int error = 0;
	int iter = 0;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	hw = &adapter->hw;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	/* sample the timestamp bracketed by the clock_gettime() */
	for (iter = 0; iter < MAX_ITER && duration > MIN_SYSCLOCK_WINDOW;
	     ++iter) {
		u_int32_t duration_c;

		tsauxc = E1000_READ_REG(hw, E1000_TSAUXC);
		tsauxc |= E1000_TSAUXC_SAMP_AUTO;

		/* Invalidate AUXSTMPH/L0 */
		E1000_READ_REG(hw, E1000_AUXSTMPH0);
		clock_gettime(clk_id, &t0);
		E1000_WRITE_REG(hw, E1000_TSAUXC, tsauxc);
		__sync();
		clock_gettime(clk_id, &t1);

		timespec_subtract(&t1, &t0);
		duration_c = TS2NS(t1);
		if (duration_c < duration) {
			duration = duration_c;
			timl = E1000_READ_REG(hw, E1000_AUXSTMPL0);
			timh = E1000_READ_REG(hw, E1000_AUXSTMPH0);

			if (curtime)
				*curtime = (u_int64_t)timh * 1000000000 +
					   (u_int64_t)timl;
			if (system_time)
				*system_time =
					timespec_addns(&t0, duration/2);
		}
	}

	if (sem_post(adapter->memlock) != 0) {
		error = errno;
		goto err;
	}
	/* Return the window size * -1 */
	return -duration;

err:
	return error;
}

int igb_set_class_bandwidth(device_t *dev, u_int32_t class_a, u_int32_t class_b,
			    u_int32_t tpktsz_a, u_int32_t tpktsz_b)
{
	u_int32_t tqavctrl;
	u_int32_t tqavcc0, tqavcc1;
	u_int32_t tqavhc0, tqavhc1;
	u_int32_t class_a_idle, class_b_idle;
	u_int32_t linkrate;
	struct adapter *adapter;
	struct e1000_hw *hw;
	struct igb_link_cmd link;
	int err;
	float class_a_percent, class_b_percent;
	int error = 0;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	hw = &adapter->hw;

	/* get current link speed */

	err = ioctl(adapter->ldev, IGB_LINKSPEED, &link);

	if (err)
		return -ENXIO;

	if (link.up == 0)
		return -EINVAL;

	if (link.speed < 100)
		return -EINVAL;

	if (link.duplex != FULL_DUPLEX)
		return -EINVAL;

	if (tpktsz_a < 64)
		tpktsz_a = 64; /* minimum ethernet frame size */

	if (tpktsz_a > 1500)
		return -EINVAL;

	if (tpktsz_b < 64)
		tpktsz_b = 64; /* minimum ethernet frame size */

	if (tpktsz_b > 1500)
		return -EINVAL;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	tqavctrl = E1000_READ_REG(hw, E1000_TQAVCTRL);

	if ((class_a + class_b) == 0) {
		/* disable the Qav shaper */
		tqavctrl &= ~E1000_TQAVCTRL_TX_ARB;
		E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);
		goto unlock;
	}

	tqavcc0 = E1000_TQAVCC_QUEUEMODE;
	tqavcc1 = E1000_TQAVCC_QUEUEMODE;

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

	/* 12=Ethernet IPG,
	 * 8=Preamble+Start of Frame,
	 * 18=Mac Header with VLAN+Etype,
	 * 4=CRC
	 */
	class_a_percent = (float)((tpktsz_a + (12 + 8 + 18 + 4)) * class_a);
	class_b_percent = (float)((tpktsz_b + (12 + 8 + 18 + 4)) * class_b);

	class_a_percent /= 0.000125; /* class A observation window */
	class_b_percent /= 0.000250; /* class B observation window */

	if (link.speed == 100) {
		/* bytes-per-sec @ 100Mbps */
		class_a_percent /= (100000000.0 / 8);
		class_b_percent /= (100000000.0 / 8);
		class_a_idle = (u_int32_t)(class_a_percent * 0.2 *
				(float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 0.2 *
				(float)linkrate + 0.5);
	} else {
		/* bytes-per-sec @ 1Gbps */
		class_a_percent /= (1000000000.0 / 8);
		class_b_percent /= (1000000000.0 / 8);
		class_a_idle = (u_int32_t)(class_a_percent *
				2.0 * (float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 2.0 *
				(float)linkrate + 0.5);
	}

	if ((class_a_percent + class_b_percent) > 0.75) {
		error = -EINVAL;
		goto unlock;
	}
	tqavcc0 |= class_a_idle;
	tqavcc1 |= class_b_idle;

	/*
	 * hiCredit is the number of idleslope credits accumulated due to delay
	 *
	 * we assume the maxInterferenceSize is 18 + 4 + 1500 (1522).
	 * Note: if EEE is enabled, we should use for maxInterferenceSize
	 * the overhead of link recovery (a media-specific quantity).
	 */
	tqavhc0 = 0x80000000 + (class_a_idle * 1522 / linkrate); /* L.10 */

	/*
	 * Class B high credit is is the same, except the delay
	 * is the MaxBurstSize of Class A + maxInterferenceSize of non-SR
	 * traffic
	 *
	 * L.41
	 * max Class B delay = (1522 + tpktsz_a) / (linkrate - class_a_idle)
	 */

	tqavhc1 = 0x80000000 + (class_b_idle * ((1522 + tpktsz_a) /
						(linkrate - class_a_idle)));

	/* implicitly enable the Qav shaper */
	tqavctrl |= E1000_TQAVCTRL_TX_ARB;
	E1000_WRITE_REG(hw, E1000_TQAVHC(0), tqavhc0);
	E1000_WRITE_REG(hw, E1000_TQAVCC(0), tqavcc0);
	E1000_WRITE_REG(hw, E1000_TQAVHC(1), tqavhc1);
	E1000_WRITE_REG(hw, E1000_TQAVCC(1), tqavcc1);
	E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);

unlock:
	if (sem_post(adapter->memlock) != 0)
		error = errno;

	return error;
}

int igb_set_class_bandwidth2(device_t *dev, u_int32_t class_a_bytes_per_second,
			     u_int32_t class_b_bytes_per_second)
{
	u_int32_t tqavctrl;
	u_int32_t tqavcc0, tqavcc1;
	u_int32_t tqavhc0, tqavhc1;
	u_int32_t class_a_idle, class_b_idle;
	u_int32_t linkrate;
	u_int32_t tpktsz_a;
	int temp;
	struct adapter *adapter;
	struct e1000_hw *hw;
	struct igb_link_cmd link;
	int err;
	float class_a_percent, class_b_percent;
	int error = 0;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	hw = &adapter->hw;

	/* get current link speed */

	err = ioctl(adapter->ldev, IGB_LINKSPEED, &link);

	if (err)
		return -ENXIO;

	if (link.up == 0)
		return -EINVAL;

	if (link.speed < 100)
		return -EINVAL;

	if (link.duplex != FULL_DUPLEX)
		return -EINVAL;

	if (sem_wait(adapter->memlock) != 0)
		return errno;

	tqavctrl = E1000_READ_REG(hw, E1000_TQAVCTRL);

	if ((class_a_bytes_per_second + class_b_bytes_per_second) == 0) {
		/* disable the Qav shaper */
		tqavctrl &= ~E1000_TQAVCTRL_TX_ARB;
		E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);
		goto unlock;
	}

	tqavcc0 = E1000_TQAVCC_QUEUEMODE;
	tqavcc1 = E1000_TQAVCC_QUEUEMODE;

	linkrate = E1000_TQAVCC_LINKRATE;

	/* it is needed for Class B high credit calculations
	 * so we need to guess it
	 * TODO: check if it is right
	 */
	temp = class_a_bytes_per_second / 8000 - (12 + 8 + 18 + 4);
	if (temp > 0)
		tpktsz_a = temp;
	else
		tpktsz_a = 0;
	/* TODO: in igb_set_class_bandwidth if given tpktsz_a < 64
	 * (for example 0) then the 64 value will be used even if
	 * there is no class_A streams (class_a is 0)
	 * I suspect that this is error, so we use 0 here.
	 */

	class_a_percent = class_a_bytes_per_second;
	class_b_percent = class_b_bytes_per_second;

	if (link.speed == 100) {
		/* bytes-per-sec @ 100Mbps */
		class_a_percent /= (100000000.0 / 8);
		class_b_percent /= (100000000.0 / 8);
		class_a_idle = (u_int32_t)(class_a_percent * 0.2 *
				(float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 0.2 *
				(float)linkrate + 0.5);
	} else {
		/* bytes-per-sec @ 1Gbps */
		class_a_percent /= (1000000000.0 / 8);
		class_b_percent /= (1000000000.0 / 8);
		class_a_idle = (u_int32_t)(class_a_percent * 2.0 *
				(float)linkrate + 0.5);
		class_b_idle = (u_int32_t)(class_b_percent * 2.0 *
				(float)linkrate + 0.5);
	}

	if ((class_a_percent + class_b_percent) > 0.75) {
		error = -EINVAL;
		goto unlock;
	}
	tqavcc0 |= class_a_idle;
	tqavcc1 |= class_b_idle;

	/*
	 * hiCredit is the number of idleslope credits accumulated due to delay
	 *
	 * we assume the maxInterferenceSize is 18 + 4 + 1500 (1522).
	 * Note: if EEE is enabled, we should use for maxInterferenceSize
	 * the overhead of link recovery (a media-specific quantity).
	 */
	tqavhc0 = 0x80000000 + (class_a_idle * 1522 / linkrate); /* L.10 */

	/*
	 * Class B high credit is is the same, except the delay
	 * is the MaxBurstSize of Class A + maxInterferenceSize of non-SR
	 * traffic
	 *
	 * L.41
	 * max Class B delay = (1522 + tpktsz_a) / (linkrate - class_a_idle)
	 */

	tqavhc1 = 0x80000000 + (class_b_idle * ((1522 + tpktsz_a) /
				(linkrate - class_a_idle)));

	/* implicitly enable the Qav shaper */
	tqavctrl |= E1000_TQAVCTRL_TX_ARB;
	E1000_WRITE_REG(hw, E1000_TQAVHC(0), tqavhc0);
	E1000_WRITE_REG(hw, E1000_TQAVCC(0), tqavcc0);
	E1000_WRITE_REG(hw, E1000_TQAVHC(1), tqavhc1);
	E1000_WRITE_REG(hw, E1000_TQAVCC(1), tqavcc1);
	E1000_WRITE_REG(hw, E1000_TQAVCTRL, tqavctrl);

unlock:
	if (sem_post(adapter->memlock) != 0)
		error = errno;

	return error;
}

int igb_get_mac_addr(device_t *dev, u_int8_t mac_addr[ETH_ADDR_LEN])
{
	struct adapter *adapter;
	struct e1000_hw *hw;

	if (dev == NULL)
		return -EINVAL;
	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	hw = &adapter->hw;

	memcpy(mac_addr, hw->mac.addr, ETH_ADDR_LEN);
	return 0;
}

int igb_setup_flex_filter(device_t *dev, unsigned int queue_id,
			  unsigned int filter_id, unsigned int filter_len,
			  u_int8_t *filter, u_int8_t *mask)
{
	struct adapter *adapter;
	struct e1000_hw *hw;
	u32 i = 0, j, k;
	u32 fhft, wufc;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (filter_id > 7)
		return -EINVAL;

	if (queue_id > 1)
		return -EINVAL;

	if (filter_len > 128)
		return -EINVAL;

	hw = &adapter->hw;

	/*
	 * example pattern to set to match on the following in a Magic Packet
	 * 0x00: xx xx xx xx xx xx xx xx  xx xx xx xx 08 00 45 00
	 * 0x10: xx xx xx xx xx xx xx 11  xx xx xx xx xx xx xx xx
	 * 0x20: xx xx xx xx 00 07 00 86  xx xx ff ff ff ff ff ff
	 * 0x30: m0 m1 m2 m3 m4 m5 xx xx  xx xx xx xx xx xx xx xx
	 *
	 * Where m0-m5 are the 6 bytes of the mac address in network order
	 *
	 * example code follows
	 *
	 * ethertype should be IP which is 0x0800
	 * filter[0x0C] = 0x08;
	 * filter[0x0D] = 0x00;

	 * verify IPv4 and header length 20
	 * filter[0x0E] = 0x45;
	 * filter[0x0F] = 0x00;
	 * mask[1] = 0xF0;

	 * verify L3 protocol is UDP
	 * filter[0x17] = 0x11;
	 * mask[2] = 0x80;

	 * verify source and destination port numbers
	 * filter[0x24] = 0x00;
	 * filter[0x25] = 0x07;
	 * filter[0x26] = 0x00;
	 * filter[0x27] = 0x86;
	 * mask[4] = 0xF0;

* add start filter of 6 bytes all 0xFF
	 * memset(&filter[0x2a], 0xff, 6);
	 * mask[5] = 0xFC;

	 * add mac address
	 * memcpy(&filter[0x30], hw->mac.addr, 6);
	 * mask[6] |= 0x3F;
	 */

	while (i < filter_len) {
		for (j = 0; j < 8; j += 4) {
			fhft = 0;
			for (k = 0; k < 4; k++)
				fhft |= ((u32)(filter[i + j + k])) << (k * 8);
			E1000_WRITE_REG_ARRAY(hw, E1000_FHFT(filter_id),
					(i/2) + (j/4), fhft);
		}
		E1000_WRITE_REG_ARRAY(hw, E1000_FHFT(filter_id),
				(i/2) + 2, mask[i/8]);
		i += 8;
	}

	E1000_WRITE_REG_ARRAY(hw, E1000_FHFT(filter_id),
			63, (queue_id << 8) | filter_len);

	E1000_WRITE_REG(hw, E1000_WUC, 0x21); /* XXX is this right ? */

	wufc = E1000_READ_REG(hw, E1000_WUFC);
	wufc |= (E1000_WUFC_FLX0 << filter_id) | E1000_WUFC_FLEX_HQ;
	E1000_WRITE_REG(hw, E1000_WUFC, wufc);

	return 0;
}

int igb_clear_flex_filter(device_t *dev, unsigned int filter_id)
{
	struct adapter *adapter;
	struct e1000_hw *hw;
	u32 wufc;

	if (dev == NULL)
		return -EINVAL;

	adapter = (struct adapter *)dev->private_data;
	if (adapter == NULL)
		return -ENXIO;

	if (filter_id > 7)
		return -EINVAL;


	hw = &adapter->hw;

	wufc = E1000_READ_REG(hw, E1000_WUFC);
	wufc &= ~(E1000_WUFC_FLX0 << filter_id);
	E1000_WRITE_REG(hw, E1000_WUFC, wufc);

	return 0;
}
