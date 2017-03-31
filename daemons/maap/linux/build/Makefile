COMMON_DIR=../../common
LINUX_DIR=../src
TEST_DIR=../../test

OPT?=-O2
CFLAGS=$(OPT) -Wall
CPPFLAGS=-I$(COMMON_DIR)
CC?=gcc

VPATH=$(LINUX_DIR) $(COMMON_DIR) $(TEST_DIR)

BINARIES=maap_daemon maap_test test_intervals

.PHONY: all clean

all: $(BINARIES)

# Header dependencies

intervals.o: intervals.h

maap.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_packet.h maap_log.h
maap_net.o: maap_net.h maap_log.h
maap_packet.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_packet.h maap_log.h
maap_parse.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_parse.h maap_log.h
maap_log_queue.o: maap_log_queue.h maap_log.h

maap_daemon.o: maap.h intervals.h maap_iface.h maap_timer.h maap_net.h platform.h maap_parse.h maap_packet.h
maap_log_linux.o: maap_log_queue.h maap_helper_linux.h platform.h
maap_timer_linux.o: maap_timer.h platform.h

test_intervals.o: intervals.h

# Binary targets

maap_daemon: LDLIBS = -lrt -lpthread
maap_daemon: maap_log_linux.o maap_timer_linux.o intervals.o maap.o maap_net.o maap_packet.o maap_parse.o maap_log_queue.o

maap_test: LDLIBS = -lpcap

test_intervals: intervals.o

# Utility targets

clean:
	rm -f *.o
	rm -f $(BINARIES)
