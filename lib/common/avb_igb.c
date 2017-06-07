/*
 * Copyright (c) <2013>, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "avb_igb.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pci/pci.h>

/**
 * @brief Connect to the network card
 * @param igb_dev [inout] Device handle
 * @return 0 for success, ENXIO for failure
 */

int pci_connect(device_t *igb_dev)
{
	char devpath[IGB_BIND_NAMESZ];
	struct pci_access *pacc;
	struct pci_dev *dev;
	int err;

	memset(igb_dev, 0, sizeof(device_t));
	pacc = pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);

	for (dev = pacc->devices; dev; dev = dev->next) {
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
		igb_dev->pci_vendor_id = dev->vendor_id;
		igb_dev->pci_device_id = dev->device_id;
		igb_dev->domain = dev->domain;
		igb_dev->bus = dev->bus;
		igb_dev->dev = dev->dev;
		igb_dev->func = dev->func;
		snprintf(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d",
				dev->domain, dev->bus, dev->dev, dev->func);
		err = igb_probe(igb_dev);
		if (err) {
			continue;
		}
		printf("attaching to %s\n", devpath);
		err = igb_attach(devpath, igb_dev);
		if (err) {
			printf("attach failed! (%s)\n", strerror(err));
			continue;
		}
		err = igb_attach_tx(igb_dev);
		if (err) {
			printf("attach_tx failed! (%s)\n", strerror(err));
			igb_detach(igb_dev);
			continue;
		}
		goto out;
	}
	pci_cleanup(pacc);
	return ENXIO;
out:
	pci_cleanup(pacc);
	return 0;
}
