#
#  Copyright (c) 2015 Coveloz Consulting
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
#   3. Neither the name of the Coveloz Consulting nor the names of its
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

COMMON_DIR = ../../common
TARGET_NAME := shm_test

CFLAGS_G = -Wall -g -Wnon-virtual-dtor -I. -I$(COMMON_DIR)
LDFLAGS_G = -lpthread -lrt

OBJ_FILES =
HEADER_FILES = $(COMMON_DIR)/ipcdef.hpp

CFLAGS = $(CFLAGS_G)
LDFLAGS = $(LDFLAGS_G)

all: $(TARGET_NAME)

$(TARGET_NAME): shm_test.cpp
	# Generating $@
	@ $(CXX) $(CFLAGS) $(CXXFLAGS) $(OBJ_FILES) shm_test.cpp -o $(TARGET_NAME) $(LDFLAGS)

clean:
	# Cleaning up
	@ $(RM) *.o  $(TARGET_NAME)

