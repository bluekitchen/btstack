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

#include <lc3_cpp.h>
#include <fuzzer/FuzzedDataProvider.h>

using namespace lc3;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  const int dt_list[] = { 2500, 5000, 7500, 10000 };
  const int sr_list[] = { 8000, 16000, 24000, 32000, 48000, 96000 };

  FuzzedDataProvider fdp(data, size);

  int dt_us = fdp.PickValueInArray(dt_list);
  int sr_hz = fdp.PickValueInArray(sr_list);
  int nchannels =fdp.PickValueInArray({1, 2});
  bool hrmode = fdp.ConsumeBool();

  int sr_pcm_hz = fdp.PickValueInArray(sr_list);
  if (sr_pcm_hz < sr_hz)
    sr_pcm_hz = 0;

  Decoder dec(dt_us, sr_hz, sr_pcm_hz, nchannels, hrmode);

  int frame_size = fdp.ConsumeIntegralInRange(
    LC3_MIN_FRAME_BYTES, LC3_MAX_FRAME_BYTES);

  PcmFormat fmt = fdp.PickValueInArray(
    { PcmFormat::kS16, PcmFormat::kS24,
      PcmFormat::kS24In3Le, PcmFormat::kF32 });

  int frame_samples = dec.GetFrameSamples();
  if (frame_samples < 0)
      return -1;

  int sample_bytes =
    fmt == PcmFormat::kS16 ? sizeof(int16_t) :
    fmt == PcmFormat::kS24 ? sizeof(int32_t) :
    fmt == PcmFormat::kS24In3Le ? sizeof(uint8_t) * 3 :
    fmt == PcmFormat::kF32 ? sizeof(float) : 0;

  if (fdp.remaining_bytes() < frame_size * nchannels)
    return -1;

  dec.Decode(
    fdp.ConsumeBytes<uint8_t>(nchannels * frame_size).data(), frame_size,
    fmt, std::vector<uint8_t>(nchannels * frame_samples * sample_bytes).data());

  return 0;
}
