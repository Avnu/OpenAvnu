#ifndef __AVB_GPTP_H__
#define __AVB_GPTP_H__

#include <inttypes.h>

#define SHM_SIZE (4*8 + sizeof(pthread_mutex_t)) /* 3 - 64 bit and 2 - 32 bits */
#define SHM_NAME  "/ptp"

typedef long double FrequencyRatio;

typedef struct {
	int64_t ml_phoffset;
	int64_t ls_phoffset;
	FrequencyRatio ml_freqoffset;
	FrequencyRatio ls_freqoffset;
	uint64_t local_time;

	/* Current grandmaster information */
	/* Referenced by the IEEE Std 1722.1-2013 AVDECC Discovery Protocol Data Unit (ADPDU) */
	uint8_t gptp_grandmaster_id[8];			/* Current grandmaster id (all 0's if no grandmaster selected) */
	uint8_t gptp_domain_number;				/* gPTP domain number */

	/* Grandmaster support for the network interface */
	/* Referenced by the IEEE Std 1722.1-2013 AVDECC AVB_INTERFACE descriptor */
	uint8_t  clock_identity[8];				/* The clock identity of the interface */
	uint8_t  priority1;						/* The priority1 field of the grandmaster functionality of the interface, or 0xFF if not supported */
	uint8_t  clock_class;					/* The clockClass field of the grandmaster functionality of the interface, or 0xFF if not supported */
	int16_t  offset_scaled_log_variance;	/* The offsetScaledLogVariance field of the grandmaster functionality of the interface, or 0x0000 if not supported */
	uint8_t  clock_accuracy;				/* The clockAccuracy field of the grandmaster functionality of the interface, or 0xFF if not supported */
	uint8_t  priority2;						/* The priority2 field of the grandmaster functionality of the interface, or 0xFF if not supported */
	uint8_t  domain_number;					/* The domainNumber field of the grandmaster functionality of the interface, or 0 if not supported */
	int8_t   log_sync_interval;				/* The currentLogSyncInterval field of the grandmaster functionality of the interface, or 0 if not supported */
	int8_t   log_announce_interval;			/* The currentLogAnnounceInterval field of the grandmaster functionality of the interface, or 0 if not supported */
	int8_t   log_pdelay_interval;			/* The currentLogPDelayReqInterval field of the grandmaster functionality of the interface, or 0 if not supported */
	uint16_t port_number;					/* The portNumber field of the interface, or 0x0000 if not supported */
} gPtpTimeData;

/*TODO fix this*/
#ifndef false
typedef enum { false = 0, true = 1 } bool;
#endif

int gptpinit(int *shm_fd, char **shm_map);
int gptpdeinit(int *shm_fd, char **shm_map);
int gptpgetdata(char *shm_mmap, gPtpTimeData *td);
int gptpscaling(char *shm_mmap, gPtpTimeData *td);
bool gptplocaltime(const gPtpTimeData * td, uint64_t* now_local);
bool gptpmaster2local(const gPtpTimeData *td, const uint64_t master, uint64_t *local);

#endif
