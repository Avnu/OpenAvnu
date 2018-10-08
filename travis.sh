#!/bin/bash
set -ev

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
