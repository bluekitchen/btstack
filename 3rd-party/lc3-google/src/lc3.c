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

#include <lc3.h>

#include "common.h"
#include "bits.h"

#include "attdet.h"
#include "bwdet.h"
#include "ltpf.h"
#include "mdct.h"
#include "energy.h"
#include "sns.h"
#include "tns.h"
#include "spec.h"
#include "plc.h"


/**
 * Frame side data
 */

struct side_data {
    enum lc3_bandwidth bw;
    bool pitch_present;
    lc3_ltpf_data_t ltpf;
    lc3_sns_data_t sns;
    lc3_tns_data_t tns;
    lc3_spec_side_t spec;
};


/* ----------------------------------------------------------------------------
 *  General
 * -------------------------------------------------------------------------- */

/**
 * Resolve frame duration in us
 * us              Frame duration in us
 * hrmode          High-resolution mode indication
 * return          Frame duration identifier, or LC3_NUM_DT
 */
static enum lc3_dt resolve_dt(int us, bool hrmode)
{
    return LC3_PLUS && us ==  2500 ? LC3_DT_2M5 :
           LC3_PLUS && us ==  5000 ? LC3_DT_5M  :
           !hrmode  && us ==  7500 ? LC3_DT_7M5 :
                       us == 10000 ? LC3_DT_10M : LC3_NUM_DT;
}

/**
 * Resolve samplerate in Hz
 * hz              Samplerate in Hz
 * hrmode          High-resolution mode indication
 * return          Sample rate identifier, or LC3_NUM_SRATE
 */
static enum lc3_srate resolve_srate(int hz, bool hrmode)
{
    hrmode = LC3_PLUS_HR && hrmode;

    return !hrmode && hz ==  8000 ? LC3_SRATE_8K     :
           !hrmode && hz == 16000 ? LC3_SRATE_16K    :
           !hrmode && hz == 24000 ? LC3_SRATE_24K    :
           !hrmode && hz == 32000 ? LC3_SRATE_32K    :
           !hrmode && hz == 48000 ? LC3_SRATE_48K    :
            hrmode && hz == 48000 ? LC3_SRATE_48K_HR :
            hrmode && hz == 96000 ? LC3_SRATE_96K_HR : LC3_NUM_SRATE;
}

/**
 * Return the number of PCM samples in a frame
 */
LC3_EXPORT int lc3_hr_frame_samples(bool hrmode, int dt_us, int sr_hz)
{
    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr >= LC3_NUM_SRATE)
        return -1;

    return lc3_ns(dt, sr);
}

LC3_EXPORT int lc3_frame_samples(int dt_us, int sr_hz)
{
    return lc3_hr_frame_samples(false, dt_us, sr_hz);
}

/**
 * Return the size of frames or frame blocks, from bitrate
 */
LC3_EXPORT int lc3_hr_frame_block_bytes(
    bool hrmode, int dt_us, int sr_hz, int nchannels, int bitrate)
{
    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr >= LC3_NUM_SRATE
            || nchannels < 1 || nchannels > 8 || bitrate < 0)
        return -1;

    bitrate = LC3_CLIP(bitrate, 0, 8*LC3_HR_MAX_BITRATE);

    return LC3_CLIP((bitrate * (int)(1 + dt)) / 3200,
        nchannels * lc3_min_frame_bytes(dt, sr),
        nchannels * lc3_max_frame_bytes(dt, sr) );
}

LC3_EXPORT int lc3_frame_bock_bytes(int dt_us, int nchannels, int bitrate)
{
    return lc3_hr_frame_block_bytes(false, dt_us, 8000, nchannels, bitrate);
}

LC3_EXPORT int lc3_hr_frame_bytes(
    bool hrmode, int dt_us, int sr_hz, int bitrate)
{
    return lc3_hr_frame_block_bytes(hrmode, dt_us, sr_hz, 1, bitrate);
}

LC3_EXPORT int lc3_frame_bytes(int dt_us, int bitrate)
{
    return lc3_hr_frame_bytes(false, dt_us, 8000, bitrate);
}

