AVB_FEATURE_ENDPOINT ?= 1

.PHONY: all clean

all: build/Makefile
	$(MAKE) -s -C build install

clean:
	$(RM) -r build

build/Makefile:
	mkdir -p build && \
	cd build && \
	cmake -DCMAKE_TOOLCHAIN_FILE=../platform/Linux/x86_i210_linux.cmake \
	      -DAVB_FEATURE_ENDPOINT=$(AVB_FEATURE_ENDPOINT) \
              ..
