/******************************************************************************

  Copyright (c) 2014, AudioScience, Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the AudioScience, Inc nor the names of its
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

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

extern "C"
{
#include "mrpd.h"
#include "mrp.h"
#include "mvrp.h"
#include "msrp.h"
#include "mmrp.h"
}

extern "C"
{

    HTIMER periodic_timer;
    unsigned char STATION_ADDR[] = { 0x00, 0x88, 0x77, 0x66, 0x55, 0x44 };
    unsigned int send_count = 0;

    HTIMER mrpd_timer_create(void)
    {
#if defined WIN32
        return NULL;
#else
        return 1;
#endif
    }

    void mrpd_timer_close(HTIMER t)
    {
    }

    int mrpd_timer_start_interval(HTIMER timerfd,
                                  unsigned long value_ms, unsigned long interval_ms)
    {
        return 0;
    }

    int mrpd_timer_start(HTIMER timerfd, unsigned long value_ms)
    {
        return mrpd_timer_start_interval(timerfd, value_ms, 0);
    }

    int mrpd_timer_stop(HTIMER timerfd)
    {
        return 0;
    }

    int mrp_periodictimer_start()
    {
        return mrpd_timer_start_interval(periodic_timer, 1000, 1000);
    }

    int mrp_periodictimer_stop()
    {
        return mrpd_timer_stop(periodic_timer);
    }

    int mrpd_recvmsgbuf(SOCKET sock, char **buf)
    {
        return 0;
    }

    int mrpd_send_ctl_msg(struct sockaddr_in *client_addr, char *notify_data,
                          int notify_len)
    {
        return notify_len;
    }

    size_t mrpd_send(SOCKET sockfd, const void *buf, size_t len, int flags)
    {
        send_count++;
        return len;
    }

    unsigned int mrpd_send_packet_count(void)
    {
        return send_count;
    }

    void mrpd_reset(void)
    {
        send_count = 0;
    }

    int mrpd_close_socket(SOCKET sock)
    {
        return 0;
    }

    int mrpd_init_protocol_socket(uint16_t etype, SOCKET *sock,
                                  unsigned char *multicast_addr)
    {
        return 0;
    }

    int mrpd_init_timers(struct mrp_database *mrp_db)
    {
        mrp_db->join_timer = mrpd_timer_create();
        mrp_db->lv_timer = mrpd_timer_create();
        mrp_db->lva_timer = mrpd_timer_create();
        mrp_db->join_timer_running = 0;
        mrp_db->lv_timer_running = 0;
        mrp_db->lva_timer_running = 0;

        return 0;
    };
}

int main(int ac, char **av)
{
    return CommandLineTestRunner::RunAllTests(ac, av);
}
