# Makefile for rpitx_fsk, a fsk modulator for the Pi

all: rpitx_fsk

CODEC2_DIR=../codec2/build_linux/src
CXXFLAGS = -std=c++11 -Wall -g -O2 -Wno-unused-variable -I../codec2/src
LDFLAGS = ../librpitx/src/librpitx.a $(CODEC2_DIR)/libcodec2.so -Wl,-rpath=$(CODEC2_DIR) -lm -lrt -lpthread
CCP = c++
CC = cc

rpitx_fsk : rpitx_fsk.cpp ../librpitx/src/librpitx.a
	$(CCP) $(CXXFLAGS) -o rpitx_fsk rpitx_fsk.cpp  $(LDFLAGS)

