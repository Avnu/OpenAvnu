## Introduction
This readme covers the **optional** feature to use GENIVI DLT (Diagnostic Log and Trace) 
for logging withiin gPTP. General information about GENIVI DLT can be found at:

Primary DLT site:
https://at.projects.genivi.org/wiki/display/PROJ/Diagnostic+Log+and+Trace

DLT on GitHub:
https://github.com/GENIVI/dlt-daemon


## Build Related

DLT support in gPTP is a build time option.

### DLT must be built separately first before it can be used within gPTP.
- Get the latest DLT from github
	- https://github.com/GENIVI/dlt-daemon.git
- Follow instructions in INSTALL file.
- For example (Building with Shared libraries off)
	- mkdir build
	- cd build
	- cmake -DBUILD_SHARED_LIBS=OFF ..
	- make
	- sudo make install
		- Install places static lib in: /usr/local/lib/x86_64-linux-gnu/static/libdlt.a
		- Install places header files in: /usr/local/include/dlt/dlt.h

### Building gPTP with DLT loggging enabled.
- May need to update the gptp Makefile to specify location of DLT headers and lib
- GENIVI_DLT=1 make gptp


## Running 
### DLT 
- See the DLT documentaion about how to run the dlt-daemon.
- For example
	- dlt-daemon -d
		- Runs the daemon
	- dlt-receive -a localhost
		- Simple test client included in DLT to recieve DLT messages. 
	- dlt-user-example "Test message"
		- Test the DLT client and daemon.

### gPTP
- Run as normal.


