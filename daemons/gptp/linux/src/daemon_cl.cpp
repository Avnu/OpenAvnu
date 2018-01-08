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
#include "avbts_persist.hpp"
#include "gptp_cfg.hpp"

#ifdef ARCH_INTELCE
#include "linux_hal_intelce.hpp"
#else
#include "linux_hal_generic.hpp"
#endif

#include "linux_hal_persist_file.hpp"

#ifdef APTP
#include "addressregisterlistener.hpp"
#endif

#include <ctype.h>
#include <inttypes.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#ifdef SYSTEMD_WATCHDOG
#include <watchdog.hpp>
#endif

#define PHY_DELAY_GB_TX_I20 184 //1G delay
#define PHY_DELAY_GB_RX_I20 382 //1G delay
#define PHY_DELAY_MB_TX_I20 1044//100M delay
#define PHY_DELAY_MB_RX_I20 2133//100M delay

void gPTPPersistWriteCB(char *bufPtr, uint32_t bufSize);

void print_usage( char *arg0 ) {
	fprintf( stderr,
			"%s <network interface> [-S] [-P] [-M <filename>] "
			"[-G <group>] [-R <priority 1>] "
			"[-D <gb_tx_delay,gb_rx_delay,mb_tx_delay,mb_rx_delay>] "
			"[-T] [-L] [-E] [-GM] [-INITSYNC <value>] [-OPERSYNC <value>] "
			"[-INITPDELAY <value>] [-OPERPDELAY <value>] "
			"[-F <path to gptp_cfg.ini file>] "
			"\n",
			arg0 );
	fprintf
		( stderr,
		  "\t-S start syntonization\n"
		  "\t-P pulse per second\n"
		  "\t-M <filename> save/restore state\n"
		  "\t-G <group> group id for shared memory\n"
		  "\t-R <priority 1> priority 1 value\n"
		  "\t-D Phy Delay <gb_tx_delay,gb_rx_delay,mb_tx_delay,mb_rx_delay>\n"
		  "\t-T force master (ignored when Automotive Profile set)\n"
		  "\t-L force slave (ignored when Automotive Profile set)\n"
		  "\t-E enable test mode (as defined in AVnu automotive profile)\n"
		  "\t-V enable AVnu Automotive Profile\n"
		  "\t-GM set grandmaster for Automotive Profile\n"
		  "\t-INITSYNC <value> initial sync interval (Log base 2. 0 = 1 second)\n"
		  "\t-OPERSYNC <value> operational sync interval (Log base 2. 0 = 1 second)\n"
		  "\t-INITPDELAY <value> initial pdelay interval (Log base 2. 0 = 1 second)\n"
		  "\t-OPERPDELAY <value> operational pdelay interval (Log base 2. 0 = 1 sec)\n"
		  "\t-F <path-to-ini-file>\n"
		);
}

int watchdog_setup(OSThreadFactory *thread_factory)
{
#ifdef SYSTEMD_WATCHDOG
	SystemdWatchdogHandler *watchdog = new SystemdWatchdogHandler();
	OSThread *watchdog_thread = thread_factory->createThread();
	int watchdog_result;
	long unsigned int watchdog_interval;
	watchdog_interval = watchdog->getSystemdWatchdogInterval(&watchdog_result);
	if (watchdog_result) {
		GPTP_LOG_INFO("Watchtog interval read from service file: %lu us", watchdog_interval);
		watchdog->update_interval = watchdog_interval / 2;
		GPTP_LOG_STATUS("Starting watchdog handler (Update every: %lu us)", watchdog->update_interval);
		watchdog_thread->start(watchdogUpdateThreadFunction, watchdog);
		return 0;
	} else if (watchdog_result < 0) {
		GPTP_LOG_ERROR("Watchdog settings read error.");
		return -1;
	} else {
		GPTP_LOG_STATUS("Watchdog disabled");
		return 0;
	}
#else
	return 0;
#endif
}

// Use RAII for removal of heap allocations
class AKeeper
{
	public:
		static IEEE1588Clock *pClock;
		static EtherPort *pPort;

		InterfaceName *ifname;
		LinuxSharedMemoryIPC *ipc;
		LinuxThreadFactory *thread_factory;
		LinuxTimerQueueFactory *timerq_factory;
		LinuxLockFactory *lock_factory;
		LinuxTimerFactory *timer_factory;
		LinuxConditionFactory *condition_factory;

