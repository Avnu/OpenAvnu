/*
 * Copyright (c) 2019, Aquantia Corporation
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

#include "avb_atl.h"

#include <string.h>
#include <stdio.h>
#include <errno.h>

/**
 * @brief Connect to the network card
 * @param atl_dev [inout] Device handle
 * @return 0 for success, ENXIO for failure
 */

int pci_connect(device_t *atl_dev)
{
	int err;
	err = atl_probe(atl_dev);
	if (err) {
		logprint(LOG_LVL_ERROR, "atl_probe failed! (%s)", strerror(err));
		return ENXIO;
	}
	logprint(LOG_LVL_VERBOSE, "attaching to %s", atl_dev->ifname);
	err = atl_attach(atl_dev);
	if (err) {
		logprint(LOG_LVL_ERROR, "attach failed! (%s)", strerror(err));
	}
	else {
		logprint(LOG_LVL_VERBOSE, "attaching to %s tx", atl_dev->ifname);
		err = atl_attach_tx(atl_dev);
	}
	if (err) {
		logprint(LOG_LVL_ERROR, "attach_tx failed! (%s)", strerror(err));
		atl_detach(atl_dev);
	}
	else {
		logprint(LOG_LVL_VERBOSE, "attaching to %s rx", atl_dev->ifname);
		err = atl_attach_rx(atl_dev);
	}
	if (err) {
		logprint(LOG_LVL_ERROR, "attach_rx failed! (%s)", strerror(err));
		atl_detach(atl_dev);
	}
	logprint(LOG_LVL_INFO, "Attaching to %s complete", atl_dev->ifname);
	return 0;
}
