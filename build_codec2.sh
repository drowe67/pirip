#!/bin/bash -x
# automate local build of codec2
git clone https://github.com/drowe67/codec2.git
cd codec2 && mkdir -p build_linux && cd build_linux
cmake ../ && make
