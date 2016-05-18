## Introduction
This readme covers only the additional features added to the gPTP daemon
to support the AVnu Automotive Profile (AP).

## Automotive Profile Features Implemented
- In general all features of the AP support for a Slave device have been
implemented with the following exceptions.
	- Windows OS platform code has not been implemented.
- Detailed list of the AP implementation includes
	- Configuration
		- Static gPTP values (AutoCDS 6.2.1)
		- Persistent gPTP values (AutoCDS 6.2.2)
		- Management Updated Values (AutoCDS 6.2.3)
		- Signalling Message Configuration (AutoCDS 6.2.4)
		- Followup TLV (AutoCDS 6.2.5)
		- Required Values for gPTP (AutoCDS 6.2.6)
	- Operation
		- Disable BMCA (AutoCDS 6.3)
		- No verification of sourcePortIdentity (AutoCDS 6.3)
		- Sync Holdover (AutoCDS 6.3)
		- Sync Recovery (AutoCDS 6.3)
		- Implement Signalling Message (802.1AS 2011 10.5.4)
		- Implement Test Mode (AutoCDS 5.3)
	- Exception Handling
		- Link State Change (AutoCDS 9.2.1)
		- Loss of Sync Messages (AutoCDS 9.3.1)
		- Non-Continuous Sync Values (AutoCDS 9.3.2)
		- Pdelay Response Timeout (AutoCDS 9.3.3) (Not implemented)
		- neighborPropDelay value (AutoCDS 9.3.4)
	- Diagnostic counters (AutoCDS 13)
		- Abstracted with only simple dump to log upon SIGUSR2 signal.

## Non-Automotive Profile Changes
- Added fuller feature logging within gPTP
- Updated the various logging macros and direct usage of printfs to use the new
logging
- IEEE1588Port class initialization changed to reduced the 14+ constructor
parameters to a single init parameter.
- Link up and Link down detection added.
- Abstracted the state save functionality to allow for easier integration to
other platforms.

## New Command Line Options
- E enable test mode (as defined in AVnu automotive profile)
- V enable AVnu Automotive Profile
- GM set grandmaster for Automotive Profile
- INITSYNC <value> initial sync interval (Log base 2. 0 = 1 sec)
- OPERSYNC <value> operational sync interval (Log base 2. 0 = 1 sec)
- INITPDELAY <value> initial pdelay interval (Log base 2. 0 = 1 sec)
- OPERPDELAY <value> operational pdelay interval (Log base 2. 0 = 1 sec)

## Build Related
- There are no changes to the standard build for gPTP

## Execution Related
- Other than the new command line options there are no changes to starting gPTP
- Example
	- ./run_gptp.sh eth1 -M statefile -V
		- Start with AP and a specific file name for saving state information.

## Run Time Related
- Signals
	- SIGHUP: writes the current gPTP state information to file.
	- SIGUSR2: Dumps the diagnostic counters to the log

## Known Issues
- When running as slave (none -GM) there may be a SYNC rx timeout exception reported when signalling to a different SYNC rate.
- Link down or link up may be incorrectly detected at times.

## Windows Functionality That is Missing
- The initial implementation of the Automotive Profile for gPTP was done for Linux only. 
- To locate the functionality that must be implemented for windows one approach is to look at the pull request 
at https://github.com/AVnu/Open-AVB/pull/355 and review all the differences in the Linux folder.
- Brief summary to give a sense of scope of updates needed for Windows
	- MakeFile 
		- Updates for new files
	- daemon_cl.cpp
		- New command line options
		- Changed state save and restore (persistence)
		- Automotive profile configuration setup
		- Updates to signal handling
		- Other updates as well
	- linux_hal_common.cpp
		- Link Up and Down functionality
	- linux_hal_common.hpp
		- Function prototypes updated
	- linux_hal_persist_file.cpp
		- New functionality
	- linux_hal_persist_file.hpp
		- New functionality

