/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <fuzzer/FuzzedDataProvider.h>

#include "include/lc3.h"

void TestEncoder(FuzzedDataProvider& fdp) {
  enum lc3_pcm_format pcm_format =
      fdp.PickValueInArray({LC3_PCM_FORMAT_S16, LC3_PCM_FORMAT_S24});
  int dt_us = fdp.PickValueInArray({10000, 7500});
  int sr_hz =
      fdp.PickValueInArray({8000, 16000, 24000, 32000, /*44100,*/ 48000});
  unsigned enc_size = lc3_encoder_size(dt_us, sr_hz);
  uint16_t output_byte_count = fdp.ConsumeIntegralInRange(20, 400);
  uint16_t num_frames = lc3_frame_samples(dt_us, sr_hz);
  uint8_t bytes_per_frame = (pcm_format == LC3_PCM_FORMAT_S16 ? 2 : 4);

  if (fdp.remaining_bytes() < num_frames * bytes_per_frame) {
    return;
  }

  std::vector<uint32_t> input_frames(
      num_frames / (pcm_format == LC3_PCM_FORMAT_S16 ? 2 : 1));
  fdp.ConsumeData(input_frames.data(), num_frames * bytes_per_frame);

  void* lc3_encoder_mem = nullptr;
  lc3_encoder_mem = malloc(enc_size);
  lc3_encoder_t lc3_encoder =
      lc3_setup_encoder(dt_us, sr_hz, 0, lc3_encoder_mem);

  std::vector<uint8_t> output(output_byte_count);
  lc3_encode(lc3_encoder, pcm_format, (const int16_t*)input_frames.data(), 1,
             output.size(), output.data());

  free(lc3_encoder_mem);
  lc3_encoder_mem = nullptr;
  return;
}

void TestDecoder(FuzzedDataProvider& fdp) {
  enum lc3_pcm_format pcm_format =
      fdp.PickValueInArray({LC3_PCM_FORMAT_S16, LC3_PCM_FORMAT_S24});
  int dt_us = fdp.PickValueInArray({10000, 7500});
  int sr_hz =
      fdp.PickValueInArray({8000, 16000, 24000, 32000, /*44100,*/ 48000});
  unsigned dec_size = lc3_decoder_size(dt_us, sr_hz);
  uint16_t input_byte_count = fdp.ConsumeIntegralInRange(20, 400);
  uint16_t num_frames = lc3_frame_samples(dt_us, sr_hz);

  if (fdp.remaining_bytes() < input_byte_count) {
    return;
  }

  std::vector<uint8_t> input(input_byte_count);
  fdp.ConsumeData(input.data(), input.size());

  void* lc3_decoder_mem = nullptr;
  lc3_decoder_mem = malloc(dec_size);
  lc3_decoder_t lc3_decoder =
      lc3_setup_decoder(dt_us, sr_hz, 0, lc3_decoder_mem);

  std::vector<uint32_t> output(num_frames /
                               (pcm_format == LC3_PCM_FORMAT_S16 ? 2 : 1));
  lc3_decode(lc3_decoder, input.data(), input.size(), pcm_format,
             (int16_t*)output.data(), 1);

  /* Empty input performs PLC (packet loss concealment) */
  lc3_decode(lc3_decoder, nullptr, 0, pcm_format, (int16_t*)output.data(), 1);

  free(lc3_decoder_mem);
  lc3_decoder_mem = nullptr;
  return;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fdp(data, size);
  TestEncoder(fdp);
  TestDecoder(fdp);
  return 0;
}