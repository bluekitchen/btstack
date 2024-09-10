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

template <typename T>
T ConsumeInRange(FuzzedDataProvider &fdp, T min, T max) {
  return fdp.ConsumeIntegralInRange<T>(min, max);
}

template <>
float ConsumeInRange(FuzzedDataProvider &fdp, float min, float max) {
  return fdp.ConsumeFloatingPointInRange<float>(min, max);
}

template <typename T>
int encode(Encoder &e, int nchannels, int frame_size, FuzzedDataProvider &fdp,
  T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max())
{
  int pcm_samples = nchannels * e.GetFrameSamples();
  if (fdp.remaining_bytes() < pcm_samples * sizeof(T))
    return -1;

  std::vector<T> pcm(pcm_samples);
  for (auto &s: pcm)
    s = ConsumeInRange<T>(fdp, min, max);

  e.Encode(pcm.data(),
    frame_size, std::vector<uint8_t>(nchannels * frame_size).data());

  return 0;
}

int encode(Encoder &e, int frame_size, int nchannels,
  PcmFormat fmt, FuzzedDataProvider &fdp)
{
  int sample_bytes =
    fmt == PcmFormat::kS16 ? sizeof(int16_t) :
    fmt == PcmFormat::kS24 ? sizeof(int32_t) :
    fmt == PcmFormat::kS24In3Le ? sizeof(uint8_t) * 3 :
    fmt == PcmFormat::kF32 ? sizeof(float) : 0;

  int pcm_bytes = nchannels * e.GetFrameSamples() * sample_bytes;
  if (fdp.remaining_bytes() < pcm_bytes)
    return -1;

  e.Encode(fmt, fdp.ConsumeBytes<uint8_t>(pcm_bytes).data(),
    frame_size, std::vector<uint8_t>(nchannels * frame_size).data());

  return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  const int dt_list[] = { 2500, 5000, 7500, 10000 };
  const int sr_list[] = { 8000, 16000, 24000, 32000, 48000 };

  FuzzedDataProvider fdp(data, size);

  int dt_us = fdp.PickValueInArray(dt_list);
  int sr_hz = fdp.PickValueInArray(sr_list);
  int nchannels = fdp.PickValueInArray({1, 2});
  bool hrmode = fdp.ConsumeBool();

  int sr_pcm_hz = fdp.PickValueInArray(sr_list);
  if (sr_pcm_hz < sr_hz)
    sr_pcm_hz = 0;

  Encoder enc(dt_us, sr_hz, sr_pcm_hz, nchannels, hrmode);

  PcmFormat fmt = fdp.PickValueInArray(
    { PcmFormat::kS16, PcmFormat::kS24,
      PcmFormat::kS24In3Le, PcmFormat::kF32 });

  int frame_size = fdp.ConsumeIntegralInRange(
    LC3_MIN_FRAME_BYTES, LC3_MAX_FRAME_BYTES);

  switch (fmt) {

  case PcmFormat::kS16:
    return encode<int16_t>(enc, nchannels, frame_size, fdp);

  case PcmFormat::kS24: {
    const int32_t s24_min = -(1 << 23);
    const int32_t s24_max =  (1 << 23) - 1;
    return encode<int32_t>(enc, nchannels, frame_size, fdp, s24_min, s24_max);
  }

  case PcmFormat::kF32: {
    const float f32_min = -1.0;
    const float f32_max =  1.0;
    return encode<float>(enc, nchannels, frame_size, fdp, f32_min, f32_max);
  }

  case PcmFormat::kS24In3Le:
    return encode(enc, nchannels, frame_size, fmt, fdp);
  }

  return 0;
}
