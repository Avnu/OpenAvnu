BUILD_DIR=../build
COMMON_DIR=../common
KER_VER=$(`uname -r`)
INCFLAGS=$(COMMON_DIR)/maap_protocol.c 
CFLAGS=$(OPT) -Wall
EXTRA_FLAGS=-I$(COMMON_DIR) 
FLAGS=-lpthread -lpcap

all: maap_daemon

maap_daemon: maap_linux.c
	gcc -o $(BUILD_DIR)/maap_daemon $(INCFLAGS) maap_linux.c $(CFLAGS) $(FLAGS) $(EXTRA_FLAGS)
	echo $(KER_VER)

clean:
	rm -rf $(BUILD_DIR)/maap_daemon
