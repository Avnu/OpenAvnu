
====================================
MMRP, MVRP, MSRP documentation
====================================

The MRP daemon is required to establish stream reservations with compatible AVB
infrastructure devices. The command line selectively enables MMRP (via the 
-m option), MVRP (via the -v option), and MSRP (via the -s option). You must 
also specify the interface on which you want to bind the daemon to 
(e.g. -i eth2). The full command line typically appears as follows:
	sudo ./mrpd -mvs -i eth2

Sample client applications - mrpctl, mrpq, mrpl - illustrate how to connect, 
query and add attributes to the MRP daemon.

General command string format
=============================

CCC:X=12233,Y=34567

where CCC is the 3 character command and X and Y are parameters followed by their values.


MSRP
====

S??: query MSRP Registrar database

S+?: (re)JOIN a stream

S++: NEW a stream
	
S--: LV a stream

S+L: Report a listener status

S-L: Withdraw a listener status

S+D: Report a domain status

S-D: Withdraw a domain status


