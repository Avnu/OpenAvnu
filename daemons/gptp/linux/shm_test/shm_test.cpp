/******************************************************************************

  Copyright (c) 2015, Coveloz Consulting
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Coveloz Consulting nor the names of its
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

#include <iostream>
#include <fstream>

#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "linux_ipc.hpp"

static inline uint64_t getCpuFrequency(void)
{
    uint64_t freq = 0;
    std::string line;
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open())
    {
        while ( getline (cpuinfo,line) )
        {
            if(line.find("MHz") != line.npos)
                break;
        }
        cpuinfo.close();
    }
    else std::cout << "Unable to open file";

    size_t pos = line.find(":");
    if (pos != line.npos) {
        std::string mhz_str = line.substr(pos+2, line.npos - pos);
        double freq1 = strtod(mhz_str.c_str(), NULL);
        freq = freq1 * 1000000ULL;
    }
    return freq;
}

int main(int argc, char *argv[])
{
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);

    if( shm_fd < 0) {
        fprintf(stderr, "shm_open(). %s\n", strerror(errno));
        return -1;
    }
    char *addr = (char*)mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);

    if( addr == MAP_FAILED ) {
        fprintf(stderr, "Error on mmap. Aborting.\n");
        return -1;
    }
    fprintf(stdout, "--------------------------------------------\n");
    int buf_offset = 0;
    buf_offset += sizeof(pthread_mutex_t);
    gPtpTimeData *ptpData = (gPtpTimeData*)(addr+buf_offset);
    /*TODO: Scale to ns*/
    uint64_t freq = getCpuFrequency();
    printf("Frequency %lu Hz\n", freq);

    fprintf(stdout, "ml phoffset %ld\n", ptpData->ml_phoffset);
    fprintf(stdout, "ml freq offset %Lf\n", ptpData->ml_freqoffset);
    fprintf(stdout, "ls phoffset %ld\n", ptpData->ls_phoffset);
    fprintf(stdout, "ls freq offset %Lf\n", ptpData->ls_freqoffset);
    fprintf(stdout, "local time %lu\n", ptpData->local_time);
    fprintf(stdout, "sync count %u\n", ptpData->sync_count);
    fprintf(stdout, "pdelay count %u\n", ptpData->pdelay_count);
    fprintf(stdout, "asCapable %s\n", ptpData->asCapable ? "True" : "False");
    fprintf(stdout, "Port State %d\n", (int)ptpData->port_state);

    return 0;
}

