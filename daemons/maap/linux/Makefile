BUILD_DIR=.
COMMON_DIR=../common
LINUX_DIR=src
TEST_DIR=../test
CFLAGS=$(OPT) -Wall
EXTRA_FLAGS=-I$(COMMON_DIR) 
FLAGS=-lpthread -lpcap -lrt
CC?=gcc

all: maap_daemon maap_test test_intervals test_maap_packet_packing

# common/maap_protocol.c, common/maap_protocol.h, and src/maap_linux.c are current OpenAVB files.
# test/endian.h is licensed using the GNU Lesser General Public License version 2.1

maap_daemon_legacy: $(LINUX_DIR)/maap_linux_legacy.c
	$(CC) -o $(BUILD_DIR)/maap_daemon_legacy $(COMMON_DIR)/maap_protocol.c src/maap_linux_legacy.c $(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)

maap_daemon: $(LINUX_DIR)/maap_linux.c $(LINUX_DIR)/maap_net_linux.c $(LINUX_DIR)/maap_timer_linux.c \
		$(COMMON_DIR)/intervals.c $(COMMON_DIR)/maap.c $(COMMON_DIR)/maap_packet.c $(COMMON_DIR)/maap_parse.c
	$(CC) -o $(BUILD_DIR)/maap_daemon \
		$(LINUX_DIR)/maap_linux.c $(LINUX_DIR)/maap_net_linux.c $(LINUX_DIR)/maap_timer_linux.c \
		$(COMMON_DIR)/intervals.c $(COMMON_DIR)/maap.c $(COMMON_DIR)/maap_packet.c $(COMMON_DIR)/maap_parse.c \
		$(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)


maap_test: $(TEST_DIR)/maap_test.c
	$(CC) -o $(BUILD_DIR)/maap_test $(TEST_DIR)/maap_test.c $(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)

test_intervals: $(TEST_DIR)/test_intervals.c
	$(CC) -o $(BUILD_DIR)/test_intervals $(TEST_DIR)/test_intervals.c $(COMMON_DIR)/intervals.c $(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)

test_maap_packet_packing: $(TEST_DIR)/test_maap_packet_packing.c
	$(CC) -o $(BUILD_DIR)/test_maap_packet_packing $(TEST_DIR)/test_maap_packet_packing.c $(COMMON_DIR)/maap_packet.c $(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)


clean:
	rm -rf $(BUILD_DIR)/maap_daemon_legacy
	rm -rf $(BUILD_DIR)/maap_daemon
	rm -rf $(BUILD_DIR)/maap_test
	rm -rf $(BUILD_DIR)/test_intervals
	rm -rf $(BUILD_DIR)/test_maap_packet_packing
