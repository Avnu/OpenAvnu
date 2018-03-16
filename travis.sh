#!/bin/bash
set -ev

ROOT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

make igb
make lib
make daemons_all
make examples_all
make avtp_pipeline
make avtp_avdecc
mkdir build
cd build
cmake .. -G "Unix Makefiles"
make
export ARGS=--output-on-failure
make test
cd ../daemons/gptp/doc
mkdir build
cd build
cmake ..
make doc
cd $ROOT_DIR
CFLAGS=-Wno-missing-braces meson lib/libavtp/ lib/libavtp/build
ninja -C lib/libavtp/build/ test aaf-talker aaf-listener
