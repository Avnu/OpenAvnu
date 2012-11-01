========================================================================
(C) Copyright 2009-2012 Intel Corporation, All Rights Reserved
Author: Christopher Hall <christopher.s.hall@intel.com>
========================================================================

========================================================================
    CONSOLE APPLICATION : daemon_cl Project Overview
========================================================================

* Dependencies

	WinPCAP Developer's Pack is required for linking (WpdPack_*.zip)
	WinPCAP must also be installed on any machine where the daemon runs (WinPcap_*.exe installer for windows)

To run from the command line:

daemon_cl.exe xx-xx-xx-xx-xx-xx

	where xx-xx-xx-xx-xx-xx is the mac address of the local interface

* Terminology

	master - 802.1AS Grandmaster Clock
	local  - local network device clock (802.1AS timestamp source)
	system - clock use elsewhere on a PC-like device, e.g. TSC or HPET timer

* Interprocess Communication:

The daemon communicates with other user processes through a named pipe.  The pipe name and message format is defined in ipcdef.hpp.

The pipe name is "gptp-update".  An example is in the project named_pipe_test.

The message format is:

    Integer64  <master-local phase offset>
    Integer64  <local-system phase offset>
    Integer32  <master-local frequency offset>
    Integer32  <local-system frequency offset>
    UInteger64 <local time of last update>

* Meaning of IPC provided values:

	master  ~= local  - <master-local phase offset>
	local   ~= system - <local-system phase offset>
	Dmaster ~= Dlocal * (1-<master-local phase offset>/1e12)
		(where D denotes a delta rather than a specific value)
	Dlocal ~= Dsystem * (1-<local-system freq offset>/1e12)
		(where D denotes a delta rather than a specific value)

* Known Limitations:

	* There are problems with timestamp accuracy create problems using switches that impose limits on the peer rate offset
	* The current driver version does not allow timestamping between the system clock (e.g. TCS) and the network device clock;
		systems offsets are not valid