/**
 * Resolve the bitrate, from the size of frames or frame blocks
 */
LC3_EXPORT int lc3_hr_resolve_bitrate(
    bool hrmode, int dt_us, int sr_hz, int nbytes)
{
    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr >= LC3_NUM_SRATE || nbytes < 0)
        return -1;

    return LC3_MIN(((int64_t)nbytes * 3200 + dt) / (1 + dt), INT_MAX);
}

LC3_EXPORT int lc3_resolve_bitrate(int dt_us, int nbytes)
{
    return lc3_hr_resolve_bitrate(false, dt_us, 8000, nbytes);
}

/**
 * Return algorithmic delay, as a number of samples
 */
LC3_EXPORT int lc3_hr_delay_samples(bool hrmode, int dt_us, int sr_hz)
{
    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr >= LC3_NUM_SRATE)
        return -1;

    return 2 * lc3_nd(dt, sr) - lc3_ns(dt, sr);
}

LC3_EXPORT int lc3_delay_samples(int dt_us, int sr_hz)
{
    return lc3_hr_delay_samples(false, dt_us, sr_hz);
}


/* ----------------------------------------------------------------------------
 *  Encoder
 * -------------------------------------------------------------------------- */

/**
 * Input PCM Samples from signed 16 bits
 * encoder         Encoder state
 * pcm, stride     Input PCM samples, and count between two consecutives
 */
static void load_s16(
    struct lc3_encoder *encoder, const void *_pcm, int stride)
{
    const int16_t *pcm = _pcm;

    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr_pcm;

    int16_t *xt = (int16_t *)encoder->x + encoder->xt_off;
    float *xs = encoder->x + encoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for (int i = 0; i < ns; i++, pcm += stride)
        xt[i] = *pcm, xs[i] = *pcm;
}

/**
 * Input PCM Samples from signed 24 bits
 * encoder         Encoder state
 * pcm, stride     Input PCM samples, and count between two consecutives
 */
static void load_s24(
    struct lc3_encoder *encoder, const void *_pcm, int stride)
{
    const int32_t *pcm = _pcm;

    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr_pcm;

    int16_t *xt = (int16_t *)encoder->x + encoder->xt_off;
    float *xs = encoder->x + encoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for (int i = 0; i < ns; i++, pcm += stride) {
        xt[i] = *pcm >> 8;
        xs[i] = lc3_ldexpf(*pcm, -8);
    }
}

/**
 * Input PCM Samples from signed 24 bits packed
 * encoder         Encoder state
 * pcm, stride     Input PCM samples, and count between two consecutives
 */
static void load_s24_3le(
    struct lc3_encoder *encoder, const void *_pcm, int stride)
{
    const uint8_t *pcm = _pcm;

    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr_pcm;

    int16_t *xt = (int16_t *)encoder->x + encoder->xt_off;
    float *xs = encoder->x + encoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for (int i = 0; i < ns; i++, pcm += 3*stride) {
        int32_t in = ((uint32_t)pcm[0] <<  8) |
                     ((uint32_t)pcm[1] << 16) |
                     ((uint32_t)pcm[2] << 24)  ;

        xt[i] = in >> 16;
        xs[i] = lc3_ldexpf(in, -16);
    }
}

/**
 * Input PCM Samples from float 32 bits
 * encoder         Encoder state
 * pcm, stride     Input PCM samples, and count between two consecutives
 */
static void load_float(
    struct lc3_encoder *encoder, const void *_pcm, int stride)
{
    const float *pcm = _pcm;

    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr_pcm;

    int16_t *xt = (int16_t *)encoder->x + encoder->xt_off;
    float *xs = encoder->x + encoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for (int i = 0; i < ns; i++, pcm += stride) {
        xs[i] = lc3_ldexpf(*pcm, 15);
        xt[i] = LC3_SAT16((int32_t)xs[i]);
    }
}

