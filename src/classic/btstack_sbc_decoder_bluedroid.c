/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "btstack_sbc_decoder_bluedroid.c"

// *****************************************************************************
//
// SBC decoder based on Bluedroid library 
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>

#include <stdlib.h>
#include <string.h>

#ifdef LOG_FRAME_STATUS
#include <stdio.h>
#endif

#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_sbc.h"
#include "btstack_sbc_plc.h"

#include "oi_codec_sbc.h"
#include "oi_assert.h"

#define mSBC_SYNCWORD 0xad
#define SBC_SYNCWORD 0x9c
#define SBC_MAX_CHANNELS 2
// #define LOG_FRAME_STATUS

#define DECODER_DATA_SIZE (SBC_MAX_CHANNELS*SBC_MAX_BLOCKS*SBC_MAX_BANDS * 4 + SBC_CODEC_MIN_FILTER_BUFFERS*SBC_MAX_BANDS*SBC_MAX_CHANNELS * 2)

typedef struct {
    OI_UINT32 bytes_in_frame_buffer;
    OI_CODEC_SBC_DECODER_CONTEXT decoder_context;

    uint8_t   frame_buffer[SBC_MAX_FRAME_LEN];
    int16_t   pcm_plc_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    int16_t   pcm_data[SBC_MAX_CHANNELS * SBC_MAX_BANDS * SBC_MAX_BLOCKS];
    uint32_t  pcm_bytes;
    OI_UINT32 decoder_data[(DECODER_DATA_SIZE+3)/4];
    int       first_good_frame_found;
    int       h2_sequence_nr;
    uint16_t  msbc_bad_bytes;
} bludroid_decoder_state_t;

static btstack_sbc_decoder_state_t * sbc_decoder_state_singleton = NULL;
static bludroid_decoder_state_t bd_decoder_state;

// Testing only - START
static int plc_enabled = 1;
static int corrupt_frame_period = -1;
// Testing - STOP

void btstack_sbc_decoder_test_set_plc_enabled(int enabled){
    plc_enabled = enabled;
}

void btstack_sbc_decoder_test_simulate_corrupt_frames(int period){
    corrupt_frame_period = period;
}

static int find_sequence_of_zeros(const OI_BYTE *frame_data, OI_UINT32 frame_bytes, int seq_length){
    int zero_seq_count = 0;
    unsigned int i;
    for (i=0; i<frame_bytes; i++){
        if (frame_data[i] == 0) {
            zero_seq_count++;
            if (zero_seq_count >= seq_length) return zero_seq_count;
        } else {
            zero_seq_count = 0;
        }
    }
    return 0;
}

// returns position of mSBC sync word
static int find_h2_sync(const OI_BYTE *frame_data, OI_UINT32 frame_bytes, int * sync_word_nr){
    int syncword = mSBC_SYNCWORD;
    uint8_t h2_first_byte = 0;
    uint8_t h2_second_byte = 0;

    unsigned int i;
    for (i=0; i<frame_bytes; i++){
        if (frame_data[i] == syncword) {
            // check: first byte == 1
            if (h2_first_byte == 1) {
                // check lower nibble of second byte == 0x08
                uint8_t ln = h2_second_byte & 0x0F;
                if (ln == 8) {
                    // check if bits 0+2 == bits 1+3
                    uint8_t hn = h2_second_byte >> 4;
                    if  ( ((hn>>1) & 0x05) == (hn & 0x05) ) {
                        *sync_word_nr = ((hn & 0x04) >> 1) | (hn & 0x01);
                        return i;
                    }
                }
            }
        }
        h2_first_byte = h2_second_byte;
        h2_second_byte = frame_data[i];
    }
    return -1;
}

int btstack_sbc_decoder_num_samples_per_frame(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_blocks * decoder_state->decoder_context.common.frameInfo.nrof_subbands;
}

int btstack_sbc_decoder_num_channels(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.nrof_channels;
}

int btstack_sbc_decoder_sample_rate(btstack_sbc_decoder_state_t * state){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t *) state->decoder_state;
    return decoder_state->decoder_context.common.frameInfo.frequency;
}

