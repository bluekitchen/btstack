/******************************************************************************
 *
 *  Copyright 2022 Google LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#ifndef __LC3BIN_H
#define __LC3BIN_H

#include <stdio.h>
#include <stdint.h>
#include <lc3.h>


/**
 * Read LC3 binary header
 * fp              Opened file, moved after header on return
 * frame_us        Return frame duration, in us
 * srate_hz        Return samplerate, in Hz
 * nchannels       Return number of channels
 * nsamples        Return count of source samples by channels
 * return          0: Ok  -1: Bad LC3 File
 */
int lc3bin_read_header(FILE *fp,
    int *frame_us, int *srate_hz, int *nchannels, int *nsamples);

/**
 * Read LC3 block of data
 * fp              Opened file
 * nchannels       Number of channels
 * buffer          Output buffer of `nchannels * LC3_MAX_FRAME_BYTES`
 * return          Size of each 'nchannels` frames, -1 on error
 */
int lc3bin_read_data(FILE *fp, int nchannels, void *buffer);

/**
 * Write LC3 binary header
 * fp              Opened file, moved after header on return
 * frame_us        Frame duration, in us
 * srate_hz        Samplerate, in Hz
 * bitrate         Bitrate indication of the stream, in bps
 * nchannels       Number of channels
 * nsamples        Count of source samples by channels
 */
void lc3bin_write_header(FILE *fp,
    int frame_us, int srate_hz, int bitrate, int nchannels, int nsamples);

/**
 * Write LC3 block of data
 * fp              Opened file
 * data            The frames data
 * nchannels       Number of channels
 * frame_bytes     Size of each `nchannels` frames
 */
void lc3bin_write_data(FILE *fp,
    const void *data, int nchannels, int frame_bytes);


#endif /* __LC3BIN_H */
