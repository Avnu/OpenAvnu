AVB_FEATURE_AVDECC ?= 1
PLATFORM_TOOLCHAIN ?= x86_i210_linux

.PHONY: all clean

all: build_avdecc/Makefile
	$(MAKE) -s -C build_avdecc install

doc: build_avdecc/Makefile
	$(MAKE) -s -C build_avdecc doc
	@echo "\n\nTo display documentation use:\n\n" \
	      "\txdg-open $(abspath build_avdecc/documents/api_docs/index.html)\n"

clean:
	$(RM) -r build_avdecc

build_avdecc/Makefile:
	mkdir -p build_avdecc && \
	cd build_avdecc && \
	cmake -DCMAKE_TOOLCHAIN_FILE=../platform/Linux/$(PLATFORM_TOOLCHAIN).cmake \
	      -DAVB_FEATURE_AVDECC=$(AVB_FEATURE_AVDECC) \
              ..