#ifdef OI_DEBUG
void OI_AssertFail(const char* file, int line, const char* reason){
    log_error("AssertFail file %s, line %d, reason %s", file, line, reason);
}
#endif

void btstack_sbc_decoder_init(btstack_sbc_decoder_state_t * state, btstack_sbc_mode_t mode, void (*callback)(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context), void * context){
    if (sbc_decoder_state_singleton && (sbc_decoder_state_singleton != state) ){
        log_error("SBC decoder: different sbc decoder state already registered");
    }
    OI_STATUS status = OI_STATUS_SUCCESS;
    switch (mode){
        case SBC_MODE_STANDARD:
            // note: we always request stereo output, even for mono input
            status = OI_CODEC_SBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data), 2, 2, FALSE);
            break;
        case SBC_MODE_mSBC:
            status = OI_CODEC_mSBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data));
            break;
        default:
            break;
    }

    if (status != OI_STATUS_SUCCESS){
        log_error("SBC decoder: error during reset %d\n", status);
    }

    sbc_decoder_state_singleton = state;

    bd_decoder_state.bytes_in_frame_buffer = 0;
    bd_decoder_state.pcm_bytes = sizeof(bd_decoder_state.pcm_data);
    bd_decoder_state.h2_sequence_nr = -1;
    bd_decoder_state.first_good_frame_found = 0;

    memset(state, 0, sizeof(btstack_sbc_decoder_state_t));
    state->handle_pcm_data = callback;
    state->mode = mode;
    state->context = context;
    state->decoder_state = &bd_decoder_state;
    btstack_sbc_plc_init(&state->plc_state);
}

static void append_received_sbc_data(bludroid_decoder_state_t * state, const uint8_t * buffer, int size){
    int numFreeBytes = sizeof(state->frame_buffer) - state->bytes_in_frame_buffer;

    if (size > numFreeBytes){
        log_error("SBC data: more bytes read %u than free bytes in buffer %u", size, numFreeBytes);
    }

    (void)memcpy(state->frame_buffer + state->bytes_in_frame_buffer, buffer,
                 size);
    state->bytes_in_frame_buffer += size;
}

static void btstack_sbc_decoder_bluedroid_simulate_error(const OI_BYTE *frame_data) {
    static int frame_count = 0;
    if (corrupt_frame_period > 0){
        frame_count++;

        if ((frame_count % corrupt_frame_period) == 0){
            *(uint8_t*)&frame_data[5] = 0;
            frame_count = 0;
        }
    }
}