/**
 * Frame Analysis
 * encoder         Encoder state
 * nbytes          Size in bytes of the frame
 * side            Return frame data
 */
static void analyze(struct lc3_encoder *encoder,
    int nbytes, struct side_data *side)
{
    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr;
    enum lc3_srate sr_pcm = encoder->sr_pcm;

    int16_t *xt = (int16_t *)encoder->x + encoder->xt_off;
    float *xs = encoder->x + encoder->xs_off;
    int ns = lc3_ns(dt, sr_pcm);
    int nt = lc3_nt(sr_pcm);

    float *xd = encoder->x + encoder->xd_off;
    float *xf = xs;

    /* --- Temporal --- */

    bool att = lc3_attdet_run(dt, sr_pcm, nbytes, &encoder->attdet, xt);

    side->pitch_present =
        lc3_ltpf_analyse(dt, sr_pcm, &encoder->ltpf, xt, &side->ltpf);

    memmove(xt - nt, xt + (ns-nt), nt * sizeof(*xt));

    /* --- Spectral --- */

    float e[LC3_MAX_BANDS];

    lc3_mdct_forward(dt, sr_pcm, sr, xs, xd, xf);

    bool nn_flag = lc3_energy_compute(dt, sr, xf, e);
    if (nn_flag)
        lc3_ltpf_disable(&side->ltpf);

    side->bw = lc3_bwdet_run(dt, sr, e);

    lc3_sns_analyze(dt, sr, nbytes, e, att, &side->sns, xf, xf);

    lc3_tns_analyze(dt, side->bw, nn_flag, nbytes, &side->tns, xf);

    lc3_spec_analyze(dt, sr,
        nbytes, side->pitch_present, &side->tns,
        &encoder->spec, xf, &side->spec);
}

/**
 * Encode bitstream
 * encoder         Encoder state
 * side            The frame data
 * nbytes          Target size of the frame (20 to 400)
 * buffer          Output bitstream buffer of `nbytes` size
 */
static void encode(struct lc3_encoder *encoder,
    const struct side_data *side, int nbytes, void *buffer)
{
    enum lc3_dt dt = encoder->dt;
    enum lc3_srate sr = encoder->sr;

    float *xf = encoder->x + encoder->xs_off;
    enum lc3_bandwidth bw = side->bw;

    lc3_bits_t bits;

    lc3_setup_bits(&bits, LC3_BITS_MODE_WRITE, buffer, nbytes);

    lc3_bwdet_put_bw(&bits, sr, bw);

    lc3_spec_put_side(&bits, dt, sr, &side->spec);

    lc3_tns_put_data(&bits, &side->tns);

    lc3_put_bit(&bits, side->pitch_present);

    lc3_sns_put_data(&bits, &side->sns);

    if (side->pitch_present)
        lc3_ltpf_put_data(&bits, &side->ltpf);

    lc3_spec_encode(&bits, dt, sr, bw, nbytes, &side->spec, xf);

    lc3_flush_bits(&bits);
}

/**
 * Return size needed for an encoder
 */
LC3_EXPORT unsigned lc3_hr_encoder_size(bool hrmode, int dt_us, int sr_hz)
{
    if (resolve_dt(dt_us, hrmode) >= LC3_NUM_DT ||
        resolve_srate(sr_hz, hrmode) >= LC3_NUM_SRATE)
        return 0;

    return sizeof(struct lc3_encoder) +
        (LC3_ENCODER_BUFFER_COUNT(dt_us, sr_hz)-1) * sizeof(float);
}

LC3_EXPORT unsigned lc3_encoder_size(int dt_us, int sr_hz)
{
    return lc3_hr_encoder_size(false, dt_us, sr_hz);
}

/**
 * Setup encoder
 */
