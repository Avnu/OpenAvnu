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
#include "gptp_cfg.hpp"

#ifdef ARCH_INTELCE
#include "linux_hal_intelce.hpp"
#else
#include "linux_hal_generic.hpp"
#endif
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#define PHY_DELAY_GB_TX_I20 184 //1G delay
#define PHY_DELAY_GB_RX_I20 382 //1G delay
#define PHY_DELAY_MB_TX_I20 1044//100M delay
#define PHY_DELAY_MB_RX_I20 2133//100M delay

void print_usage( char *arg0 ) {
    fprintf( stderr,
            "%s <network interface> [-S] [-P] [-M <filename>] "
            "[-A <count>] [-G <group>] [-R <priority 1>] [-D <gb_tx_delay,gb_rx_delay,mb_tx_delay,mb_rx_delay>]\n"
            "[-F <path to gptp_cfg.ini file>]\n",
            arg0 );
    fprintf
        ( stderr,
          "\t-S start syntonization\n\t-P pulse per second\n"
          "\t-M <filename> save/restore state\n"
          "\t-A <count> initial accelerated sync count\n"
          "\t-G <group> group id for shared memory\n"
          "\t-R <priority 1> priority 1 value\n"
          "\t-T force master\n\t-L force slave\n"
          "\t-F <path-to-ini-file>\n"
        );
}

