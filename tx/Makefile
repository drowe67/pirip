# Makefile for rpitx_fsk, a fsk modulator for the Pi

all: rpitx_fsk frame_repeater

CODEC2_DIR=$(HOME)/pirip/codec2/build_linux/src
LIBRTLSDR_DIR=$(HOME)/pirip/librpitx/src/
CXXFLAGS = -std=c++11 -Wall -g -O2 -Wno-unused-variable -I$(HOME)/pirip/codec2/src
CFLAGS = -Wall -g -O2 -Wno-unused-variable -I$(HOME)/pirip/codec2/src
LDFLAGS = $(LIBRTLSDR_DIR)/librpitx.a $(CODEC2_DIR)/libcodec2.so -Wl,-rpath=$(CODEC2_DIR) -lm -lrt -lpthread
CCP = c++
CC = cc

rpitx_fsk : rpitx_fsk.cpp $(LIBRTLSDR_DIR)/librpitx.a
	$(CCP) $(CXXFLAGS) -o rpitx_fsk rpitx_fsk.cpp  $(LDFLAGS)

frame_repeater : frame_repeater.c
	$(CC) $(CFLAGS) -o $@ $^


