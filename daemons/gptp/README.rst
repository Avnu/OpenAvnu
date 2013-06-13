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

	Integer32	<master-local frequency offset>

	Integer32	<local-system frequency offset>

	UInteger64	<local time of last update>

Meaning of IPC provided values
++++++++++++++++++++++++++++++
- master  ~= local  - <master-local phase offset>
- local   ~= system - <local-system phase offset>
- Dmaster ~= Dlocal * (1-<master-local phase offset>/1e12) (where D denotes a delta rather than a specific value)
- Dlocal ~= Dsystem * (1-<local-system freq offset>/1e12) (where D denotes a delta rather than a specific value)

Known Limitations
+++++++++++++++++

* There are problems with timestamp accuracy create problems using switches that impose limits on the peer rate offset

* The current Windows driver version does not allow timestamping between the system clock (e.g. TCS) and the network device clock; systems offsets are not valid


Linux Specific
++++++++++++++

To build, execute the linux/build makefile.

To execute, run 
	./daemon_cl <interface-name>
such as
	./daemon_cl eth0

Windows Version
+++++++++++++++

Build Dependencies

* WinPCAP Developer's Pack (WpdPack) is required for linking - downloadable from http://www.winpcap.org/devel.htm.

Extract WpdPack so the Include folder is in one of the below locations

1- %ProgramData%
	\WpdPack
		\Include
		\Lib
		\Lib\x64

2- %USERPROFILE%
	\src\pcap
		\Include
		\Lib
		\Lib\x64

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

  Note with thsi version to use gPTP specific settings, which differ 
  slightly from IEEE 1588.

* http://ptpd.sourceforge.net/

* http://ptpd2.sourceforge.net/

* http://code.google.com/p/ptpv2d

* http://home.mit.bme.hu/~khazy/ptpd/


