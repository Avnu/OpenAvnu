ALSA_INC=/usr/include/alsa
ISAUDK_ROOT:=$(strip $(shell dirname $(realpath $(lastword \
	$(MAKEFILE_LIST)))))
ISAUDK_INC=$(ISAUDK_ROOT)
DOC_DIR=$(ISAUDK_ROOT)/documentation

PTP_INC=$(ISAUDK_ROOT)/../../daemons/gptp/common

CFLAGS=-g -I$(ISAUDK_INC) -Wall -Wextra -Wno-unused-but-set-variable -Werror
EXTERNAL_LIBS=-lasound -lpthread -lsndfile
EXTERNAL_LIB_DIRS=-L/usr/local/lib/

QUIET_CC=@/bin/echo -e '* [CC]\t\t$@';
QUIET_MD=@/bin/echo -e '* [MAKEDEP]\t$@';
QUIET_LD=@/bin/echo -e '* [LD]??$@ {$^}' | fold -w 62 -s | \
	sed -e '1h;2,$$H;$$!d;g' -re 's/\n/\n\t\t/g;s/\?/\t/g';

TEST_TARGETS=alsa_test
PYTHON_HELPERS=play_file_at record_file_at monoraw_to_net_time \
	net_time_to_monoraw

.PHONY: all
all: $(TEST_TARGETS) $(PYTHON_HELPERS)

.PHONY: clean
clean:
	/usr/bin/env rm -f *.o $(TEST_TARGETS) $(PYTHON_HELPERS)

.PHONY: pristine
pristine: clean
	/usr/bin/env rm -f *~ *.d \#*

%.o: %.c
	 $(QUIET_CC) $(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	$(QUIET_MD) $(CC) $(CFLAGS) -MM $< -o $@

-include alsa.d
alsa.o: CFLAGS += -I$(ALSA_INC)

-include monoraw_to_net_time.d
monoraw_to_net_time.o: CFLAGS += -I$(PTP_INC)

-include net_time_to_monoraw.d
net_time_to_monoraw.o: CFLAGS += -I$(PTP_INC)

-include linked_list.d

-include stack.d

-include stream.d

-include mixer.d

-include thread_signal.d

-include args.d

-include alsa_test.d

-include capture.d

alsa_test: alsa_test.o alsa.o stream.o mixer.o linked_list.o thread_signal.o \
	stack.o capture.o
	$(QUIET_LD) $(CC) $(CFLAGS) $^ -o $@ $(EXTERNAL_LIB_DIRS) \
	$(EXTERNAL_LIBS)

-include play_file_at.d

play_file_at: play_file_at.o args.o stream.o mixer.o linked_list.o alsa.o \
	thread_signal.o stack.o capture.o
	$(QUIET_LD) $(CC) $(CFLAGS) $^ -o $@ $(EXTERNAL_LIB_DIRS) \
	$(EXTERNAL_LIBS)

-include record_file_at.d

record_file_at:	record_file_at.o args.o stream.o capture.o alsa.o \
	thread_signal.o linked_list.o mixer.o stack.o
	$(QUIET_LD) $(CC) $(CFLAGS) $^ -o $@ $(EXTERNAL_LIB_DIRS) \
	$(EXTERNAL_LIBS)

monoraw_to_net_time: monoraw_to_net_time.o args.o
	$(QUIET_LD) $(CC) $(CFLAGS) $^ -o $@ -lrt

net_time_to_monoraw: net_time_to_monoraw.o args.o
	$(QUIET_LD) $(CC) $(CFLAGS) $^ -o $@ -lrt

.PHONY: docs
docs:
	cd $(DOC_DIR); PROJECT_ROOT=$(ISAUDK_ROOT) SRC_DIR=$(ISAUDK_ROOT) \
	doxygen Doxyfile
	cd $(DOC_DIR)/latex; $(MAKE) pdf
	cp $(DOC_DIR)/latex/refman.pdf $(ISAUDK_ROOT)/documentation.pdf
