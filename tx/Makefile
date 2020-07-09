# Makefile for fsk_rpitx, a fsk modulator for the Pi

all: fsk_rpitx

CFLAGS	= -Wall -g -O2 -Wno-unused-variable
CXXFLAGS = -std=c++11 -Wall -g -O2 -Wno-unused-variable
LDFLAGS = ../librpitx/src/librpitx.a -lm -lrt -lpthread
CCP = c++
CC = cc

fsk_rpitx : fsk_rpitx.cpp ../librpitx/src/librpitx.a
	$(CCP) $(CXXFLAGS) -o fsk_rpitx fsk_rpitx.cpp  $(LDFLAGS)

