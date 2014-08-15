Introduction
------------

Multicast MAC addresses or locally administered unicast addresses are required
by AVTP for the transmission of media streams. Because AVTP runs directly on a
layer 2 transport, there is no existing protocol to allocate multicast MAC
addresses dynamically. MAAP is designed to provide a way to allocate dynamically
the multicast MAC addresses needed by AVTP. MAAP is not designed to allocate locally
administered unicast addresses.

A block of Multicast MAC addresses has been reserved for the use of AVTP. These addresses are listed in
MAAP Dynamic Allocation Pool below shown. These addresses are available for dynamic allocation by the MAAP.
91:E0:F0:00:00:00 – 91:E0:F0:00:FD:FF


Linux Specific
++++++++++++++

To build, execute the linux makefile.

To execute(in root), go to build directory(where binary is created) and run
	maap_daemon [-d] -i interface-name
such as
	./maap_daemon -i eth0

The daemon creates a 6 bytes shared memory segment with the key 1234 and writes the allocated mac-addr there.
The client applications will have to use this key value 1234 in the applications and read the allocated mac-addr
and use it as dest mac-addr for the packets transmission.


Execute Command :
------------

	sudo ./maap_daemon -i eth0


OUTPUT :
------------

$ sudo ./maap_daemon -i eth0

SENT MAAP_PROBE 

SENT MAAP_PROBE 

SENT MAAP_PROBE 

SENT MAAP_ANNOUNCE 

ANNOUNCED ADDRESS:
0x91 0xe0 0xf0 0 0x95 0x18 

STATE change to DEFEND:
   
SENT MAAP_ANNOUNCE
SENT MAAP_ANNOUNCE
SENT MAAP_ANNOUNCE

Here in the case the allocated address which starts from 0x91 0xe0 0xf0 0 0x95 0x18 
This mac-addr is assigned to Dest-Mac for packets transfer.
ADDR[0] = 0x91
ADDR[1] = 0xe0
ADDR[2] = 0xf0
ADDR[3] = 0
ADDR[4] = 0x95
ADDR[5] = 0x18
