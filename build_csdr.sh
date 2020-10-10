#!/bin/bash -x
# Automate building of csdr library, which is used by rtl_fsk application
PIRIP_DIR=$(pwd)
git clone https://github.com/ha7ilm/csdr.git
cd csdr && make
