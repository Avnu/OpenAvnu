#
#  Copyright (c) 2012 Intel Corporation
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#   1. Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#   2. Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#
#   3. Neither the name of the Intel Corporation nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

ALTERNATE_LINUX_INCPATH=$(HOME)/header/include/

CFLAGS_G = -Wall -g -I. -I../../common -I../src \
	-I$(ALTERNATE_LINUX_INCPATH)

# Check to see if PTP cross-timestamping is supported in hardware/driver
# and, if so, add flag to compile in support

PRECISE_TIME_TEST = "\#include <linux/ptp_clock.h>\\n\
\#ifdef PTP_SYS_OFFSET_PRECISE\\nint main(){return 0;}\\n\#endif"

HAS_PRECISE_TIME = $(shell echo $(PRECISE_TIME_TEST) | $(CC) -xc - \
-I$(ALTERNATE_LINUX_INCPATH) -o /dev/null > /dev/null 2>&1 ; echo $$?)

ifeq ($(HAS_PRECISE_TIME),0)
       CFLAGS_G += -DPTP_HW_CROSSTSTAMP
endif

CPPFLAGS_G = $(CFLAGS_G) -std=c++0x -Wnon-virtual-dtor
LDFLAGS_G = 

COMMON_DIR = ../../common
SRC_DIR = ../src
OBJ_DIR = obj

OBJ_FILES = $(OBJ_DIR)/ptp_message.o\
		 $(OBJ_DIR)/ap_message.o\
		 $(OBJ_DIR)/avbts_osnet.o\
		 $(OBJ_DIR)/ether_port.o\
		 $(OBJ_DIR)/common_port.o\
		 $(OBJ_DIR)/ieee1588clock.o \
		 $(OBJ_DIR)/linux_hal_common.o\
		 $(OBJ_DIR)/linux_hal_persist_file.o\
		 $(OBJ_DIR)/gptp_log.o\
		 $(OBJ_DIR)/platform.o \
		 $(OBJ_DIR)/ini.o \
		 $(OBJ_DIR)/gptp_cfg.o

HEADER_FILES = $(COMMON_DIR)/ether_port.hpp\
		$(COMMON_DIR)/common_port.hpp\
		$(COMMON_DIR)/avbts_ostimerq.hpp\
		$(COMMON_DIR)/avbts_ostimer.hpp\
		$(COMMON_DIR)/avbts_osthread.hpp\
		$(COMMON_DIR)/avbts_osnet.hpp\
		$(COMMON_DIR)/avbts_oslock.hpp\
		$(COMMON_DIR)/avbts_osipc.hpp\
		$(COMMON_DIR)/avbts_oscondition.hpp\
		$(COMMON_DIR)/avbts_message.hpp\
		$(COMMON_DIR)/avbts_clock.hpp\
		$(COMMON_DIR)/avbts_persist.hpp\
		$(COMMON_DIR)/avbap_message.hpp\
		$(COMMON_DIR)/ieee1588.hpp\
		$(COMMON_DIR)/ipcdef.hpp\
		$(COMMON_DIR)/ini.h\
		$(COMMON_DIR)/gptp_cfg.hpp\
		$(COMMON_DIR)/gptp_log.hpp\
		$(SRC_DIR)/linux_ipc.hpp\
		$(SRC_DIR)/linux_hal_common.hpp\
		$(SRC_DIR)/linux_hal_persist_file.hpp\
		$(SRC_DIR)/platform.hpp

ifeq ($(ARCH),I210)
	IGB_LIB_INCPATH=../../../../lib/igb/
	IGB_LIB_PATH=../../../../lib/igb/
	CFLAGS_G += -I$(IGB_LIB_INCPATH) -DWITH_IGBLIB
	LDFLAGS_G += -lz -ligb -lpci -L$(IGB_LIB_PATH)
	OBJ_FILES += $(OBJ_DIR)/linux_hal_generic.o \
		$(OBJ_DIR)/linux_hal_generic_adj.o
	OBJ_FILES += $(OBJ_DIR)/linux_hal_i210.o
	HEADER_FILES += $(SRC_DIR)/linux_hal_generic.hpp
else ifeq ($(ARCH),IntelCE)
	INTELCE_INCPATH=/home/hitesh/work/smd/Dinerout/i686-linux-elf/include/
	INTELCE_LINUX_INCPATH=/home/hitesh/work/smd/Dinerout/i686-linux-elf/include/linux_user
	INTELCE_LIBPATH=/home/hitesh/work/smd/Dinerout/i686-linux-elf/lib/
	CFLAGS_G += -I$(INTELCE_LINUX_INCPATH) -I$(INTELCE_INCPATH) -DARCH_INTELCE
	LDFLAGS_G += -L$(INTELCE_LIBPATH) -lismd_clock_ioctl -lismd_core_ioctl
	HEADER_FILES += $(SRC_DIR)/linux_hal_intelce.hpp
	OBJ_FILES += $(OBJ_DIR)/linux_hal_intelce.o
	CXX = $(TARGETCXX)
