#include "avb_gptp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

/**
 * @brief Open the memory mapping used for IPC
 * @param shm_fd [inout] File descriptor for mapping
 * @param shm_map [inout] Pointer to mapping
 * @return 0 for success, negative for failure
 */

int gptpinit(int *shm_fd, char **shm_map)
{
	if (NULL == shm_fd || NULL == shm_map) {
		return -1;
	}
	*shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
	if (*shm_fd == -1) {
		perror("shm_open()");
		return -1;
	}
	*shm_map = (char *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
				MAP_SHARED, *shm_fd, 0);
	if ((char*)-1 == *shm_map) {
		perror("mmap()");
		*shm_map = NULL;
		shm_unlink(SHM_NAME);
		return -1;
	}
	return 0;
}

/**
 * @brief Free the memory mapping used for IPC
 * @param shm_fd [in] File descriptor for mapping
 * @param shm_map [in] Pointer to mapping
 * @return 0 for success, negative for failure
 *	-1 = close failed
 *	-2 = munmap failed
 *	-3 = close and munmap failed
 */

int gptpdeinit(int *shm_fd, char **shm_map)
{
	int ret = 0;
	if (NULL == shm_fd || -1 == *shm_fd) {
		ret -= 1;
	} else {
		if(close(*shm_fd) == -1) {
			ret -= 1;
		}
		*shm_fd = -1;
	}
	if (NULL == shm_map || NULL == *shm_map) {
		ret -= 2;
	} else {
		if (munmap(*shm_map, SHM_SIZE) == -1) {
			ret -= 2;
		}
		*shm_map = NULL;
	}
	return ret;
}

/**
 * @brief Read the ptp data from IPC memory
 * @param shm_map [in] Pointer to mapping
 * @param td [inout] Struct to read the data into
 * @return 0 for success, negative for failure
 */

int gptpgetdata(char *shm_map, gPtpTimeData *td)
{
	if (NULL == shm_map || NULL == td) {
		return -1;
	}
	pthread_mutex_lock((pthread_mutex_t *) shm_map);
	memcpy(td, shm_map + sizeof(pthread_mutex_t), sizeof(*td));
	pthread_mutex_unlock((pthread_mutex_t *) shm_map);

	return 0;
}

/**
 * @brief Read the ptp data from IPC memory and print its contents
 * @param shm_map [in] Pointer to mapping
 * @param td [inout] Struct to read the data into
 * @return 0 for success, negative for failure
 */

int gptpscaling(char *shm_map, gPtpTimeData *td) //change this function name ??
{
	int i;
	if ((i = gptpgetdata(shm_map, td)) < 0) {
		return i;
	}
	fprintf(stderr, "ml_phoffset = %" PRId64 ", ls_phoffset = %" PRId64 "\n",
		td->ml_phoffset, td->ls_phoffset);
	fprintf(stderr, "ml_freqffset = %Lf, ls_freqoffset = %Lf\n",
		td->ml_freqoffset, td->ls_freqoffset);

	return 0;
}

bool gptplocaltime(const gPtpTimeData * td, uint64_t* now_local)
{
	struct timespec sys_time;
	uint64_t now_system;
	uint64_t system_time;
	int64_t delta_local;
	int64_t delta_system;

	if (!td || !now_local)
		return false;

	if (clock_gettime(CLOCK_REALTIME, &sys_time) != 0)
		return false;

	now_system = (uint64_t)sys_time.tv_sec * 1000000000ULL + (uint64_t)sys_time.tv_nsec;

	system_time = td->local_time + td->ls_phoffset;
	delta_system = now_system - system_time;
	delta_local = td->ls_freqoffset * delta_system;
	*now_local = td->local_time + delta_local;

	return true;
}

bool gptpmaster2local(const gPtpTimeData *td, const uint64_t master, uint64_t *local)
{
	int64_t delta_8021as;
	int64_t delta_local;

	if (!td || !local)
		return false;

	delta_8021as = master - td->local_time + td->ml_phoffset;
	delta_local = delta_8021as / td->ml_freqoffset;
	*local = td->local_time + delta_local;

	return true;
}