LC3_EXPORT struct lc3_encoder *lc3_hr_setup_encoder(
    bool hrmode, int dt_us, int sr_hz, int sr_pcm_hz, void *mem)
{
    if (sr_pcm_hz <= 0)
        sr_pcm_hz = sr_hz;

    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);
    enum lc3_srate sr_pcm = resolve_srate(sr_pcm_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr_pcm >= LC3_NUM_SRATE || sr > sr_pcm || !mem)
        return NULL;

    struct lc3_encoder *encoder = mem;
    int ns = lc3_ns(dt, sr_pcm);
    int nt = lc3_nt(sr_pcm);

    *encoder = (struct lc3_encoder){
        .dt = dt, .sr = sr,
        .sr_pcm = sr_pcm,

        .xt_off = nt,
        .xs_off = (nt + ns) / 2,
        .xd_off = (nt + ns) / 2 + ns,
    };

    memset(encoder->x, 0,
        LC3_ENCODER_BUFFER_COUNT(dt_us, sr_pcm_hz) * sizeof(float));

    return encoder;
}

LC3_EXPORT struct lc3_encoder *lc3_setup_encoder(
    int dt_us, int sr_hz, int sr_pcm_hz, void *mem)
{
    return lc3_hr_setup_encoder(false, dt_us, sr_hz, sr_pcm_hz, mem);
}

/**
 * Encode a frame
 */
LC3_EXPORT int lc3_encode(struct lc3_encoder *encoder,
    enum lc3_pcm_format fmt, const void *pcm, int stride, int nbytes, void *out)
{
    static void (* const load[])(struct lc3_encoder *, const void *, int) = {
        [LC3_PCM_FORMAT_S16    ] = load_s16,
        [LC3_PCM_FORMAT_S24    ] = load_s24,
        [LC3_PCM_FORMAT_S24_3LE] = load_s24_3le,
        [LC3_PCM_FORMAT_FLOAT  ] = load_float,
    };

    /* --- Check parameters --- */

    if (!encoder || nbytes < lc3_min_frame_bytes(encoder->dt, encoder->sr)
                 || nbytes > lc3_max_frame_bytes(encoder->dt, encoder->sr))
        return -1;

    /* --- Processing --- */

    struct side_data side;

    load[fmt](encoder, pcm, stride);

    analyze(encoder, nbytes, &side);

    encode(encoder, &side, nbytes, out);

    return 0;
}


/* ----------------------------------------------------------------------------
 *  Decoder
 * -------------------------------------------------------------------------- */

/**
 * Output PCM Samples to signed 16 bits
 * decoder         Decoder state
 * pcm, stride     Output PCM samples, and count between two consecutives
 */
static void store_s16(
    struct lc3_decoder *decoder, void *_pcm, int stride)
{
    int16_t *pcm = _pcm;

    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr_pcm;

    float *xs = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for ( ; ns > 0; ns--, xs++, pcm += stride) {
        int32_t s = *xs >= 0 ? (int)(*xs + 0.5f) : (int)(*xs - 0.5f);
        *pcm = LC3_SAT16(s);
    }
}

/**
 * Output PCM Samples to signed 24 bits
 * decoder         Decoder state
 * pcm, stride     Output PCM samples, and count between two consecutives
 */
static void store_s24(
    struct lc3_decoder *decoder, void *_pcm, int stride)
{
    int32_t *pcm = _pcm;

    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr_pcm;

    float *xs = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for ( ; ns > 0; ns--, xs++, pcm += stride) {
        int32_t s = *xs >= 0 ? (int32_t)(lc3_ldexpf(*xs, 8) + 0.5f)
                             : (int32_t)(lc3_ldexpf(*xs, 8) - 0.5f);
        *pcm = LC3_SAT24(s);
    }
}

/**
 * Output PCM Samples to signed 24 bits packed
 * decoder         Decoder state
 * pcm, stride     Output PCM samples, and count between two consecutives
 */
static void store_s24_3le(
    struct lc3_decoder *decoder, void *_pcm, int stride)
{
    uint8_t *pcm = _pcm;

    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr_pcm;

