#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lhdc_dec.h"

int g_nzc_formula = 0;
int g_pkframe = 0;

static uint8_t *read_all(const char *path, size_t *size) {
    FILE *f = NULL;
#ifdef _MSC_VER
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return NULL; }
    uint8_t *p = (uint8_t *)malloc((size_t)n);
    if (!p || fread(p, 1, (size_t)n, f) != (size_t)n) {
        free(p); fclose(f); return NULL;
    }
    fclose(f); *size = (size_t)n; return p;
}
int main(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s input.lhdc sample_rate bit_depth [out.pcm]\n", argv[0]);
        return 2;
    }
    uint32_t sr = (uint32_t)strtoul(argv[2], NULL, 10);
    uint8_t depth = (uint8_t)strtoul(argv[3], NULL, 10);
    size_t input_size = 0;
    uint8_t *input = read_all(argv[1], &input_size);
    if (!input) { fprintf(stderr, "cannot read input\n"); return 1; }

    lhdc_dec_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.sample_rate = (lhdc_dec_sample_rate_t)sr;
    cfg.bit_depth = depth == 24 ? LHDC_DEC_BITDEPTH_24 : LHDC_DEC_BITDEPTH_16;
    cfg.frame_duration = LHDC_DEC_FRAME_5MS;
    cfg.channels = 2;
    cfg.max_frame_bytes = 8192;
    size_t ws_size = lhdc_dec_get_workspace_size(sr, 5);
    void *ws = calloc(1, ws_size);
    lhdc_decoder_t *dec = ws ? lhdc_dec_init(ws, &cfg) : NULL;
    if (!dec) { fprintf(stderr, "decoder init failed\n"); free(input); free(ws); return 1; }

    FILE *out = NULL;
    if (argc > 4) {
#ifdef _MSC_VER
        fopen_s(&out, argv[4], "wb");
#else
        out = fopen(argv[4], "wb");
#endif
    }
    size_t off = 0;
    uint32_t good = 0, bad = 0;
    uint64_t pcm_frames = 0;
    uint32_t errors[16] = {0};
    while (off + 2 <= input_size) {
        uint16_t h = (uint16_t)(input[off] | ((uint16_t)input[off + 1] << 8));
        size_t payload = (size_t)(h & 0x3ff) * 2;
        size_t step = 2 + payload;
        if (payload == 0 || off + step > input_size) {
            fprintf(stderr, "bad framing at %zu hdr=%04x payload=%zu\n", off, h, payload);
            break;
        }
        uint8_t pcm[1920 * 2 * 4];
        size_t consumed = 0;
        uint32_t generated = 0;
        lhdc_dec_frame_info_t info;
        lhdc_dec_ret_t r = lhdc_dec_decode_frame(dec, input + off, step, pcm, 1920,
                                                  &consumed, &generated, &info);
        if (r == LHDC_DEC_OK && consumed == step) {
            good++; pcm_frames += generated;
            if (out) fwrite(pcm, 1, (size_t)generated * info.channels * (info.bit_depth / 8), out);
        } else {
            bad++;
            unsigned e = (r < 0 && -r < 16) ? (unsigned)(-r) : 0;
            errors[e]++;
            if (bad <= 20) fprintf(stderr, "error frame=%u off=%zu ret=%d consumed=%zu step=%zu\n",
                                   good + bad - 1, off, (int)r, consumed, step);
        }
        off += step;
    }
    if (out) fclose(out);
    printf("input=%zu bytes good=%u bad=%u pcm_frames=%llu\n",
           input_size, good, bad, (unsigned long long)pcm_frames);
    for (unsigned i = 0; i < 16; i++) {
        if (errors[i]) printf("error -%u: %u\n", i, errors[i]);
    }
    free(input);
    free(ws);
    lhdc_dec_free_window();
    return bad ? 3 : 0;
}
