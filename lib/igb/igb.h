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

#ifndef _IGB_H_DEFINED_
#define _IGB_H_DEFINED_

struct resource {
        u_int64_t       paddr;
	u_int32_t	mmap_size;
};

/* datastructure used to transmit a timed packet */
#define IGB_PACKET_LAUNCHTIME	1	/* control when packet transmitted */
#define IGB_PACKET_LATCHTIME	2	/* grab a timestamp of transmission */

struct igb_packet {
        struct resource map;            /* bus_dma map for packet */
	unsigned int	offset;		/* offset into physical page */
	void		*vaddr;
	u_int32_t 	len;
	u_int32_t 	flags;
	u_int64_t	attime;		/* launchtime */
	u_int64_t	dmatime;	/* when dma tx desc wb*/
	struct igb_packet *next;	/* used in the clean routine */
};

typedef struct _device_t {
	void	*private_data;
	u_int16_t pci_vendor_id;
	u_int16_t pci_device_id;
	u_int16_t domain;
	u_int8_t	bus;
	u_int8_t	dev;
	u_int8_t	func;
} device_t;

/*
 * Bus dma allocation structure used by
 * e1000_dma_malloc_page and e1000_dma_free_page.
 */
struct igb_dma_alloc {
        u_int64_t               dma_paddr;
        void                    *dma_vaddr;
        unsigned int    	mmap_size;
};

int	igb_probe( device_t *dev );
int	igb_attach(char *dev_path, device_t *pdev);
int	igb_detach(device_t *dev);
int	igb_suspend(device_t *dev);
int	igb_resume(device_t *dev);
int	igb_init(device_t *dev);
int	igb_dma_malloc_page(device_t *dev, struct igb_dma_alloc *page);
void	igb_dma_free_page(device_t *dev, struct igb_dma_alloc *page);
int	igb_xmit(device_t *dev, unsigned int queue_index, struct igb_packet *packet);
void	igb_clean(device_t *dev, struct igb_packet **cleaned_packets);
int	igb_get_wallclock(device_t *dev, u_int64_t	*curtime, u_int64_t *rdtsc);
int	igb_set_class_bandwidth(device_t *dev, u_int32_t class_a, u_int32_t class_b, u_int32_t tpktsz_a, u_int32_t tpktsz_b);

void	igb_trigger(device_t *dev, u_int32_t data);
void	igb_readreg(device_t *dev, u_int32_t reg, u_int32_t *data);
void	igb_writereg(device_t *dev, u_int32_t reg, u_int32_t data);

#endif /* _IGB_H_DEFINED_ */


