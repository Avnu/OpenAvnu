
MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

IGBLIB_DIR = ../../lib/igb_avb/lib
IGBLIB_OBJS = igb.o
IGBLIB_TARGETS = $(addprefix $(IGBLIB_DIR)/,$(IGBLIB_OBJS))

DAEMONS_DIR = ../../daemons

CC ?= gcc
OPT = -O2 -g
WARN=-Wall -Wextra -Wno-parentheses
CFLAGS=$(OPT) $(WARN)
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common -I$(IGBLIB_DIR)
# GLIBC versions starting with 2.17 don't need -lrt anymore
LDLIBS = -lpcap -lsndfile -pthread -lpci -lrt

all: simple_rx

simple_rx: simple_rx.o $(MRPLISTENER_TARGETS) $(IGBLIB_TARGETS)

simple_rx.o: simple_rx.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

$(IGBLIB_DIR)/%.o: $(IGBLIB_DIR)/%.c $(IGBLIB_DIR)/%.h
	make -C $(IGBLIB_DIR) $@

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM)  simple_rx
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
