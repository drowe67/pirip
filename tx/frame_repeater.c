/*
  frame_repeater.c
  David Rowe Nov 2020

  Sits between rtl_fsk and rpitx_fsk, to implement a pirip frame
  repeater, useful for unattended testing.  Allows us to "ping" a
  terminal.

  rtl_fsk | frame_repeater | rpitx_fsk
 
  + State machine gets "clocked" every time rtl_fsk sends a status byte.
  + Bursts are accumulated until complete, then sent to rpitx_fsk for re-transmission
  + we wait for the estimated time of the Tx burst, ignoring any frames received during the Tx burst,
    as our sensitive RX will be picking up what we are sending

  TODO:
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
#define MAX_FRAMES      100

int main(int argc, char *argv[]) {
    int state = IDLE;
    int next_state, bytes_in, bytes_out, nframes, ret, nclocks;
    uint8_t burst_control, source_byte;

    if (argc < 3) {
        fprintf(stderr, "usage: %s dataBitsPerFrame sourceByte\n", argv[0]);
        exit(1);
    }

    int data_bits_per_frame = atoi(argv[1]);
    assert((data_bits_per_frame % 8) == 0);
    int data_bytes_per_frame = data_bits_per_frame/8;
    fprintf(stderr, "frame_repeater: %d %d\n", data_bits_per_frame, data_bytes_per_frame);
    uint8_t data[data_bytes_per_frame];
    uint8_t data_buffer[data_bytes_per_frame*MAX_FRAMES];

    source_byte = strtol(argv[2], NULL, 0);
        
    /* At regular intervals rtl_fsk sends a status byte then
       data_bytes_per_frame.  If there is no received data then
       data_buffer will be all zeros. */

    uint8_t rx_status, prev_rx_status = 0; int time_ms; nclocks = 0; int wait_until_ms;
    while(fread(&rx_status, sizeof(uint8_t), 1, stdin)) {
        ret = fread(data, sizeof(uint8_t), data_bytes_per_frame, stdin);
        assert(ret == data_bytes_per_frame);
        nclocks++;
        
        /* update rate from rtl_fsk can be quite fast, so only fprintf status updates when the rx_status
           changes */
        
        next_state = state;
        switch(state) {
        case IDLE:
            if (rx_status == (FREEDV_RX_SYNC | FREEDV_RX_BITS)) {
                fprintf(stderr, "frame_repeater: Starting to receive a burst\n");
                memcpy(data_buffer, data, data_bytes_per_frame);
                bytes_in = data_bytes_per_frame;
                nframes = 1;
                next_state = RECEIVING_BURST;
            }
        break;
        case RECEIVING_BURST:
            if (rx_status & FREEDV_RX_BITS) {
                fprintf(stderr, "frame_repeater: Receiving a frame in a burst\n");
                // buffer latest packet
                memcpy(&data_buffer[bytes_in], data, data_bytes_per_frame);
                bytes_in += data_bytes_per_frame;
                nframes++;
                assert(bytes_in <= data_bytes_per_frame*MAX_FRAMES);
            }
            if (!(rx_status & FREEDV_RX_SYNC)) {
                /* We've lost RX_SYNC so receive burst finished.  So now we can Tx the frames we have buffered */
                fprintf(stderr, "frame_repeater: Sending received %d frame burst of %d bytes\n", nframes, bytes_in);
                bytes_out = 0; burst_control = 1;
                while(bytes_out != bytes_in) {
                    fwrite(&burst_control, sizeof(uint8_t), 1, stdout);
                    /* change source address so our local receiver can filter these packets out */
                    data_buffer[bytes_out] = source_byte;
                    fwrite(&data_buffer[bytes_out], sizeof(uint8_t), data_bytes_per_frame, stdout);
                    bytes_out += data_bytes_per_frame;
                    burst_control = 0;
                }
                burst_control = 2; memset(data, 0, data_bytes_per_frame);
                fwrite(&burst_control, sizeof(uint8_t), 1, stdout);
                fwrite(data, sizeof(uint8_t), data_bytes_per_frame, stdout);
                fflush(stdout);
                next_state = IDLE;
            }
            break;
        default:
            assert(0);
        }
        /* just log ouput when state changes to avoid to omuch noise */
        if ((state != next_state) || (rx_status & FREEDV_RX_BITS))
            fprintf(stderr, "frame_repeater: [%d] state %d next_state: %d rx_status: 0x%02x\n", nclocks, state, next_state, rx_status);
       state = next_state;
    }

    return 0;
}
