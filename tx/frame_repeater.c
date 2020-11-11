/*
  frame_repeater.c
  David Rowe Nov 2020

  Sits between rtl_fsk and rpitx_fsk, to implement a pirip frame
  repeater, useful for unattended testing.  

  rtl_fsk | frame_repeater | rpitx 2>some sort of fifo for status
 
  + State machine gets "clocked" every time rtl_fsk sends a status byte.
  + rpitx_fsk communicates events via a linux fifo.
  + In hindsight not a great design, be better to put tx and rx in one TNC type application and
    control them together.  But good enough for testing the phsyical layer.

  TODO:
  [ ] listening to fifo, events from rpitx_fsk
  [ ] Show it can echo one frame
  [ ] Show it can echo multiple frames
  [ ] leave running for a few hours with no lockups
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "freedv_api_internal.h"

#define IDLE             0
#define RECEIVING_BURST  1
#define WAIT_FOR_TX_OFF  2
#define WAIT_FOR_RX_SYNC 3

#define MAX_FRAMES      100

int main(int argc, char *argv[]) {
    int state = IDLE;
    int next_state, bytes_in, bytes_out, ret;
    uint8_t burst_control, tx_off_event;

    if (argc < 3) {
        fprintf(stderr, "usage: %s dataBitsPerFrame rpitx_fskFifo\n", argv[0]);
        exit(1);
    }

    int data_bits_per_frame = atoi(argv[1]);
    assert((data_bits_per_frame % 8) == 0);
    int data_bytes_per_frame = data_bits_per_frame/8;
    fprintf(stderr, "frame_repeater: %d %d\n", data_bits_per_frame, data_bytes_per_frame);
    uint8_t data[data_bytes_per_frame];
    uint8_t data_buffer[data_bytes_per_frame*MAX_FRAMES];
    fprintf(stderr, "frame_repeater: opening FIFO\n");
    int rpitx_fsk_fifo = open(argv[2], O_RDONLY | O_NONBLOCK);
    if (rpitx_fsk_fifo == -1) {
        fprintf(stderr, "Error opening fifo %s\n", argv[2]);
        exit(1);
    }
    fprintf(stderr, "frame_repeater: FIFO opened OK ...\n");
    
    /* At regular intervals rtl_fsk sends a status byte then
       data_bytes_per_frame.  If there is no received data then
       data_buffer will be all zeros. */

    uint8_t rx_status, prev_rx_status = 0;
    while(fread(&rx_status, sizeof(uint8_t), 1, stdin)) {
        ret = fread(data, sizeof(uint8_t), data_bytes_per_frame, stdin);
        assert(ret == data_bytes_per_frame);
        tx_off_event = 0;
        
        // messages from rpitx_fsk in fifo create events
        //fprintf(stderr, "about to poll FIFO\n");
        char buf[256];
        ret = read(rpitx_fsk_fifo, buf, sizeof(buf));
        if (ret > 0) {
            fprintf(stderr, "frame_repeater: Read %d bytes from rpitx_fsk: %s\n", ret, buf);
            if (strcmp(buf,"Tx off") == 0) tx_off_event = 1;
        }
        
        if (prev_rx_status != rx_status)
            fprintf(stderr, "frame_repeater: state: %d rx_status: 0x%02x tx_off_event: %d\n", state, rx_status, tx_off_event);
        
        next_state = state;
        switch(state) {
        case IDLE:
            if (rx_status == (FREEDV_RX_SYNC | FREEDV_RX_BITS)) {
                fprintf(stderr, "frame_repeater: Starting to receive a burst\n");
                memcpy(data_buffer, data, data_bytes_per_frame);
                bytes_in = data_bytes_per_frame;
                next_state = RECEIVING_BURST;
            }
        break;
        case RECEIVING_BURST:
            if (rx_status & FREEDV_RX_BITS) {
                fprintf(stderr, "frame_repeater: Receiving a frame in a burst\n");
                // buffer latest packet
                memcpy(data_buffer, data, data_bytes_per_frame);
                bytes_in += data_bytes_per_frame;
                assert(bytes_in <= data_bytes_per_frame*MAX_FRAMES);
            }
            if (!(rx_status & FREEDV_RX_SYNC)) {
                fprintf(stderr, "frame_repeater: Sending received burst of %d bytes\n", bytes_in);
                /* We've lost RX_SYNC so receive burst finished.  So
                   lets Tx data we received.  fwrite's shouldn't block
                   due to size of stdout buffer */
                bytes_out = 0; burst_control = 1;
                while(bytes_out != bytes_in) {
                    fwrite(&burst_control, sizeof(uint8_t), 1, stdout);
                    fwrite(&data_buffer[bytes_out], sizeof(uint8_t), data_bytes_per_frame, stdout);
                    bytes_out += data_bytes_per_frame;
                    burst_control = 0;
                }
                burst_control = 2; memset(data, 0, data_bytes_per_frame);
                fwrite(&burst_control, sizeof(uint8_t), 1, stdout);
                fwrite(data, sizeof(uint8_t), data_bytes_per_frame, stdout);
                fflush(stdout);
                next_state = WAIT_FOR_TX_OFF;
            }
            break;
        case WAIT_FOR_TX_OFF:
            /* In this stage we ignore received bytes which will
               likely be the echo of frames being transmitted by the Tx */
            if (tx_off_event) {
                next_state = WAIT_FOR_RX_SYNC;
            }            
            break;
        case WAIT_FOR_RX_SYNC:
            /* We might have an incoming packet, the echo of what we
               just sent.  Wait for this to finish */
            if (!(rx_status & FREEDV_RX_SYNC)) next_state = IDLE;
            break;
        default:
            assert(0);
        }
        state = next_state;
        prev_rx_status = rx_status;
    }

    return 0;
}