static void btstack_sbc_decoder_process_sbc_data(btstack_sbc_decoder_state_t * state, const uint8_t * buffer, int size){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
    int input_bytes_to_process = size;
    int keep_decoding = 1;

    while (keep_decoding) {
        // Fill decoder_state->frame_buffer as much as possible.
        int bytes_free_in_frame_buffer = SBC_MAX_FRAME_LEN - decoder_state->bytes_in_frame_buffer;
        int bytes_to_append = btstack_min(input_bytes_to_process, bytes_free_in_frame_buffer);
        if (bytes_to_append){
            append_received_sbc_data(decoder_state, buffer, bytes_to_append);
            buffer += bytes_to_append;
            input_bytes_to_process -= bytes_to_append;
        }

        // Decode the next frame in decoder_state->frame_buffer.
        int bytes_in_frame_buffer_before_decoding = decoder_state->bytes_in_frame_buffer;
        const OI_BYTE *frame_data = decoder_state->frame_buffer;
        OI_UINT32 frame_data_len = decoder_state->bytes_in_frame_buffer;
        OI_STATUS status = OI_CODEC_SBC_DecodeFrame(&(decoder_state->decoder_context),
                                                    &frame_data,
                                                    &frame_data_len,
                                                    decoder_state->pcm_plc_data,
                                                    &(decoder_state->pcm_bytes));
        uint16_t bytes_processed = bytes_in_frame_buffer_before_decoding - frame_data_len;

        // testing only - corrupt frame periodically
        btstack_sbc_decoder_bluedroid_simulate_error(frame_data);

        // Handle decoding result.
        switch(status){
            case OI_STATUS_SUCCESS:
            case OI_CODEC_SBC_PARTIAL_DECODE:
                state->handle_pcm_data(decoder_state->pcm_plc_data,
                                       btstack_sbc_decoder_num_samples_per_frame(state),
                                       btstack_sbc_decoder_num_channels(state),
                                       btstack_sbc_decoder_sample_rate(state), state->context);
                state->good_frames_nr++;
                break;

            case OI_CODEC_SBC_NOT_ENOUGH_HEADER_DATA:
            case OI_CODEC_SBC_NOT_ENOUGH_BODY_DATA:
            case OI_CODEC_SBC_NOT_ENOUGH_AUDIO_DATA:
                if (input_bytes_to_process > 0){
                    // Should never occur: The SBC codec claims there is not enough bytes in the frame_buffer,
                    // but the frame_buffer was full. (The frame_buffer is always full before decoding when input_bytes_to_process > 0.)
                    // Clear frame_buffer.
                    log_info("SBC decode: frame_buffer too small for frame");
                    bytes_processed = bytes_in_frame_buffer_before_decoding;
                } else {
                    // Exit decode loop, because there is not enough data in frame_buffer to decode the next frame.
                    keep_decoding = 0;
                }
                break;

            case OI_CODEC_SBC_NO_SYNCWORD:
                // This means the entire frame_buffer did not contain the syncword.
                // Discard the frame_buffer contents.
                log_info("SBC decode: no syncword found");
                bytes_processed = bytes_in_frame_buffer_before_decoding;
                break;

            case OI_CODEC_SBC_CHECKSUM_MISMATCH:
                // The next frame is somehow corrupt.
                log_info("SBC decode: checksum error");
                // Did the codec consume any bytes?
                if (bytes_processed > 0){
                    // Good. Nothing to do.
                } else {
                    // Skip the bogus frame by skipping the header.
                    bytes_processed = 1;
                }
                break;

            case OI_STATUS_INVALID_PARAMETERS:
                // This caused by corrupt frames.
                // The codec apparently does not recover from this.
                // Re-initialize the codec.
                log_info("SBC decode: invalid parameters: resetting codec");
                if (OI_CODEC_SBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data), 2, 2, FALSE) != OI_STATUS_SUCCESS){
                    log_info("SBC decode: resetting codec failed");

                }
                break;
            default:
                // Anything else went wrong. 
                // Skip a few bytes and try again.
                bytes_processed = 1;
                log_info("SBC decode: unknown status %d", status);
                break;
        }

        // Remove decoded frame from decoder_state->frame_buffer.
        if (bytes_processed > bytes_in_frame_buffer_before_decoding) {
            bytes_processed = bytes_in_frame_buffer_before_decoding;
        }
        memmove(decoder_state->frame_buffer, decoder_state->frame_buffer + bytes_processed, bytes_in_frame_buffer_before_decoding - bytes_processed);
        decoder_state->bytes_in_frame_buffer -= bytes_processed;
    }
}


static void btstack_sbc_decoder_insert_missing_frames(btstack_sbc_decoder_state_t *state) {
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
    const unsigned int MSBC_FRAME_SIZE = 60;

    while (decoder_state->first_good_frame_found && (decoder_state->msbc_bad_bytes >= MSBC_FRAME_SIZE)){

        decoder_state->msbc_bad_bytes -= MSBC_FRAME_SIZE;
        state->bad_frames_nr++;

        // prepare zero signal frame
        const OI_BYTE * frame_data  = btstack_sbc_plc_zero_signal_frame();
        OI_UINT32 bytes_in_frame_buffer = 57;

        // log_info("Trace bad frame generator, bad bytes %u", decoder_state->msbc_bad_bytes);
        OI_STATUS status = status = OI_CODEC_SBC_DecodeFrame(&(decoder_state->decoder_context),
                                            &frame_data,
                                            &bytes_in_frame_buffer,
                                            decoder_state->pcm_plc_data,
                                            &(decoder_state->pcm_bytes));

        if (status) {
            log_error("SBC decoder for ZIR frame: error %d\n", status);
        }

        if (bytes_in_frame_buffer){
            log_error("PLC: not all bytes of zero frame processed, left %u\n", (unsigned int) bytes_in_frame_buffer);
        }

        if (plc_enabled) {
            btstack_sbc_plc_bad_frame(&state->plc_state, decoder_state->pcm_plc_data, decoder_state->pcm_data);
        } else {
            (void)memcpy(decoder_state->pcm_data,
                         decoder_state->pcm_plc_data,
                         decoder_state->pcm_bytes);
        }

        state->handle_pcm_data(decoder_state->pcm_data,
                            btstack_sbc_decoder_num_samples_per_frame(state),
                            btstack_sbc_decoder_num_channels(state),
                            btstack_sbc_decoder_sample_rate(state), state->context);
    }
}

