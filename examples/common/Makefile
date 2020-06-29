DAEMONS_DIR = ../../daemons

CC ?= gcc
OPT = -O2 -g
WARN = -Wall -Wextra -Wno-parentheses
CFLAGS = $(OPT) $(WARN)
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(DAEMONS_DIR)/common

all: talker_mrp_client.o listener_mrp_client.o async_pcap_storing.o

talker_mrp_client.o: talker_mrp_client.c talker_mrp_client.h

listener_mrp_client.o: listener_mrp_client.c listener_mrp_client.h

async_pcap_storing.o: async_pcap_storing.c async_pcap_storing.h

clean:
	$(RM)  talker_mrp_client.o listener_mrp_client.o async_pcap_storing.o
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
