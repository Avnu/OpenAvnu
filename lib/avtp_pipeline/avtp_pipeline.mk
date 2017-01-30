AVB_FEATURE_ENDPOINT ?= 1
IGB_LAUNCHTIME_ENABLED ?= 0
AVB_FEATURE_GSTREAMER ?= 1

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
	cmake -DCMAKE_TOOLCHAIN_FILE=../platform/Linux/x86_i210_linux.cmake \
	      -DAVB_FEATURE_ENDPOINT=$(AVB_FEATURE_ENDPOINT) \
	      -DIGB_LAUNCHTIME_ENABLED=$(IGB_LAUNCHTIME_ENABLED) \
	      -DAVB_FEATURE_GSTREAMER=$(AVB_FEATURE_GSTREAMER) \
              ..
