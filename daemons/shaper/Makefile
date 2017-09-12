CC ?= gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra
INCFLAGS =
LDLIBS = -lm -pthread

SRC_DIR = src
OUT_O_DIR = build

all: shaper_daemon

shaper_daemon: \
	$(OUT_O_DIR)/shaper_daemon.o \
	$(OUT_O_DIR)/shaper_log_queue.o \
	$(OUT_O_DIR)/shaper_log_linux.o

$(OUT_O_DIR)/shaper_daemon.o: $(SRC_DIR)/shaper_daemon.c \
		$(SRC_DIR)/shaper_log.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCFLAGS) -c $(SRC_DIR)/shaper_daemon.c -o $(OUT_O_DIR)/shaper_daemon.o

$(OUT_O_DIR)/shaper_log_queue.o: $(SRC_DIR)/shaper_log_queue.c \
		$(SRC_DIR)/shaper_log.h $(SRC_DIR)/shaper_log_queue.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCFLAGS) -c $(SRC_DIR)/shaper_log_queue.c -o $(OUT_O_DIR)/shaper_log_queue.o

$(OUT_O_DIR)/shaper_log_linux.o: $(SRC_DIR)/shaper_log_linux.c \
		$(SRC_DIR)/platform.h $(SRC_DIR)/shaper_log.h $(SRC_DIR)/shaper_log_queue.h $(SRC_DIR)/shaper_helper_linux.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCFLAGS) -c $(SRC_DIR)/shaper_log_linux.c -o $(OUT_O_DIR)/shaper_log_linux.o

%: $(OUT_O_DIR)/%.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) shaper_daemon
	$(RM) -r $(OUT_O_DIR)