    float *xs = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for ( ; ns > 0; ns--, xs++, pcm += 3*stride) {
        int32_t s = *xs >= 0 ? (int32_t)(lc3_ldexpf(*xs, 8) + 0.5f)
                             : (int32_t)(lc3_ldexpf(*xs, 8) - 0.5f);

        s = LC3_SAT24(s);
        pcm[0] = (s >>  0) & 0xff;
        pcm[1] = (s >>  8) & 0xff;
        pcm[2] = (s >> 16) & 0xff;
    }
}

/**
 * Output PCM Samples to float 32 bits
 * decoder         Decoder state
 * pcm, stride     Output PCM samples, and count between two consecutives
 */
static void store_float(
    struct lc3_decoder *decoder, void *_pcm, int stride)
{
    float *pcm = _pcm;

    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr_pcm;

    float *xs = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr);

    for ( ; ns > 0; ns--, xs++, pcm += stride) {
        float s = lc3_ldexpf(*xs, -15);
        *pcm = fminf(fmaxf(s, -1.f), 1.f);
    }
}

/**
 * Decode bitstream
 * decoder         Decoder state
 * data, nbytes    Input bitstream buffer
 * side            Return the side data
 * return          0: Ok  < 0: Bitsream error detected
 */
static int decode(struct lc3_decoder *decoder,
    const void *data, int nbytes, struct side_data *side)
{
    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr;

    float *xf = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr);
    int ne = lc3_ne(dt, sr);

    lc3_bits_t bits;
    int ret = 0;

    lc3_setup_bits(&bits, LC3_BITS_MODE_READ, (void *)data, nbytes);

    if ((ret = lc3_bwdet_get_bw(&bits, sr, &side->bw)) < 0)
        return ret;

    if ((ret = lc3_spec_get_side(&bits, dt, sr, &side->spec)) < 0)
        return ret;

    if ((ret = lc3_tns_get_data(&bits, dt, side->bw, nbytes, &side->tns)) < 0)
        return ret;

    side->pitch_present = lc3_get_bit(&bits);

    if ((ret = lc3_sns_get_data(&bits, &side->sns)) < 0)
        return ret;

    if (side->pitch_present)
      lc3_ltpf_get_data(&bits, &side->ltpf);

    if ((ret = lc3_spec_decode(&bits, dt, sr,
                    side->bw, nbytes, &side->spec, xf)) < 0)
        return ret;

    memset(xf + ne, 0, (ns - ne) * sizeof(float));

    return lc3_check_bits(&bits);
}

/**
 * Frame synthesis
 * decoder         Decoder state
 * side            Frame data, NULL performs PLC
 * nbytes          Size in bytes of the frame
 */
static void synthesize(struct lc3_decoder *decoder,
    const struct side_data *side, int nbytes)
{
    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr = decoder->sr;
    enum lc3_srate sr_pcm = decoder->sr_pcm;

    float *xf = decoder->x + decoder->xs_off;
    int ns = lc3_ns(dt, sr_pcm);
    int ne = lc3_ne(dt, sr);

    float *xg = decoder->x + decoder->xg_off;
    float *xs = xf;

    float *xd = decoder->x + decoder->xd_off;
    float *xh = decoder->x + decoder->xh_off;

    if (side) {
        enum lc3_bandwidth bw = side->bw;

        lc3_plc_suspend(&decoder->plc);

        lc3_tns_synthesize(dt, bw, &side->tns, xf);

        lc3_sns_synthesize(dt, sr, &side->sns, xf, xg);

        lc3_mdct_inverse(dt, sr_pcm, sr, xg, xd, xs);

    } else {
        lc3_plc_synthesize(dt, sr, &decoder->plc, xg, xf);

        memset(xf + ne, 0, (ns - ne) * sizeof(float));

        lc3_mdct_inverse(dt, sr_pcm, sr, xf, xd, xs);
    }

    if (!lc3_hr(sr))
        lc3_ltpf_synthesize(dt, sr_pcm, nbytes, &decoder->ltpf,
            side && side->pitch_present ? &side->ltpf : NULL, xh, xs);
}

/**
 * Update decoder state on decoding completion
 * decoder         Decoder state
 */
