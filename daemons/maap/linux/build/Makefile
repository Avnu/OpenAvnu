COMMON_DIR=../../common
LINUX_DIR=../src
TEST_DIR=../../test

OPT=-O2
CFLAGS=$(OPT) -Wall
CPPFLAGS=-I$(COMMON_DIR)
CC?=gcc

VPATH=$(LINUX_DIR) $(COMMON_DIR) $(TEST_DIR)

BINARIES=maap_daemon maap_test test_intervals test_maap_packet_packing

.PHONY: all clean

all: $(BINARIES)

# Header dependencies

intervals.o: intervals.h

maap.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_packet.h
maap_packet.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_packet.h
maap_parse.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_parse.h

maap_daemon.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_parse.h maap_packet.h
maap_net_linux.o: maap_net.h
maap_timer_linux.o: maap_timer.h platform.h

test_intervals.o: intervals.h

test_maap_packet_packing.o: maap_packet.h platform.h

# Binary targets

maap_daemon: LDLIBS = -lrt
maap_daemon: maap_net_linux.o maap_timer_linux.o intervals.o maap.o maap_packet.o maap_parse.o

maap_test: LDLIBS = -lpcap

test_intervals: intervals.o

test_maap_packet_packing: maap_packet.o

# Utility targets

clean:
	rm -f *.o
	rm -f $(BINARIES)