		AKeeper(char* interfaceName)
		{
			ifname = new InterfaceName(interfaceName, strlen(interfaceName));
			thread_factory = new LinuxThreadFactory();
			timerq_factory = new LinuxTimerQueueFactory();
			lock_factory = new LinuxLockFactory();
			timer_factory = new LinuxTimerFactory();
			condition_factory = new LinuxConditionFactory();
			ipc = new LinuxSharedMemoryIPC();
		}

		~AKeeper()
		{
			delete ifname;
			delete pPort;
			delete pClock;
			delete ipc;
			delete thread_factory;
			delete timerq_factory;
			delete lock_factory;
			delete timer_factory;
			delete condition_factory;
		}
};

IEEE1588Clock *AKeeper::pClock = nullptr;
EtherPort *AKeeper::pPort = nullptr;


int main(int argc, char **argv)
{
	AKeeper k(argv[1]);

	PortInit_t portInit;

	sigset_t set;
	int sig;

	bool syntonize = false;
	int i;
	bool pps = false;
	uint8_t priority1 = 248;
	bool override_portstate = false;
	PortState port_state = PTP_SLAVE;

	char *restoredata = nullptr;
	char *restoredataptr = nullptr;
	off_t restoredatalength = 0;
	off_t restoredatacount = 0;
	bool restorefailed = false;
	LinuxIPCArg *ipc_arg = nullptr;
	bool use_config_file = false;
	char config_file_path[512];
	memset(config_file_path, 0, 512);

	GPTPPersist *pGPTPPersist = nullptr;

	// Block SIGUSR1
	{
		sigset_t block;
		sigemptyset( &block );
		sigaddset( &block, SIGUSR1 );
		if( pthread_sigmask( SIG_BLOCK, &block, NULL ) != 0 ) {
			GPTP_LOG_ERROR("Failed to block SIGUSR1");
			return -1;
		}
	}

	GPTP_LOG_REGISTER();
	GPTP_LOG_INFO("gPTP starting");
	phy_delay_map_t ether_phy_delay;
	bool input_delay=false;

	portInit.clock = NULL;
	portInit.index = 0;
	portInit.timestamper = NULL;
	portInit.net_label = NULL;
	portInit.automotive_profile = false;
	portInit.isGM = false;
	portInit.testMode = false;
	portInit.linkUp = false;
	portInit.initialLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.initialLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogPdelayReqInterval = LOG2_INTERVAL_INVALID;
	portInit.operLogSyncInterval = LOG2_INTERVAL_INVALID;
	portInit.condition_factory = NULL;
	portInit.thread_factory = NULL;
	portInit.timer_factory = NULL;
	portInit.lock_factory = NULL;
	portInit.smoothRateChange = true;
	portInit.syncReceiptThreshold =
		CommonPort::DEFAULT_SYNC_RECEIPT_THRESH;
	portInit.neighborPropDelayThreshold =
		CommonPort::NEIGHBOR_PROP_DELAY_THRESH;

	std::shared_ptr<LinuxNetworkInterfaceFactory> default_factory =
		std::make_shared<LinuxNetworkInterfaceFactory>();
	OSNetworkInterfaceFactory::registerFactory
		(factory_name_t("default"), default_factory);

	if (watchdog_setup(k.thread_factory) != 0) {
		GPTP_LOG_ERROR("Watchdog handler setup error");
		return -1;
	}

	/* Create Low level network interface object */
	if( argc < 2 ) {
		printf( "Interface name required\n" );
		print_usage( argv[0] );
		return -1;
	}

	/* Process optional arguments */
	for( i = 2; i < argc; ++i ) {

		if( argv[i][0] == '-' ) {
			if( strcmp(argv[i] + 1,  "S") == 0 ) {
				// Get syntonize directive from command line
				syntonize = true;
			}
			else if( strcmp(argv[i] + 1,  "T" ) == 0 ) {
				override_portstate = true;
				port_state = PTP_MASTER;
			}
			else if( strcmp(argv[i] + 1,  "L" ) == 0 ) {
				override_portstate = true;
				port_state = PTP_SLAVE;
			}
			else if( strcmp(argv[i] + 1,  "M" )  == 0 ) {
				// Open file
				if( i+1 < argc ) {
					pGPTPPersist = makeLinuxGPTPPersistFile();
					if (pGPTPPersist) {
						pGPTPPersist->initStorage(argv[i + 1]);
				  }
				}
				else {
					printf( "Restore file must be specified on "
							"command line\n" );
				}
			}
			else if( strcmp(argv[i] + 1,  "G") == 0 ) {
				if( i+1 < argc ) {
					ipc_arg = new LinuxIPCArg(argv[++i]);
				} else {
					printf( "Must specify group name on the command line\n" );
				}
			}
			else if( strcmp(argv[i] + 1,  "P") == 0 ) {
				pps = true;
			}
			else if( strcmp(argv[i] + 1,  "H") == 0 ) {
				print_usage( argv[0] );
				GPTP_LOG_UNREGISTER();
				return 0;
			}
			else if( strcmp(argv[i] + 1,  "R") == 0 ) {
				if( i+1 >= argc ) {
					printf( "Priority 1 value must be specified on "
							"command line, using default value\n" );
				} else {
					unsigned long tmp = strtoul( argv[i+1], NULL, 0 ); ++i;
					if( tmp == 0 ) {
						printf( "Invalid priority 1 value, using "
								"default value\n" );
					} else {
						priority1 = (uint8_t) tmp;
					}
				}
			}
			else if (strcmp(argv[i] + 1, "D") == 0) {
				int phy_delay[4];
				input_delay=true;
				int delay_count=0;
				char *cli_inp_delay = strtok(argv[i+1],",");
				while (cli_inp_delay != NULL)
				{
					if(delay_count>3)
					{
						printf("Too many values\n");
						print_usage( argv[0] );
						GPTP_LOG_UNREGISTER();
						return 0;
					}
					phy_delay[delay_count]=atoi(cli_inp_delay);
					delay_count++;
					cli_inp_delay = strtok(NULL,",");
				}
				if (delay_count != 4)
				{
					printf("All four delay values must be specified\n");
					print_usage( argv[0] );
					GPTP_LOG_UNREGISTER();
					return 0;
				}
				ether_phy_delay[LINKSPEED_1G].set_delay
					( phy_delay[0], phy_delay[1] );
				ether_phy_delay[LINKSPEED_100MB].set_delay
					( phy_delay[2], phy_delay[3] );
			}
			else if (strcmp(argv[i] + 1, "V") == 0) {
				portInit.automotive_profile = true;
			}
			else if (strcmp(argv[i] + 1, "GM") == 0) {
				portInit.isGM = true;
			}
			else if (strcmp(argv[i] + 1, "E") == 0) {
				portInit.testMode = true;
			}
			else if (strcmp(argv[i] + 1, "INITSYNC") == 0) {
				portInit.initialLogSyncInterval = atoi(argv[++i]);
			}
			else if (strcmp(argv[i] + 1, "OPERSYNC") == 0) {
				portInit.operLogSyncInterval = atoi(argv[++i]);
			}
			else if (strcmp(argv[i] + 1, "INITPDELAY") == 0) {
				portInit.initialLogPdelayReqInterval = atoi(argv[++i]);
			}
			else if (strcmp(argv[i] + 1, "OPERPDELAY") == 0) {
				portInit.operLogPdelayReqInterval = atoi(argv[++i]);
			}
			else if (strcmp(argv[i] + 1, "F") == 0)
			{
				if( i+1 < argc ) {
					use_config_file = true;
					strcpy(config_file_path, argv[i+1]);
				} else {
					fprintf(stderr, "config file must be specified.\n");
				}
			}
			else if (0 == strcmp(argv[i] + 1, "SMOOTH"))
			{
				portInit.smoothRateChange = true;
			}
			else if (0 == strcmp(argv[i] + 1, "NOSMOOTH"))
			{
				portInit.smoothRateChange = false;
			}
		}
	}

	if (!input_delay)
	{
		ether_phy_delay[LINKSPEED_1G].set_delay
			( PHY_DELAY_GB_TX_I20, PHY_DELAY_GB_RX_I20 );
		ether_phy_delay[LINKSPEED_100MB].set_delay
			( PHY_DELAY_MB_TX_I20, PHY_DELAY_MB_RX_I20 );
	}
	portInit.phy_delay = &ether_phy_delay;

	if( pGPTPPersist ) {
		uint32_t bufSize = 0;
		if (!pGPTPPersist->readStorage(&restoredata, &bufSize))
			GPTP_LOG_ERROR("Failed to stat restore file");
		restoredatalength = bufSize;
		restoredatacount = restoredatalength;
		restoredataptr = (char *)restoredata;
	}

#ifdef ARCH_INTELCE
	std::shared_ptr<EtherTimestamper>  timestamper = std::make_shared<LinuxTimestamperIntelCE>();
#else
	#ifdef RPI
		std::shared_ptr<LinuxTimestamperGeneric>  timestamper = std::make_shared<LinuxTimestamperGeneric>();
		std::shared_ptr<LinuxThreadFactory> pulseThreadFactory = 
	 	 std::make_shared<LinuxThreadFactory>();
		timestamper->PulseThreadFactory(pulseThreadFactory);
	#else
		std::shared_ptr<EtherTimestamper>  timestamper = std::make_shared<LinuxTimestamperGeneric>();
	#endif
#endif


	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset( &set, SIGTERM );
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGUSR2);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
		perror("pthread_sigmask()");
		GPTP_LOG_UNREGISTER();
		return -1;
	}

	// TODO: The setting of values into temporary variables should be changed to
	// just set directly into the portInit struct.
	portInit.clock = k.pClock;
	portInit.index = 1;
	portInit.timestamper = timestamper;
	portInit.net_label = k.ifname;
	portInit.condition_factory = k.condition_factory;
	portInit.thread_factory = k.thread_factory;
	portInit.timer_factory = k.timer_factory;
	portInit.lock_factory = k.lock_factory;

	k.pPort = new EtherPort(&portInit);

	GPTP_LOG_INFO("smoothRateChange: %s", (k.pPort->SmoothRateChange() ? "true" : "false"));
	
	if(use_config_file)
	{
		GptpIniParser iniParser(config_file_path);
		k.pPort->setIniParser(iniParser);

		if (iniParser.parserError() < 0) {
			GPTP_LOG_ERROR("Cant parse ini file. Aborting file reading.");
		}
		else
		{
			GPTP_LOG_INFO("E2E delay mechansm: %d", V2_E2E);
			GPTP_LOG_INFO("delay_mechansm: %d", iniParser.getDelayMechanism());
			GPTP_LOG_INFO("priority1 = %d", iniParser.getPriority1());
			GPTP_LOG_INFO("announceReceiptTimeout: %d", iniParser.getAnnounceReceiptTimeout());
			GPTP_LOG_INFO("initialLogSyncInterval: %d", iniParser.getInitialLogSyncInterval());
			GPTP_LOG_INFO("syncReceiptTimeout: %d", iniParser.getSyncReceiptTimeout());
			iniParser.print_phy_delay();
			GPTP_LOG_INFO("neighborPropDelayThresh: %ld", iniParser.getNeighborPropDelayThresh());
			GPTP_LOG_INFO("syncReceiptThreshold: %d", iniParser.getSyncReceiptThresh());
			GPTP_LOG_INFO("Address reg socket Port: %d", iniParser.AdrRegSocketPort());

#ifndef APTP
			/* If using config file, set the neighborPropDelayThresh.
			 * Otherwise it will use its default value (800ns) */
			portInit.neighborPropDelayThreshold =
				iniParser.getNeighborPropDelayThresh();
#endif

			priority1 = iniParser.getPriority1();

			/* If using config file, set the syncReceiptThreshold, otherwise
			 * it will use the default value (SYNC_RECEIPT_THRESH)
			 */
			k.pPort->setSyncReceiptThresh(iniParser.getSyncReceiptThresh());

			k.pPort->setSyncInterval(iniParser.getInitialLogSyncInterval());
			GPTP_LOG_VERBOSE("pPort->getSyncInterval:%d", k.pPort->getSyncInterval());

			// Set the delay_mechanism from the ini file valid values are E2E or P2P
			k.pPort->setDelayMechanism(V2_E2E);

			// Allows for an initial setting of unicast send and receive 
			// ip addresses if the destination platform doesn't have
			// socket support for the send and receive address registration api
			k.pPort->UnicastSendNodes(iniParser.UnicastSendNodes());
			k.pPort->UnicastReceiveNodes(iniParser.UnicastReceiveNodes());

			// Set the address registration api ip and port
			k.pPort->AdrRegSocketPort(iniParser.AdrRegSocketPort());

			/*Only overwrites phy_delay default values if not input_delay switch enabled*/
			if (!input_delay)
			{
				ether_phy_delay = iniParser.getPhyDelay();
			}
		}

	}

	if (!k.pPort->init_port()) {
		GPTP_LOG_ERROR("failed to initialize port");
		GPTP_LOG_UNREGISTER();
		return -1;
	}

	if (nullptr == ipc_arg)
	{
		ipc_arg = new LinuxIPCArg;
	}
	ipc_arg->AdrRegSocketPort(k.pPort->AdrRegSocketPort());
	if (!k.ipc->init(ipc_arg))
	{
		delete k.ipc;
		k.ipc = nullptr;
	}

	delete ipc_arg;
	ipc_arg = nullptr;

	k.pClock = new IEEE1588Clock(false, syntonize, priority1, k.timerq_factory, k.ipc,
	  k.lock_factory);

	k.pPort->setClock(k.pClock);

	if (restoredataptr != nullptr)
	{
		if (!restorefailed)
		{
			restorefailed =
				!k.pClock->restoreSerializedState( restoredataptr, &restoredatacount );
		}
		restoredataptr = ((char *)restoredata) + (restoredatalength - restoredatacount);
	}

	if (portInit.automotive_profile) {
		if (portInit.isGM) {
			port_state = PTP_MASTER;
		}
		else {
			port_state = PTP_SLAVE;
		}
		override_portstate = true;
	}

	if( override_portstate ) {
		k.pPort->setPortState( port_state );
	}

	// Start PPS if requested
	if( pps ) {
		if( !timestamper->HWTimestamper_PPS_start()) {
			GPTP_LOG_ERROR("Failed to start pulse per second I/O");
		}
	}

