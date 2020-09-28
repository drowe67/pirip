# Makefile for fsk_rpitx, a fsk modulator for the Pi

all: fsk_rpitx

CODEC2_DIR=../codec2/build_linux/src
CXXFLAGS = -std=c++11 -Wall -g -O2 -Wno-unused-variable -I../codec2/src
LDFLAGS = ../librpitx/src/librpitx.a $(CODEC2_DIR)/libcodec2.so -Wl,-rpath=$(CODEC2_DIR) -lm -lrt -lpthread
CCP = c++
CC = cc

fsk_rpitx : fsk_rpitx.cpp ../librpitx/src/librpitx.a
	$(CCP) $(CXXFLAGS) -o fsk_rpitx fsk_rpitx.cpp  $(LDFLAGS)

