/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#ifndef WAV_UIL_H
#define WAV_UIL_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// return 0 if ok

/**
 * Open singleton wav writer
 * @return 0 if ok
 */
int wav_writer_open(const char * filepath, int num_channels, int sampling_frequency);
/**
 * Write Int8 samples
 * @return 0 if ok
 */
int wav_writer_write_int8(int num_samples, int8_t * data);
/**
 * Write Int16 samples (host endianess)
 * @return 0 if ok
 */
int wav_writer_write_int16(int num_samples, int16_t * data);
/**
 * Write Little Endian Int16 samples
 * @return 0 if ok
 */
int wav_writer_write_le_int16(int num_samples, int16_t * data);
/**
 * Close wav writer and update wav file header
 * @return 0 if ok
 */
int wav_writer_close(void);


/**
 * Open singleton wav reader
 * @return 0 if ok
 */
int wav_reader_open(const char * filepath);

/**
 * Get number of channels
 */
uint8_t wav_reader_get_num_channels(void);

/**
 * Get sampling rate
 */
uint32_t wav_reader_get_sampling_rate(void);

/**
 * Read Int8 samples
 * @return 0 if ok
 */
int wav_reader_read_int8(int num_samples, int8_t * data);
/**
 * Read Int16 samples (host endianess)
 * @return 0 if ok
 */
int wav_reader_read_int16(int num_samples, int16_t * data);
/**
 * Close war reader
 * @return 0 if ok
 */
int wav_reader_close(void);

#if defined __cplusplus
}
#endif

#endif // WAV_UTIL_H