else
	OBJ_FILES += $(OBJ_DIR)/linux_hal_generic.o \
		$(OBJ_DIR)/linux_hal_generic_adj.o
	HEADER_FILES += $(SRC_DIR)/linux_hal_generic.hpp
endif

ifeq ($(GENIVI_DLT),1)
	GENIVI_DLT_INCLUDE_PATH=/usr/local/include/dlt/
	GENIVI_DLT_LIB_PATH=/usr/local/lib/x86_64-linux-gnu/static/
	CFLAGS_G += -I$(GENIVI_DLT_INCLUDE_PATH) -DGENIVI_DLT
	LDFLAGS_G += -ldlt -L$(GENIVI_DLT_LIB_PATH)
endif

ifeq ($(SYSTEMD_WATCHDOG),1)
	CFLAGS_G += -DSYSTEMD_WATCHDOG
	LDFLAGS_G += -lsystemd
	OBJ_FILES += $(OBJ_DIR)/watchdog.o
	HEADER_FILES += $(SRC_DIR)/watchdog.hpp
endif

LDFLAGS_G += -lpthread -lrt

CFLAGS = $(CFLAGS_G)
CPPFLAGS = $(CPPFLAGS_G)
LDFLAGS = $(LDFLAGS_G)

all: $(OBJ_DIR)/daemon_cl

rebuild: clean all

$(OBJ_DIR)/daemon_cl: $(SRC_DIR)/daemon_cl.cpp $(OBJ_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(OBJ_FILES) $(SRC_DIR)/daemon_cl.cpp -o $(OBJ_DIR)/daemon_cl $(LDFLAGS)

$(OBJ_DIR)/linux_hal_common.o: $(SRC_DIR)/linux_hal_common.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_common.cpp -o $(OBJ_DIR)/linux_hal_common.o

$(OBJ_DIR)/platform.o: $(SRC_DIR)/platform.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/platform.cpp -o $(OBJ_DIR)/platform.o

$(OBJ_DIR)/linux_hal_generic.o: $(SRC_DIR)/linux_hal_generic.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_generic.cpp -o $(OBJ_DIR)/linux_hal_generic.o

$(OBJ_DIR)/linux_hal_generic_adj.o: $(SRC_DIR)/linux_hal_generic_adj.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_generic_adj.cpp -o $(OBJ_DIR)/linux_hal_generic_adj.o

$(OBJ_DIR)/linux_hal_i210.o: $(SRC_DIR)/linux_hal_i210.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_i210.cpp -o $(OBJ_DIR)/linux_hal_i210.o

$(OBJ_DIR)/linux_hal_intelce.o: $(SRC_DIR)/linux_hal_intelce.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_intelce.cpp -o $(OBJ_DIR)/linux_hal_intelce.o

$(OBJ_DIR)/ether_port.o: $(COMMON_DIR)/ether_port.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/common_port.o: $(COMMON_DIR)/common_port.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/ieee1588clock.o: $(COMMON_DIR)/ieee1588clock.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/ieee1588clock.cpp -o $(OBJ_DIR)/ieee1588clock.o

$(OBJ_DIR)/ptp_message.o: $(COMMON_DIR)/ptp_message.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/ptp_message.cpp -o $(OBJ_DIR)/ptp_message.o

$(OBJ_DIR)/ap_message.o: $(COMMON_DIR)/ap_message.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/ap_message.cpp -o $(OBJ_DIR)/ap_message.o

$(OBJ_DIR)/avbts_osnet.o: $(COMMON_DIR)/avbts_osnet.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/avbts_osnet.cpp -o $(OBJ_DIR)/avbts_osnet.o

$(OBJ_DIR)/linux_hal_persist_file.o: $(SRC_DIR)/linux_hal_persist_file.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/linux_hal_persist_file.cpp -o $(OBJ_DIR)/linux_hal_persist_file.o

$(OBJ_DIR)/gptp_log.o: $(COMMON_DIR)/gptp_log.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/gptp_log.cpp -o $(OBJ_DIR)/gptp_log.o

$(OBJ_DIR)/gptp_cfg.o: $(COMMON_DIR)/gptp_cfg.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(COMMON_DIR)/gptp_cfg.cpp -o $(OBJ_DIR)/gptp_cfg.o

$(OBJ_DIR)/ini.o: $(COMMON_DIR)/ini.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $(COMMON_DIR)/ini.c -o $(OBJ_DIR)/ini.o

$(OBJ_DIR)/watchdog.o: $(SRC_DIR)/watchdog.cpp $(HEADER_FILES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $(SRC_DIR)/watchdog.cpp -o $(OBJ_DIR)/watchdog.o
clean:
	$(RM) *~ $(OBJ_DIR)/*.o  $(OBJ_DIR)/daemon_cl

