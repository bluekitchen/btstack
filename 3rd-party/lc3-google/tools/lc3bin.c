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

#include <stdint.h>
#include "lc3bin.h"


/**
 * LC3 binary header
 */

#define LC3_FILE_ID (0x1C | (0xCC << 8))

struct lc3bin_header {
    uint16_t file_id;
    uint16_t header_size;
    uint16_t srate_100hz;
    uint16_t bitrate_100bps;
    uint16_t channels;
    uint16_t frame_10us;
    uint16_t epmode;
    uint16_t nsamples_low;
    uint16_t nsamples_high;
};


/**
 * Read LC3 binary header
 */
int lc3bin_read_header(FILE *fp,
    int *frame_us, int *srate_hz, bool *hrmode, int *nchannels, int *nsamples)
{
    struct lc3bin_header hdr;
    uint16_t hdr_hrmode = 0;

    if (fread(&hdr, sizeof(hdr), 1, fp) != 1
            || hdr.file_id != LC3_FILE_ID
            || hdr.header_size < sizeof(hdr))
        return -1;

    int num_extended_params = (hdr.header_size - sizeof(hdr)) / sizeof(uint16_t);
    if (num_extended_params >= 1 &&
        fread(&hdr_hrmode, sizeof(hdr_hrmode), 1, fp) != 1)
      return -1;

    *nchannels = hdr.channels;
    *frame_us = hdr.frame_10us * 10;
    *srate_hz = hdr.srate_100hz * 100;
    *nsamples = hdr.nsamples_low | (hdr.nsamples_high << 16);
    *hrmode = hdr_hrmode != 0;

    if (hdr.epmode)
      return -1;

    fseek(fp, hdr.header_size, SEEK_SET);

    return 0;
}

/**
 * Read LC3 block of data
 */
int lc3bin_read_data(FILE *fp, int nchannels, void *buffer)
{
    uint16_t nbytes;

    if (fread(&nbytes, sizeof(nbytes), 1, fp) < 1
            || nbytes > nchannels * LC3_HR_MAX_FRAME_BYTES
            || fread(buffer, nbytes, 1, fp) < 1)
        return -1;

    return nbytes;
}

/**
 * Write LC3 binary header
 */
void lc3bin_write_header(FILE *fp,
    int frame_us, int srate_hz, bool hrmode,
    int bitrate, int nchannels, int nsamples)
{
    uint16_t hdr_hrmode = (hrmode != 0);

    struct lc3bin_header hdr = {
        .file_id = LC3_FILE_ID,
        .header_size = sizeof(struct lc3bin_header) +
            (hrmode ? sizeof(hdr_hrmode) : 0),
        .srate_100hz = srate_hz / 100,
        .bitrate_100bps = bitrate / 100,
        .channels = nchannels,
        .frame_10us = frame_us / 10,
        .nsamples_low = nsamples & 0xffff,
        .nsamples_high = nsamples >> 16,
    };

    fwrite(&hdr, sizeof(hdr), 1, fp);

    if (hrmode)
        fwrite(&hdr_hrmode, sizeof(hdr_hrmode), 1, fp);
}

/**
 * Write LC3 block of data
 */
void lc3bin_write_data(FILE *fp, const void *data, int nbytes)
{
    uint16_t hdr_nbytes = nbytes;
    fwrite(&hdr_nbytes, sizeof(hdr_nbytes), 1, fp);

    fwrite(data, 1, nbytes, fp);
}
