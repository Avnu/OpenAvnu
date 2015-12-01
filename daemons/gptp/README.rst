Introduction
------------
This is an example Intel provided gptp daemon which can be used on Linux
and Windows platforms. There are a number of other ptp daemons available
for Linux which can be used to establish clock synchronization, although
not all may export the required APIs needed for an AVB system.

The daemon communicates with other processes through a named pipe.
The pipe name and message format is defined in ipcdef.hpp.  The pipe name 
is "gptp-update". A Windows example is in the project named_pipe_test.

The message format is:

	Integer64	<master-local phase offset>

	Integer64	<local-system phase offset>

	Float	<master-local frequency offset>

	Float	<local-system frequency offset>

	UInteger64	<local time of last update>

Meaning of IPC provided values
++++++++++++++++++++++++++++++
- master  ~= local  - <master-local phase offset>
- local   ~= system - <local-system phase offset>
- Dmaster ~= Dlocal * (1-<master-local phase offset>/1e12) (where D denotes a delta rather than a specific value)
- Dlocal ~= Dsystem * (1-<local-system freq offset>/1e12) (where D denotes a delta rather than a specific value)

Linux Specific
++++++++++++++

Requirements for documentation on a ubuntu based system:
    - cmake: sudo apt-get install cmake
    - doxygen: sudo apt-get install doxygen
    - graphviz: sudo apt-get install graphviz

To build, execute the linux/build makefile.

To build for I210:

ARCH=I210 make clean all

To build for 'generic' Linux:

make clean all

To build for Intel CE 5100 Platforms:

ARCH=IntelCE make clean all

To execute, run 
	./daemon_cl <interface-name>
such as
	./daemon_cl eth0

The daemon creates a shared memory segment with the 'ptp' group. Some distributions may not have this group installed.  The IPC interface will not available unless the 'ptp' group is available.


Windows Version
+++++++++++++++

Build Dependencies

* WinPCAP Developer's Pack (WpdPack) is required for linking - downloadable from http://www.winpcap.org/devel.htm.

The following environment variables must be defined:
* WPCAP_DIR the directory where WinPcap is installed

* WinPCAP must also be installed on any machine where the daemon runs.

To run from the command line:

	daemon_cl.exe xx-xx-xx-xx-xx-xx

where xx-xx-xx-xx-xx-xx is the mac address of the local interface

Other Available PTP Daemons
---------------------------
There are a number of existing ptp daemon projects. Some of the other known 
ptp daemons are listed below. Intel has not tested Open AVB with the following 
ptp daemons.

* Richard Cochran's ptp4l daemon - https://sourceforge.net/p/linuxptp/

  Note with this version to use gPTP specific settings, which differ 
  slightly from IEEE 1588.

* http://ptpd.sourceforge.net/

* http://ptpd2.sourceforge.net/

* http://code.google.com/p/ptpv2d

* http://home.mit.bme.hu/~khazy/ptpd/


