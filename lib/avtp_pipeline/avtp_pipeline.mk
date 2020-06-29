AVB_FEATURE_ENDPOINT ?= 1
IGB_LAUNCHTIME_ENABLED ?= 0
ATL_LAUNCHTIME_ENABLED ?= 1
AVB_FEATURE_GSTREAMER ?= 0
PLATFORM_TOOLCHAIN ?= x86_aqc_linux

.PHONY: all clean

all: build/Makefile
	$(MAKE) -s -C build install

doc: build/Makefile
	$(MAKE) -s -C build doc
	@echo "\n\nTo display documentation use:\n\n" \
	      "\txdg-open $(abspath build/documents/api_docs/index.html)\n"

clean:
	$(RM) -r build

build/Makefile:
	mkdir -p build && \
	cd build && \
	cmake -DCMAKE_BUILD_TYPE=Release \
	      -DCMAKE_TOOLCHAIN_FILE=../platform/Linux/$(PLATFORM_TOOLCHAIN).cmake \
	      -DAVB_FEATURE_ENDPOINT=$(AVB_FEATURE_ENDPOINT) \
	      -DIGB_LAUNCHTIME_ENABLED=$(IGB_LAUNCHTIME_ENABLED) \
	      -DATL_LAUNCHTIME_ENABLED=$(ATL_LAUNCHTIME_ENABLED) \
	      -DAVB_FEATURE_GSTREAMER=$(AVB_FEATURE_GSTREAMER) \
	      ..
