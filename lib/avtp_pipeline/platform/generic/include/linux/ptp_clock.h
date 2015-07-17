#ifndef _OPENAVB_GENERIC_LINUX_PTP_CLOCK_H
#define _OPENAVB_GENERIC_LINUX_PTP_CLOCK_H

#include "/usr/include/linux/ptp_clock.h"

#ifndef ptpas_offset_request

#warning "Adding missing PTP declarations, it is just to make everything compile, AVB stack will not work properly."

struct ptpas_offset_request {
	unsigned long long syncReceiptTime;
	unsigned long long syncReceiptLocalTime;
	unsigned int gmRateRatioPPB; // gmRateRatio in ppb
	unsigned int gmSeqNumber; // an indication of the combination of clockID and clockIndex
};

struct ptpas_get_wallclock {
	long	tv_sec;			/* seconds */
	long	tv_nsec;		/* nanoseconds */
	unsigned int gmRateRatioPPB; // gmRateRatio in ppm
	unsigned int gmSeqNumber; // an indication of the combination of clockID and clockIndex
};

#define PTPAS_SET_OFFSET    _IOW(PTP_CLK_MAGIC, 6, struct ptpas_offset_request)
#define PTPAS_GET_WALLCLOCK _IOR(PTP_CLK_MAGIC, 7, struct ptpas_get_wallclock)

#endif

#endif // _OPENAVB_GENERIC_LINUX_PTP_CLOCK_H
