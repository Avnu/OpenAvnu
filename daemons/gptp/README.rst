Introduction
------------
This is an AVnu provided gptp daemon which can be used on Linux and
Windows platforms. On Windows, 802.11 wireless and wired Ethernet
network media are supported.  There are a number of other ptp daemons
available for Linux which can be used to establish clock
synchronization, although not all may export the required APIs needed
for an AVB system.

The daemon communicates with other processes through a named pipe for
Windows and shared memory in Linux.  The message format is defined in
ipcdef.hpp.

The message format is:

Integer64	<Master-Local Phase Offset>		ml_phoffset;
LongDouble	<Master-Local Frequency Offset>		ml_freqoffset;
Integer64	<Local-System Phase Offset>		ls_phoffset;
LongDouble	<Local-System Frequency Offset>		ls_freqoffset;
UInteger64	<Local Network Time>			local_time;
UInteger64	<System Tick Frequency>			tick_frequency;

Meaning of IPC provided values
++++++++++++++++++++++++++++++
- master  ~= local  - <master-local phase offset>
- local   ~= system - <local-system phase offset>
- Dmaster ~= Dlocal * (1-<master-local phase offset>/1e12) (where D denotes a delta rather than a specific value)
- Dlocal ~= Dsystem * (1-<local-system freq offset>/1e12) (where D denotes a delta rather than a specific value)

Linux Specific
++++++++++++++

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

The daemon creates a shared memory segment with the 'ptp' group. Some
distributions may not have this group installed.  The IPC interface
will not available unless the 'ptp' group is available.



Windows Version
+++++++++++++++

Build Dependencies:

* WinPCAP Developer's Pack (WpdPack) is required for linking.  It can be
downloaded from http://www.winpcap.org/devel.htm.

Extract WpdPack so the Include folder is in the following location:

%USERPROFILE%
	\src\pcap
		\Include
		\Lib
		\Lib\x64

* the WinPCAP binary must also be installed on any machine where the daemon
runs.

The wireless daemon required downloading the "complete" wireless
driver package from intel.com.  This will be labeled "Software *and*
Drivers"

Running:

To run the Ethernet gPTP daemon from the command line:

	daemon_cl.exe xx-xx-xx-xx-xx-xx

where xx-xx-xx-xx-xx-xx is the mac address of the local interface

To run the wireless daemon:

	daemon_wl.exe xx-xx-xx-xx-xx-xx yy-yy-yy-yy-yy-yy zz-zz-zz-zz-zz-zz

Where	xx* is the HW address of the local wireless adapter
	yy* is the address of the local wireless network (for example, the
		Wi-Fi direct address is different that that of the adapter)
	zz* is the address of the remote device known to the local device

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


