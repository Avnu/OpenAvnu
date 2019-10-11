AVBLIB_DIR = ../../lib/common
AVBLIB_OBJS = avb_atl.o
AVBLIB_TARGETS = $(addprefix $(AVBLIB_DIR)/,$(AVBLIB_OBJS))

ASYNC_PCAP_DIR = ../common
ASYNC_PCAP_OBJS = async_pcap_storing.o
ASYNC_PCAP_TARGETS = $(addprefix $(ASYNC_PCAP_DIR)/,$(ASYNC_PCAP_OBJS))

ATLLIB_DIR = ../../lib/atl_avb/lib
DAEMONS_DIR = ../../daemons

CC?=gcc
OPT=-O0 -g
WARN=-Wall -Wextra -Wno-parentheses
CFLAGS=$(OPT) $(WARN)
CPPFLAGS=-I$(ATLLIB_DIR) -I$(AVBLIB_DIR) -I$(ASYNC_PCAP_DIR) -I$(DAEMONS_DIR)/common -DAVB_FEATURE_ATL -DAVB_FEATURE_IGB
LDLIBS=-latl -lpci -lrt -lm -pthread -lpcap
LDFLAGS=-L$(ATLLIB_DIR)

.PHONY: all clean

all: send_packet_precisely

send_packet_precisely: send_packet_precisely.o $(ASYNC_PCAP_TARGETS) $(AVBLIB_TARGETS)

send_packet_precisely.o: send_packet_precisely.c

$(AVBLIB_DIR)/%.o: $(AVBLIB_DIR)/%.h $(AVBLIB_DIR)/%.c
	make -C $(AVBLIB_DIR) $@

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) send_packet_precisely
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