static void complete(struct lc3_decoder *decoder)
{
    enum lc3_dt dt = decoder->dt;
    enum lc3_srate sr_pcm = decoder->sr_pcm;
    int nh = lc3_nh(dt, sr_pcm);
    int ns = lc3_ns(dt, sr_pcm);

    decoder->xs_off = decoder->xs_off - decoder->xh_off < nh ?
        decoder->xs_off + ns : decoder->xh_off;
}

/**
 * Return size needed for a decoder
 */
LC3_EXPORT unsigned lc3_hr_decoder_size(bool hrmode, int dt_us, int sr_hz)
{
    if (resolve_dt(dt_us, hrmode) >= LC3_NUM_DT ||
        resolve_srate(sr_hz, hrmode) >= LC3_NUM_SRATE)
        return 0;

    return sizeof(struct lc3_decoder) +
        (LC3_DECODER_BUFFER_COUNT(dt_us, sr_hz)-1) * sizeof(float);
}

LC3_EXPORT unsigned lc3_decoder_size(int dt_us, int sr_hz)
{
    return lc3_hr_decoder_size(false, dt_us, sr_hz);
}

/**
 * Setup decoder
 */
LC3_EXPORT struct lc3_decoder *lc3_hr_setup_decoder(
    bool hrmode, int dt_us, int sr_hz, int sr_pcm_hz, void *mem)
{
    if (sr_pcm_hz <= 0)
        sr_pcm_hz = sr_hz;

    enum lc3_dt dt = resolve_dt(dt_us, hrmode);
    enum lc3_srate sr = resolve_srate(sr_hz, hrmode);
    enum lc3_srate sr_pcm = resolve_srate(sr_pcm_hz, hrmode);

    if (dt >= LC3_NUM_DT || sr_pcm >= LC3_NUM_SRATE || sr > sr_pcm || !mem)
        return NULL;

    struct lc3_decoder *decoder = mem;
    int nh = lc3_nh(dt, sr_pcm);
    int ns = lc3_ns(dt, sr_pcm);
    int nd = lc3_nd(dt, sr_pcm);

    *decoder = (struct lc3_decoder){
        .dt = dt, .sr = sr,
        .sr_pcm = sr_pcm,

        .xh_off = 0,
        .xs_off = nh,
        .xd_off = nh + ns,
        .xg_off = nh + ns + nd,
    };

    lc3_plc_reset(&decoder->plc);

    memset(decoder->x, 0,
        LC3_DECODER_BUFFER_COUNT(dt_us, sr_pcm_hz) * sizeof(float));

    return decoder;
}

LC3_EXPORT struct lc3_decoder *lc3_setup_decoder(
    int dt_us, int sr_hz, int sr_pcm_hz, void *mem)
{
    return lc3_hr_setup_decoder(false, dt_us, sr_hz, sr_pcm_hz, mem);
}

/**
 * Decode a frame
 */
LC3_EXPORT int lc3_decode(struct lc3_decoder *decoder,
    const void *in, int nbytes, enum lc3_pcm_format fmt, void *pcm, int stride)
{
    static void (* const store[])(struct lc3_decoder *, void *, int) = {
        [LC3_PCM_FORMAT_S16    ] = store_s16,
        [LC3_PCM_FORMAT_S24    ] = store_s24,
        [LC3_PCM_FORMAT_S24_3LE] = store_s24_3le,
        [LC3_PCM_FORMAT_FLOAT  ] = store_float,
    };

    /* --- Check parameters --- */

    if (!decoder)
        return -1;

    if (in && (nbytes < LC3_MIN_FRAME_BYTES ||
               nbytes > lc3_max_frame_bytes(decoder->dt, decoder->sr) ))
        return -1;

    /* --- Processing --- */

    struct side_data side;

    int ret = !in || (decode(decoder, in, nbytes, &side) < 0);

    synthesize(decoder, ret ? NULL : &side, nbytes);

    store[fmt](decoder, pcm, stride);

    complete(decoder);

    return ret;
}
