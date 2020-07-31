/*
 * Copyright (C) 2018 BlueKitchen GmbH
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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "btstack_resample.c"

#include "btstack_bool.h"
#include "btstack_resample.h"

void btstack_resample_init(btstack_resample_t * context, int num_channels){
    context->src_pos = 0;
    context->src_step = 0x10000;  // default resampling 1.0
    context->last_sample[0] = 0;
    context->last_sample[1] = 0;
    context->num_channels   = num_channels;
}

void btstack_resample_set_factor(btstack_resample_t * context, uint32_t src_step){
    context->src_step = src_step;
}

uint16_t btstack_resample_block(btstack_resample_t * context, const int16_t * input_buffer, uint32_t num_frames, int16_t * output_buffer){
    uint16_t dest_frames = 0;
    uint16_t dest_samples = 0;
    // samples between last sample of previous block and first sample in current block 
    while (context->src_pos >= 0xffff0000){
        const uint16_t t = context->src_pos & 0xffffu;
        int i;
        for (i=0;i<context->num_channels;i++){
            int s1 = context->last_sample[i];
            int s2 = input_buffer[i];
            int os = ((s1*(0x10000u - t)) + (s2*t)) >> 16u;
            output_buffer[dest_samples++] = os;
        }
        dest_frames++;
        context->src_pos += context->src_step;
    }
    // process current block
    while (true){
        const uint16_t src_pos = context->src_pos >> 16;
        const uint16_t t       = context->src_pos & 0xffffu;
        int index = src_pos * context->num_channels;
        int i;
        if (src_pos >= (num_frames - 1u)){
            // store last sample
            for (i=0;i<context->num_channels;i++){
                context->last_sample[i] = input_buffer[index++];
            }
            // samples processed
            context->src_pos -= num_frames << 16;
            break;
        }
        for (i=0;i<context->num_channels;i++){
            int s1 = input_buffer[index];
            int s2 = input_buffer[index+context->num_channels];
            int os = ((s1*(0x10000u - t)) + (s2*t)) >> 16u;
            output_buffer[dest_samples++] = os;
            index++;
        }
        dest_frames++;
        context->src_pos += context->src_step;
    }
    return dest_frames;
}
