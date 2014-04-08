/******************************************************************************

  Copyright (c) 2012, Intel Corporation 
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

#include <linux_hal_generic.hpp>
#include <linux_hal_generic_tsprivate.hpp>
#include <errno.h>

extern "C" {
#include <pci/pci.h>
#include <igb.h>
}

#define IGB_BIND_NAMESZ 24

#define TSSDP     0x003C // Time Sync SDP Configuration Register
#define FREQOUT0  0xB654 
#define TSAUXC    0xB640 
#define IGB_CTRL  0x0000
#define SYSTIMH   0xB604
#define TRGTTIML0 0xB644
#define TRGTTIMH0 0xB648


static int
pci_connect( device_t *igb_dev )
{
	struct  pci_access      *pacc;
	struct  pci_dev *dev;
	int             err;
	char    devpath[IGB_BIND_NAMESZ];

	memset( igb_dev, 0, sizeof(device_t));

	pacc    =       pci_alloc();
	pci_init(pacc);
	pci_scan_bus(pacc);
	for     (dev=pacc->devices;     dev;    dev=dev->next)
    {
		pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);
      
		igb_dev->pci_vendor_id = dev->vendor_id;
		igb_dev->pci_device_id = dev->device_id;
		igb_dev->domain = dev->domain;
		igb_dev->bus = dev->bus;
		igb_dev->dev = dev->dev;
		igb_dev->func = dev->func;
		
		snprintf
			(devpath, IGB_BIND_NAMESZ, "%04x:%02x:%02x.%d", dev->domain,
			 dev->bus, dev->dev, dev->func );

		err = igb_probe( igb_dev );
      
		if (err) {
			continue;
		}

		printf ("attaching to %s\n", devpath);
		err = igb_attach( devpath, igb_dev );
      
		if (err) {
			printf ("attach failed! (%s)\n", strerror(errno));
			continue;
		}
		goto out;
    }

	pci_cleanup(pacc);
	return  ENXIO;
  
out:
	pci_cleanup(pacc);
	return  0;
}


bool LinuxTimestamperGeneric::HWTimestamper_PPS_start( ) {
	unsigned tssdp;
	unsigned freqout;
	unsigned ctrl;
	unsigned tsauxc;
	unsigned trgttimh;

	if( igb_private == NULL ) {
		igb_private = new LinuxTimestamperIGBPrivate;
	}

	if( pci_connect( &igb_private->igb_dev ) != 0 ) {
		return false;
	}

	if( igb_init( &igb_private->igb_dev ) != 0 ) {
		return false;
	}

	igb_private->igb_initd = true;

	igb_lock( &igb_private->igb_dev );

	// Edges must be second aligned
	igb_readreg( &igb_private->igb_dev, SYSTIMH, &trgttimh );
	trgttimh += 2;  // First edge in 1-2 seconds
	igb_writereg(&igb_private->igb_dev, TRGTTIMH0, trgttimh );
	igb_writereg(&igb_private->igb_dev, TRGTTIML0, 0 );

	freqout = 500000000;
	igb_writereg(&igb_private->igb_dev, FREQOUT0, freqout );

	igb_readreg(&igb_private->igb_dev, IGB_CTRL, &ctrl );
	ctrl |= 0x400000; // set bit 22 SDP0 enabling output
	igb_writereg(&igb_private->igb_dev, IGB_CTRL, ctrl );

	igb_readreg( &igb_private->igb_dev, TSAUXC, &tsauxc );
	tsauxc |= 0x14;
	igb_writereg( &igb_private->igb_dev, TSAUXC, tsauxc );

	igb_readreg(&igb_private->igb_dev, TSSDP, &tssdp);
	tssdp &= ~0x40; // Set SDP0 output to freq clock 0
	tssdp |= 0x80; 
	igb_writereg(&igb_private->igb_dev, TSSDP, tssdp);
  
	igb_readreg(&igb_private->igb_dev, TSSDP, &tssdp);
	tssdp |= 0x100; // set bit 8 -> SDP0 Time Sync Output
	igb_writereg(&igb_private->igb_dev, TSSDP, tssdp);
  
	igb_unlock( &igb_private->igb_dev );

	return true;
}

bool LinuxTimestamperGeneric::HWTimestamper_PPS_stop() {
	unsigned tssdp;
	
	if( !igb_private->igb_initd ) return false;
	
	igb_readreg(&igb_private->igb_dev, TSSDP, &tssdp);
	tssdp &= ~0x100; // set bit 8 -> SDP0 Time Sync Output
	igb_writereg(&igb_private->igb_dev, TSSDP, tssdp);
	
	return true;
}