static void btstack_sbc_decoder_drop_processed_bytes(bludroid_decoder_state_t * decoder_state, uint16_t bytes_processed){
    memmove(decoder_state->frame_buffer, decoder_state->frame_buffer + bytes_processed, decoder_state->bytes_in_frame_buffer-bytes_processed);
    decoder_state->bytes_in_frame_buffer -= bytes_processed;
}

static void btstack_sbc_decoder_process_msbc_data(btstack_sbc_decoder_state_t * state, int packet_status_flag, const uint8_t * buffer, int size){
    bludroid_decoder_state_t * decoder_state = (bludroid_decoder_state_t*)state->decoder_state;
    int input_bytes_to_process = size;
    const unsigned int MSBC_FRAME_SIZE = 60;

    while (input_bytes_to_process > 0){

        // Use PLC to insert missing frames (after first sync found)
        btstack_sbc_decoder_insert_missing_frames(state);
        // fill buffer with new data
        int bytes_missing_for_complete_msbc_frame = MSBC_FRAME_SIZE - decoder_state->bytes_in_frame_buffer;
        int bytes_to_append = btstack_min(input_bytes_to_process, bytes_missing_for_complete_msbc_frame);
        if (bytes_to_append) {
            append_received_sbc_data(decoder_state, buffer, bytes_to_append);
            buffer += bytes_to_append;
            input_bytes_to_process -= bytes_to_append;
        }
        // complete frame in  buffer?
        if (decoder_state->bytes_in_frame_buffer < MSBC_FRAME_SIZE) break;

        uint16_t bytes_in_frame_buffer_before_decoding = decoder_state->bytes_in_frame_buffer;
        uint16_t bytes_processed = 0;
        const OI_BYTE *frame_data = decoder_state->frame_buffer;

        // testing only - corrupt frame periodically
        btstack_sbc_decoder_bluedroid_simulate_error(frame_data);

        // assert frame looks like this: 01 x8 AD [rest of frame 56 bytes] 00
        int h2_syncword = 0;
        int h2_sync_pos = find_h2_sync(frame_data, decoder_state->bytes_in_frame_buffer, &h2_syncword);
        if (h2_sync_pos < 0){
            // no sync found, discard all but last 2 bytes
            bytes_processed = decoder_state->bytes_in_frame_buffer - 2;
            btstack_sbc_decoder_drop_processed_bytes(decoder_state, bytes_processed);
            // don't try PLC without at least a single good frame
            if (decoder_state->first_good_frame_found){
                decoder_state->msbc_bad_bytes += bytes_processed;
            }
            continue;
        }

        decoder_state->h2_sequence_nr = h2_syncword;

        // drop data before it
        bytes_processed = h2_sync_pos - 2;
        if (bytes_processed > 0){
            memmove(decoder_state->frame_buffer, decoder_state->frame_buffer + bytes_processed, decoder_state->bytes_in_frame_buffer-bytes_processed);
            decoder_state->bytes_in_frame_buffer -= bytes_processed;
            // don't try PLC without at least a single good frame
            if (decoder_state->first_good_frame_found){
                decoder_state->msbc_bad_bytes += bytes_processed;
            }
            continue;
        }

        int bad_frame = 0;
        int zero_seq_found = find_sequence_of_zeros(frame_data, decoder_state->bytes_in_frame_buffer, 20);

        // after first valid frame, zero sequences count as bad frames
        if (decoder_state->first_good_frame_found){
            bad_frame = zero_seq_found || packet_status_flag;
        }

        if (bad_frame){
            // stats
            if (zero_seq_found){
                state->zero_frames_nr++;
            } else {
                state->bad_frames_nr++;
            }
#ifdef LOG_FRAME_STATUS 
            if (zero_seq_found){
                printf("%d : ZERO FRAME\n", decoder_state->h2_sequence_nr);
            } else {
                printf("%d : BAD FRAME\n", decoder_state->h2_sequence_nr);
            }
#endif
            // retry after dropping 3 byte sync
            bytes_processed = 3;
            btstack_sbc_decoder_drop_processed_bytes(decoder_state, bytes_processed);
            decoder_state->msbc_bad_bytes += bytes_processed;
            // log_info("Trace bad frame");
            continue;
        }

        //ready to decode frame
        OI_STATUS status = OI_CODEC_SBC_DecodeFrame(&(decoder_state->decoder_context),
                                            &frame_data,
                                            &(decoder_state->bytes_in_frame_buffer),
                                            decoder_state->pcm_plc_data,
                                            &(decoder_state->pcm_bytes));

        bytes_processed = bytes_in_frame_buffer_before_decoding - decoder_state->bytes_in_frame_buffer;
        // log_info("Trace decode status %u, processed %u (bad bytes %u), bytes in buffer %u", (int) status, bytes_processed, decoder_state->msbc_bad_bytes, decoder_state->bytes_in_frame_buffer);

        switch(status){
            case 0:
                // synced
                decoder_state->first_good_frame_found = 1;

                // get rid of padding byte, not processed by SBC decoder
                decoder_state->bytes_in_frame_buffer = 0;

                // restart counting bad bytes
                decoder_state->msbc_bad_bytes = 0;

                // feed good frame into PLC history
                btstack_sbc_plc_good_frame(&state->plc_state, decoder_state->pcm_plc_data, decoder_state->pcm_data);

                // deliver PCM data
                state->handle_pcm_data(decoder_state->pcm_data,
                                    btstack_sbc_decoder_num_samples_per_frame(state),
                                    btstack_sbc_decoder_num_channels(state),
                                    btstack_sbc_decoder_sample_rate(state), state->context);

                // stats
                state->good_frames_nr++;
                continue;

            case OI_CODEC_SBC_CHECKSUM_MISMATCH:
                // The next frame is somehow corrupt.
                log_debug("OI_CODEC_SBC_CHECKSUM_MISMATCH");
                // Did the codec consume any bytes?
                if (bytes_processed > 0){
                    // Good. Nothing to do.
                } else {
                    // Skip the bogus frame by skipping the header.
                    bytes_processed = 1;
                }
                break;

            case OI_STATUS_INVALID_PARAMETERS:
                // This caused by corrupt frames.
                // The codec apparently does not recover from this.
                // Re-initialize the codec.
                log_info("SBC decode: invalid parameters: resetting codec");
                if (OI_CODEC_mSBC_DecoderReset(&(bd_decoder_state.decoder_context), bd_decoder_state.decoder_data, sizeof(bd_decoder_state.decoder_data)) != OI_STATUS_SUCCESS){
                    log_info("SBC decode: resetting codec failed");
                }
                break;
            default:
                log_info("Frame decode error: %d", status);
                break;
        }

        // on success, while loop was restarted, so all processed bytes have been "bad"
        decoder_state->msbc_bad_bytes += bytes_processed;

        // drop processed bytes from frame buffer
        btstack_sbc_decoder_drop_processed_bytes(decoder_state, bytes_processed);
    }
}

void btstack_sbc_decoder_process_data(btstack_sbc_decoder_state_t * state, int packet_status_flag, const uint8_t * buffer, int size){
    if (state->mode == SBC_MODE_mSBC){
        btstack_sbc_decoder_process_msbc_data(state, packet_status_flag, buffer, size);
    } else {
        btstack_sbc_decoder_process_sbc_data(state, buffer, size);
    }
}
