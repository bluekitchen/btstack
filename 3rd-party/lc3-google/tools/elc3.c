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

#define _POSIX_C_SOURCE 199309L

#include <stdalign.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include <lc3.h>
#include "lc3bin.h"
#include "wave.h"

#define MAX_CHANNELS 2


/**
 * Error handling
 */

static void error(int status, const char *format, ...)
{
    va_list args;

    fflush(stdout);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, status ? ": %s\n" : "\n", strerror(status));
    exit(status);
}


/**
 * Parameters
 */

struct parameters {
    const char *fname_in;
    const char *fname_out;
    float frame_ms;
    int srate_hz;
    bool hrmode;
    int bitrate;
};

static struct parameters parse_args(int argc, char *argv[])
{
    static const char *usage =
        "Usage: %s [options] [wav_file] [out_file]\n"
        "\n"
        "wav_file\t"  "Input wave file, stdin if omitted\n"
        "out_file\t"  "Output bitstream file, stdout if omitted\n"
        "\n"
        "Options:\n"
        "\t-h\t"     "Display help\n"
        "\t-b\t"     "Bitrate in bps (mandatory)\n"
        "\t-m\t"     "Frame duration in ms (default 10)\n"
        "\t-r\t"     "Encoder samplerate (default is input samplerate)\n"
        "\t-H\t"     "Enable high-resolution mode\n"
        "\n";

    struct parameters p = { .frame_ms = 10 };

    for (int iarg = 1; iarg < argc; ) {
        const char *arg = argv[iarg++];

        if (arg[0] == '-') {
            if (arg[2] != '\0')
                error(EINVAL, "Option %s", arg);

            char opt = arg[1];
            const char *optarg = NULL;

            switch (opt) {
                case 'b': case 'm': case 'r':
                    if (iarg >= argc)
                        error(EINVAL, "Argument %s", arg);
                    optarg = argv[iarg++];
            }

            switch (opt) {
                case 'h': fprintf(stderr, usage, argv[0]); exit(0);
                case 'b': p.bitrate = atoi(optarg); break;
                case 'm': p.frame_ms = atof(optarg); break;
                case 'r': p.srate_hz = atoi(optarg); break;
                case 'H': p.hrmode = true; break;
                default:
                    error(EINVAL, "Option %s", arg);
            }

        } else {

            if (!p.fname_in)
                p.fname_in = arg;
            else if (!p.fname_out)
                p.fname_out = arg;
            else
                error(EINVAL, "Argument %s", arg);
        }
    }

    return p;
}


/**
 * Return time in (us) from unspecified point in the past
 */

static unsigned clock_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (unsigned)(ts.tv_sec * 1000*1000) + (unsigned)(ts.tv_nsec / 1000);
}


/**
 * Entry point
 */