int main(int argc, char **argv)
{
    sigset_t set;
    InterfaceName *ifname;
    int sig;

    bool syntonize = false;
    int i;
    bool pps = false;
    uint8_t priority1 = 248;
    bool override_portstate = false;
    PortState port_state;

    int restorefd = -1;
    void *restoredata = ((void *) -1);
    char *restoredataptr = NULL;
    off_t restoredatalength = 0;
    off_t restoredatacount;
    bool restorefailed = false;
    LinuxIPCArg *ipc_arg = NULL;
    int accelerated_sync_count = 0;
    bool use_config_file = false;
    char config_file_path[512];
    memset(config_file_path, 0, 512);

    // Block SIGUSR1
    {
        sigset_t block;
        sigemptyset( &block );
        sigaddset( &block, SIGUSR1 );
        if( pthread_sigmask( SIG_BLOCK, &block, NULL ) != 0 ) {
            fprintf( stderr, "Failed to block SIGUSR1\n" );
            return -1;
        }
    }

    int phy_delay[4]={0,0,0,0};
    bool input_delay=false;

    LinuxNetworkInterfaceFactory *default_factory =
        new LinuxNetworkInterfaceFactory;
    OSNetworkInterfaceFactory::registerFactory
        (factory_name_t("default"), default_factory);
    LinuxThreadFactory *thread_factory = new LinuxThreadFactory();
    LinuxTimerQueueFactory *timerq_factory = new LinuxTimerQueueFactory();
    LinuxLockFactory *lock_factory = new LinuxLockFactory();
    LinuxTimerFactory *timer_factory = new LinuxTimerFactory();
    LinuxConditionFactory *condition_factory = new LinuxConditionFactory();
    LinuxSharedMemoryIPC *ipc = new LinuxSharedMemoryIPC();
    /* Create Low level network interface object */
    if( argc < 2 ) {
        printf( "Interface name required\n" );
        print_usage( argv[0] );
        return -1;
    }
    ifname = new InterfaceName( argv[1], strlen(argv[1]) );

    /* Process optional arguments */
    for( i = 2; i < argc; ++i ) {
        if( argv[i][0] == '-' ) {
            if( toupper( argv[i][1] ) == 'S' ) {
                // Get syntonize directive from command line
                syntonize = true;
            }
            else if( toupper( argv[i][1] ) == 'T' ) {
                override_portstate = true;
                port_state = PTP_MASTER;
            }
            else if( toupper( argv[i][1] ) == 'L' ) {
                override_portstate = true;
                port_state = PTP_SLAVE;
            }
            else if( toupper( argv[i][1] ) == 'M' ) {
                // Open file
                if( i+1 < argc ) {
                    restorefd = open
                        ( argv[i+1], O_RDWR|O_CREAT, S_IRUSR|S_IWUSR ); ++i;
                    if( restorefd == -1 ) printf
                        ( "Failed to open restore file\n" );
                } else {
                    printf( "Restore file must be specified on "
                            "command line\n" );
                }
            }
            else if( toupper( argv[i][1] ) == 'A' ) {
                if( i+1 < argc ) {
                    accelerated_sync_count = atoi( argv[++i] );
                } else {
                    printf( "Accelerated sync count must be specified on the "
                            "command line with A option\n" );
                }
            }
            else if( toupper( argv[i][1] ) == 'G' ) {
                if( i+1 < argc ) {
                    ipc_arg = new LinuxIPCArg(argv[++i]);
                } else {
                    printf( "Must specify group name on the command line\n" );
                }
            }
            else if( toupper( argv[i][1] ) == 'P' ) {
                pps = true;
            }
            else if( toupper( argv[i][1] ) == 'H' ) {
                print_usage( argv[0] );
                return 0;
            }
            else if( toupper( argv[i][1] ) == 'R' ) {
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
            else if(toupper(argv[i][1]) == 'D'){
                input_delay=true;
                int delay_count=0;
                char *cli_inp_delay = strtok(argv[i+1],",");
                while (cli_inp_delay != NULL)
                {
                    if(delay_count>3)
                    {
                        printf("Too many values\n");
                        print_usage( argv[0] );
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
                    return 0;
                }
            }
            else if(toupper(argv[i][1]) == 'F')
            {
                if( i+1 < argc ) {
                    use_config_file = true;
                    strcpy(config_file_path, argv[i+1]);
                } else {
                    fprintf(stderr, "config file must be specified.\n");
                }
            }
        }
    }

    if (!input_delay)
    {
        phy_delay[0] = PHY_DELAY_GB_TX_I20;
        phy_delay[1] = PHY_DELAY_GB_RX_I20;
        phy_delay[2] = PHY_DELAY_MB_TX_I20;
        phy_delay[3] = PHY_DELAY_MB_RX_I20;
    }

    if( !ipc->init( ipc_arg ) ) {
        delete ipc;
        ipc = NULL;
    }
    if( ipc_arg != NULL ) delete ipc_arg;

    if( restorefd != -1 ) {
        // MMAP file
        struct stat stat0;
        if( fstat( restorefd, &stat0 ) == -1 ) {
            printf( "Failed to stat restore file, %s\n", strerror( errno ));
            restoredatalength = 0;
        } else {
            restoredatalength = stat0.st_size;
            if( restoredatalength != 0 ) {
                if(( restoredata = mmap( NULL, restoredatalength,
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                restorefd, 0 )) == ((void *)-1) ) {
                    printf( "Failed to mmap restore file, %s\n",
                            strerror( errno ));
                } else {
                    restoredatacount = restoredatalength;
                    restoredataptr = (char *) restoredata;
                }
            }
        }
    }

    if (argc < 2)
        return -1;
    ifname = new InterfaceName(argv[1], strlen(argv[1]));

#ifdef ARCH_INTELCE
    HWTimestamper *timestamper = new LinuxTimestamperIntelCE();
#else
    HWTimestamper *timestamper = new LinuxTimestamperGeneric();
#endif

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset( &set, SIGTERM );
    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        perror("pthread_sigmask()");
        return -1;
    }

    IEEE1588Clock *clock =
        new IEEE1588Clock( false, syntonize, priority1, timestamper,
                timerq_factory , ipc, lock_factory );

    if( restoredataptr != NULL ) {
        if( !restorefailed )
            restorefailed =
                !clock->restoreSerializedState( restoredataptr,
                        &restoredatacount );
        restoredataptr = ((char *)restoredata) +
            (restoredatalength - restoredatacount);
    }

    IEEE1588Port *port =
        new IEEE1588Port
        ( clock, 1, false, accelerated_sync_count, timestamper, 0, ifname,
          condition_factory, thread_factory, timer_factory, lock_factory );

    if(use_config_file)
    {
        GptpIniParser iniParser(config_file_path);

        if (iniParser.parserError() < 0) {
            fprintf(stderr, "Cant parse ini file. Aborting file reading.\n");
        }
        else
        {
            fprintf(stdout, "priority1 = %d\n", iniParser.getPriority1());
            fprintf(stdout, "announceReceiptTimeout: %d\n", iniParser.getAnnounceReceiptTimeout());
            fprintf(stdout, "syncReceiptTimeout: %d\n", iniParser.getSyncReceiptTimeout());
            fprintf(stdout, "phy_delay_gb_tx: %d\n", iniParser.getPhyDelayGbTx());
            fprintf(stdout, "phy_delay_gb_rx: %d\n", iniParser.getPhyDelayGbRx());
            fprintf(stdout, "phy_delay_mb_tx: %d\n", iniParser.getPhyDelayMbTx());
            fprintf(stdout, "phy_delay_mb_rx: %d\n", iniParser.getPhyDelayMbRx());
            fprintf(stdout, "neighborPropDelayThresh: %ld\n", iniParser.getNeighborPropDelayThresh());
            fprintf(stdout, "syncReceiptThreshold: %d\n", iniParser.getSyncReceiptThresh());

            /* If using config file, set the neighborPropDelayThresh.
             * Otherwise it will use its default value (800ns) */
            port->setNeighPropDelayThresh(iniParser.getNeighborPropDelayThresh());

            /* If using config file, set the syncReceiptThreshold, otherwise
             * it will use the default value (SYNC_RECEIPT_THRESH)
             */
            port->setSyncReceiptThresh(iniParser.getSyncReceiptThresh());

            /*Only overwrites phy_delay default values if not input_delay switch enabled*/
            if(!input_delay)
            {
                phy_delay[0] = iniParser.getPhyDelayGbTx();
                phy_delay[1] = iniParser.getPhyDelayGbRx();
                phy_delay[2] = iniParser.getPhyDelayMbTx();
                phy_delay[3] = iniParser.getPhyDelayMbRx();
            }
        }

    }
    if (!port->init_port(phy_delay)) {
        printf("failed to initialize port \n");
        return -1;
    }

    if( restoredataptr != NULL ) {
        if( !restorefailed ) restorefailed =
            !port->restoreSerializedState( restoredataptr, &restoredatacount );
        restoredataptr = ((char *)restoredata) +
            (restoredatalength - restoredatacount);
    }

    if( override_portstate ) {
        port->setPortState( port_state );
    }

    // Start PPS if requested
    if( pps ) {
        if( !timestamper->HWTimestamper_PPS_start()) {
            printf( "Failed to start pulse per second I/O\n" );
        }
    }

    port->processEvent(POWERUP);

    do {
        if (sigwait(&set, &sig) != 0) {
            perror("sigwait()");
            return -1;
        }

        if (sig == SIGHUP) {
            // If port is either master or slave, save clock and then port state
            if( restorefd != -1 ) {
                if( port->getPortState() == PTP_MASTER ||
                        port->getPortState() == PTP_SLAVE ) {
                    printf( "Signal received to write restore data\n" );
                    off_t len;
                    restoredatacount = 0;
                    clock->serializeState( NULL, &len );
                    restoredatacount += len;
                    port->serializeState( NULL, &len );
                    restoredatacount += len;

                    if( restoredatacount > restoredatalength ) {
                        ftruncate( restorefd, restoredatacount );
                        if( restoredata != ((void *) -1)) {
                            restoredata =
                                mremap( restoredata, restoredatalength, restoredatacount,
                                        MREMAP_MAYMOVE );
                        } else {
                            restoredata =
                                mmap( NULL, restoredatacount, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, restorefd, 0 );
                        }
                        if( restoredata == ((void *) -1 )) goto remap_failed;
                        restoredatalength = restoredatacount;
                    }

                    restoredataptr = (char *) restoredata;
                    clock->serializeState( restoredataptr, &restoredatacount );
                    restoredataptr = ((char *)restoredata) +
                        (restoredatalength - restoredatacount);
                    port->serializeState( restoredataptr, &restoredatacount );
                    restoredataptr = ((char *)restoredata) +
                        (restoredatalength - restoredatacount);
remap_failed:
                    ;;
                }
                if( restoredata != ((void *) -1 ))
                    munmap( restoredata, restoredatalength );
            }
        }
    } while(sig == SIGHUP);

    fprintf(stderr, "Exiting on %d\n", sig);

    // Stop PPS if previously started
    if( pps ) {
        if( !timestamper->HWTimestamper_PPS_stop()) {
            printf( "Failed to stop pulse per second I/O\n" );
        }
    }

    if( ipc ) delete ipc;

    return 0;
}
