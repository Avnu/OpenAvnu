#ifndef __AVB_IGB_H__
#define __AVB_IGB_H__

#include <igb.h>

#define IGB_BIND_NAMESZ 24

int pci_connect(device_t * igb_dev);

#endif
