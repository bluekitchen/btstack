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
        "\n";

    struct parameters p = { .frame_ms = 10 };

    for (int iarg = 1; iarg < argc; ) {
        const char *arg = argv[iarg++];

        if (arg[0] == '-') {
            if (arg[2] != '\0')
                error(EINVAL, "Option %s", arg);

            char opt = arg[1];
            const char *optarg;

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

    if (p.srate_hz && !LC3_CHECK_SR_HZ(p.srate_hz))
        error(EINVAL, "Samplerate %d Hz", p.srate_hz);

    /* --- Check parameters --- */

    int frame_us = p.frame_ms * 1000;
    int srate_hz, nch, nsamples;
    int pcm_sbits, pcm_sbytes;

    if (wave_read_header(fp_in,
            &pcm_sbits, &pcm_sbytes, &srate_hz, &nch, &nsamples) < 0)
        error(EINVAL, "Bad or unsupported WAVE input file");

    if (p.bitrate <= 0)
        error(EINVAL, "Bitrate");

    if (!LC3_CHECK_DT_US(frame_us))
        error(EINVAL, "Frame duration");

    if (!LC3_CHECK_SR_HZ(srate_hz) || (p.srate_hz && p.srate_hz > srate_hz))
        error(EINVAL, "Samplerate %d Hz", srate_hz);

    if (pcm_sbits != 16 && pcm_sbits != 24)
        error(EINVAL, "Bitdepth %d", pcm_sbits);

    if (pcm_sbytes != (pcm_sbits == 16 ? 2 : 4))
        error(EINVAL, "Sample storage on %d bytes", pcm_sbytes);

    if (nch  < 1 || nch  > 2)
        error(EINVAL, "Number of channels %d", nch);

    int enc_srate_hz = !p.srate_hz ? srate_hz : p.srate_hz;
    int enc_samples = !p.srate_hz ? nsamples :
        ((int64_t)nsamples * enc_srate_hz) / srate_hz;

    lc3bin_write_header(fp_out,
        frame_us, enc_srate_hz, p.bitrate, nch, enc_samples);

    /* --- Setup encoding --- */

    int frame_bytes = lc3_frame_bytes(frame_us, p.bitrate / nch);
    int frame_samples = lc3_frame_samples(frame_us, srate_hz);
    int encode_samples = nsamples + lc3_delay_samples(frame_us, srate_hz);

    lc3_encoder_t enc[nch];
    int8_t alignas(int32_t) pcm[nch * frame_samples * pcm_sbytes];
    enum lc3_pcm_format pcm_fmt =
        pcm_sbits == 24 ? LC3_PCM_FORMAT_S24 : LC3_PCM_FORMAT_S16;
    uint8_t out[nch][frame_bytes];

    for (int ich = 0; ich < nch; ich++)
        enc[ich] = lc3_setup_encoder(frame_us, enc_srate_hz, srate_hz,
            malloc(lc3_encoder_size(frame_us, srate_hz)));

    /* --- Encoding loop --- */

    static const char *dash_line = "========================================";

    int nsec = 0;
    unsigned t0 = clock_us();

    for (int i = 0; i * frame_samples < encode_samples; i++) {

        int nread = wave_read_pcm(fp_in, pcm_sbytes, nch, frame_samples, pcm);

        memset(pcm + nread * nch, 0,
            nch * (frame_samples - nread) * pcm_sbytes);

        if (floorf(i * frame_us * 1e-6) > nsec) {
            float progress = fminf(
                (float)i * frame_samples / encode_samples, 1);

            fprintf(stderr, "%02d:%02d [%-40s]\r",
                    nsec / 60, nsec % 60,
                    dash_line + (int)floorf((1 - progress) * 40));

            nsec = (int)(i * frame_us * 1e-6);
        }

        for (int ich = 0; ich < nch; ich++)
            lc3_encode(enc[ich],
                pcm_fmt, pcm + ich * pcm_sbytes, nch,
                frame_bytes, out[ich]);

        lc3bin_write_data(fp_out, out, nch, frame_bytes);
    }

    unsigned t = (clock_us() - t0) / 1000;
    nsec = encode_samples / srate_hz;

    fprintf(stderr, "%02d:%02d Encoded in %d.%d seconds %20s\n",
        nsec / 60, nsec % 60, t / 1000, t % 1000, "");

    /* --- Cleanup --- */

    for (int ich = 0; ich < nch; ich++)
        free(enc[ich]);

    if (fp_in != stdin)
        fclose(fp_in);

    if (fp_out != stdout)
        fclose(fp_out);
}
