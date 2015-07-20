# STC AVTP Pipeline Contribution Notes

## General Status
- Consider the AVTP Pipeline a work in progress.
- Integrated with OpenAVB:
    - gPTP
- Not yet integreated with OpenAVB:
    - MSRP
    - igb direct for packet TX. Current linux raw sockets are used for both TX and RX.
    - igb credit based shaper
    - Build system
- Very limited testing as been performed. Primarily just the Echo talker which is a simple test stream.
- Documentation and doc generation has not been fully updated.

## Building Current OpenAVB
### Tool chain and libraries
- Ubuntu 14.04
- Build "open-avb-next" branch
- Install ($ sudo apt-get install ...)
    - $ sudo apt-get install build-essential
    - $ sudo apt-get install libpcap-dev
    - $ sudo apt-get install libpci-dev
    - $ sudo apt-get install libsndfile1-dev
    - $ sudo apt-get install libjack-dev
    - $ sudo apt-get install linux-headers-generic
        - linux-headers (same version as the kernel you're building for)

### Building
- Building from the repo root
- $ make igb
- $ make lib
- $ ARCH=I210 make gptp
- $ make mrpd
- $ make maap
- $ make examples_all

## Building STC AVTP Pipeline
### Get packages
- $ sudo apt-get install libglib2.0-dev
- $ sudo apt-get install libgstreamer0.10-dev
- $ sudo apt-get install libgstreamer-plugins-base0.10-dev
- $ sudo apt-get install libasound2-dev 

### Setup AVTP Pipeline build directory
- Building from the repo root
- $ cd ..
- $ mkdir build
- $ cd build
- `$ cmake -DCMAKE_TOOLCHAIN_FILE=repo/lib/avtp_pipeline/platform/Linux/x86_i210_linux.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../repo/lib/avtp_pipeline`

### Build AVTP Pipeline
- $ make all install

## Running OpenAVB
- Helper scripts in the repo root.
- `$ sudo ./run_igb.sh eth1`
    - Load the igb driver. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_gptp.sh eth1`
    - Start gptp daemon. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_srp.sh eth1`
    - Start msrp daemon. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_simple_talker.sh eth1`
    - Run the current OpenAVB simple talker example.  Supply the interface name (ethx) as parameter.

## Running OpenAVB with STC Echo Talker (without SRP currently)
- `$ sudo ./run_igb.sh eth1`
    - Load the igb driver. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_gptp.sh eth1`
    - Start gptp daemon. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_echo_talker.sh eth1`
    - Run the AVTP Echo talker test stream. Supply the interface name (ethx) as parameter.

