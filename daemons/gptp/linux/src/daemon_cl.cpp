/******************************************************************************

  Copyright (c) 2012 Intel Corporation 
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

#include "ieee1588.hpp"
#include "avbts_clock.hpp"
#include "avbts_osnet.hpp"
#include "avbts_oslock.hpp"
#include "linux_hal.hpp"

int main(int argc, char **argv)
{
	sigset_t set;
	InterfaceName *ifname;
	int sig;

	LinuxPCAPNetworkInterfaceFactory *default_factory =
	    new LinuxPCAPNetworkInterfaceFactory;
	OSNetworkInterfaceFactory::registerFactory(factory_name_t("default"),
						   default_factory);
	LinuxThreadFactory *thread_factory = new LinuxThreadFactory();
	LinuxTimerQueueFactory *timerq_factory = new LinuxTimerQueueFactory();
	LinuxLockFactory *lock_factory = new LinuxLockFactory();
	LinuxTimerFactory *timer_factory = new LinuxTimerFactory();
	LinuxConditionFactory *condition_factory = new LinuxConditionFactory();
	LinuxSimpleIPC *ipc = new LinuxSimpleIPC();

	if (argc < 2)
		return -1;
	ifname = new InterfaceName(argv[1], strlen(argv[1]));

	IEEE1588Clock *clock = new IEEE1588Clock(false, timerq_factory, ipc);
	HWTimestamper *timestamper = new LinuxTimestamper();
	IEEE1588Port *port =
	    new IEEE1588Port(clock, 1, false, timestamper, false, 0, ifname,
			     condition_factory, thread_factory, timer_factory,
			     lock_factory);

	if (!port->init_port()) {
		printf("failed to initialize port \n");
		return -1;
	}
	port->processEvent(POWERUP);

	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
		perror("pthread_sigmask()");
		return -1;
	}

	if (sigwait(&set, &sig) != 0) {
		perror("sigwait()");
		return -1;
	}

	fprintf(stderr, "Exiting on %d\n", sig);

	return 0;
}