#ifdef APTP
	// Start the address api listener
	AAddressRegisterListener adrListener(k.ifname->Name(), k.pPort);
	std::shared_ptr<LinuxThreadFactory> addressApiThreadFactory =
	 std::make_shared<LinuxThreadFactory>();
	adrListener.ThreadFactory(addressApiThreadFactory);
	adrListener.Start();
	//std::thread addressApiListenerThread(openAddressApiPortWrapper, std::ref(adrListener));
#endif

	// Configure persistent write
	if (pGPTPPersist) {
		off_t len = 0;
		restoredatacount = 0;
		k.pClock->serializeState(NULL, &len);
		restoredatacount += len;
		k.pPort->serializeState(NULL, &len);
		restoredatacount += len;
		pGPTPPersist->setWriteSize((uint32_t)restoredatacount);
		pGPTPPersist->registerWriteCB(gPTPPersistWriteCB);
	}

	k.pPort->processEvent(POWERUP);

	do {
		sig = 0;

		if (sigwait(&set, &sig) != 0) {
			perror("sigwait()");
			GPTP_LOG_UNREGISTER();
			return -1;
		}

		if (sig == SIGHUP) {
			if (pGPTPPersist) {
			  // If port is either master or slave, save clock and then port state
			  if (k.pPort->getPortState() == PTP_MASTER || k.pPort->getPortState() == PTP_SLAVE) {
				pGPTPPersist->triggerWriteStorage();
			  }
			}
		}

		if (sig == SIGUSR2) {
			k.pPort->logIEEEPortCounters();
		}
	} while (sig == SIGHUP || sig == SIGUSR2);

#ifdef APTP
	adrListener.Stop();
#endif

	GPTP_LOG_ERROR("Exiting on %d", sig);

	if (pGPTPPersist) {
		pGPTPPersist->closeStorage();
	}

	// Stop PPS if previously started
	if( pps ) {
		if( !timestamper->HWTimestamper_PPS_stop()) {
			GPTP_LOG_ERROR("Failed to stop pulse per second I/O");
		}
	}

	GPTP_LOG_UNREGISTER();
	return 0;
}

void gPTPPersistWriteCB(char *bufPtr, uint32_t bufSize)
{
	off_t restoredatalength = bufSize;
	off_t restoredatacount = restoredatalength;
	char *restoredataptr = NULL;

	GPTP_LOG_INFO("Signal received to write restore data");

	restoredataptr = (char *)bufPtr;
	AKeeper::pClock->serializeState(restoredataptr, &restoredatacount);
	restoredataptr = ((char *)bufPtr) + (restoredatalength - restoredatacount);
	AKeeper::pPort->serializeState(restoredataptr, &restoredatacount);
	restoredataptr = ((char *)bufPtr) + (restoredatalength - restoredatacount);
}
