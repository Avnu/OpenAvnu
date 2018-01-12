AVB_FEATURE_AVDECC ?= 1
PLATFORM_TOOLCHAIN ?= generic

.PHONY: all clean

all: build_avdecc/Makefile
	$(MAKE) -s -C build_avdecc install
	mkdir -p build/bin
	cp build_avdecc/bin/* build/bin/.

doc: build_avdecc/Makefile
	$(MAKE) -s -C build_avdecc doc
	@echo "\n\nTo display documentation use:\n\n" \
	      "\txdg-open $(abspath build_avdecc/documents/api_docs/index.html)\n"

clean:
	$(RM) -r build_avdecc

build_avdecc/Makefile:
	mkdir -p build_avdecc && \
	cd build_avdecc && \
	cmake -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_TOOLCHAIN_FILE=../platform/Linux/$(PLATFORM_TOOLCHAIN).cmake \
	      -DAVB_FEATURE_AVDECC=$(AVB_FEATURE_AVDECC) \
	      ..

