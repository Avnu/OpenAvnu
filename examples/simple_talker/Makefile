AVBLIB_DIR = ../../lib/common
AVBLIB_OBJS = avb_avtp.o avb_gptp.o avb_igb.o
AVBLIB_TARGETS = $(addprefix $(AVBLIB_DIR)/,$(AVBLIB_OBJS))

MRPCLIENT_DIR = ../common
MRPTALKER_OBJS = talker_mrp_client.o
MRPTALKER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPTALKER_OBJS))

IGBLIB_DIR = ../../lib/igb_avb/lib
DAEMONS_DIR = ../../daemons

CC?=gcc
OPT=-O2 -g
WARN=-Wall -Wextra -Wno-parentheses
CFLAGS=$(OPT) $(WARN)
CPPFLAGS=-I$(IGBLIB_DIR) -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(AVBLIB_DIR) -I$(DAEMONS_DIR)/common -DAVB_FEATURE_ATL=0
LDLIBS=-ligb -lpci -lrt -lm -pthread
LDFLAGS=-L$(IGBLIB_DIR)

.PHONY: all clean

all: simple_talker

simple_talker: simple_talker.o $(MRPTALKER_TARGETS) $(AVBLIB_TARGETS)

simple_talker.o: simple_talker.c

$(AVBLIB_DIR)/%.o: $(AVBLIB_DIR)/%.h $(AVBLIB_DIR)/%.c
	make -C $(AVBLIB_DIR) $@

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) simple_talker
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