int main(int argc, char *argv[])
{
    /* --- Read parameters --- */

    struct parameters p = parse_args(argc, argv);
    FILE *fp_in = stdin, *fp_out = stdout;

    if (p.fname_in && (fp_in = fopen(p.fname_in, "rb")) == NULL)
        error(errno, "%s", p.fname_in);

    if (p.fname_out && (fp_out = fopen(p.fname_out, "wb")) == NULL)
        error(errno, "%s", p.fname_out);

    /* --- Check parameters --- */

    int frame_us = p.frame_ms * 1000;
    int srate_hz, nchannels, nsamples;
    int pcm_sbits, pcm_sbytes;

    if (wave_read_header(fp_in,
            &pcm_sbits, &pcm_sbytes, &srate_hz, &nchannels, &nsamples) < 0)
        error(EINVAL, "Bad or unsupported WAVE input file");

    if (p.bitrate <= 0)
        error(EINVAL, "Bitrate");

    if (!LC3_CHECK_DT_US(frame_us))
        error(EINVAL, "Frame duration");

    if (!LC3_HR_CHECK_SR_HZ(p.hrmode, srate_hz))
        error(EINVAL, "Samplerate %d Hz", srate_hz);

    if (pcm_sbits != 16 && pcm_sbits != 24)
        error(EINVAL, "Bitdepth %d", pcm_sbits);

    if ((pcm_sbits == 16 && pcm_sbytes != 16/8) ||
        (pcm_sbits == 24 && pcm_sbytes != 24/8 && pcm_sbytes != 32/8))
        error(EINVAL, "Sample storage on %d bytes", pcm_sbytes);

    if (nchannels < 1 || nchannels > MAX_CHANNELS)
        error(EINVAL, "Number of channels %d", nchannels);

    if (p.srate_hz && (!LC3_HR_CHECK_SR_HZ(p.hrmode, p.srate_hz) ||
                       p.srate_hz > srate_hz                       ))
        error(EINVAL, "Encoder samplerate %d Hz", p.srate_hz);

    int enc_srate_hz = !p.srate_hz ? srate_hz : p.srate_hz;
    int enc_samples = !p.srate_hz ? nsamples :
        ((int64_t)nsamples * enc_srate_hz) / srate_hz;

    int block_bytes = lc3_hr_frame_block_bytes(
        p.hrmode, frame_us, srate_hz, nchannels, p.bitrate);

    int bitrate = lc3_hr_resolve_bitrate(
        p.hrmode, frame_us, srate_hz, block_bytes);

    if (bitrate != p.bitrate)
        fprintf(stderr, "Bitrate adjusted to %d bps\n", bitrate);

    lc3bin_write_header(fp_out,
        frame_us, enc_srate_hz, p.hrmode,
        bitrate, nchannels, enc_samples);

    /* --- Setup encoding --- */

    int8_t alignas(int32_t) pcm[2 * LC3_HR_MAX_FRAME_SAMPLES*4];
    uint8_t out[2 * LC3_HR_MAX_FRAME_BYTES];
    lc3_encoder_t enc[2];

    int frame_samples = lc3_hr_frame_samples(
        p.hrmode, frame_us, srate_hz);
    int encode_samples = nsamples + lc3_hr_delay_samples(
        p.hrmode, frame_us, srate_hz);
    enum lc3_pcm_format pcm_fmt =
        pcm_sbytes == 32/8 ? LC3_PCM_FORMAT_S24 :
        pcm_sbytes == 24/8 ? LC3_PCM_FORMAT_S24_3LE : LC3_PCM_FORMAT_S16;

    for (int ich = 0; ich < nchannels; ich++) {
        enc[ich] = lc3_hr_setup_encoder(
            p.hrmode, frame_us, enc_srate_hz, srate_hz,
            malloc(lc3_hr_encoder_size(p.hrmode, frame_us, srate_hz)));

        if (!enc[ich])
            error(EINVAL, "Encoder initialization failed");
    }

    /* --- Encoding loop --- */

    static const char *dash_line = "========================================";

    int nsec = 0;
    unsigned t0 = clock_us();

    for (int i = 0; i * frame_samples < encode_samples; i++) {

        int nread = wave_read_pcm(fp_in, pcm_sbytes, nchannels, frame_samples, pcm);

        memset(pcm + nread * nchannels * pcm_sbytes, 0,
            nchannels * (frame_samples - nread) * pcm_sbytes);

        if (floorf(i * frame_us * 1e-6) > nsec) {
            float progress = fminf(
                (float)i * frame_samples / encode_samples, 1);

            fprintf(stderr, "%02d:%02d [%-40s]\r",
                    nsec / 60, nsec % 60,
                    dash_line + (int)floorf((1 - progress) * 40));

            nsec = (int)(i * frame_us * 1e-6);
        }

        uint8_t *out_ptr = out;
        for (int ich = 0; ich < nchannels; ich++) {
            int frame_bytes = block_bytes / nchannels
                + (ich < block_bytes % nchannels);

            lc3_encode(enc[ich],
                pcm_fmt, pcm + ich * pcm_sbytes, nchannels,
                frame_bytes, out_ptr);

            out_ptr += frame_bytes;
        }

        lc3bin_write_data(fp_out, out, block_bytes);
    }

    unsigned t = (clock_us() - t0) / 1000;
    nsec = encode_samples / srate_hz;

    fprintf(stderr, "%02d:%02d Encoded in %d.%d seconds %20s\n",
        nsec / 60, nsec % 60, t / 1000, t % 1000, "");

    /* --- Cleanup --- */

    for (int ich = 0; ich < nchannels; ich++)
        free(enc[ich]);

    if (fp_in != stdin)
        fclose(fp_in);

    if (fp_out != stdout)
        fclose(fp_out);
}
