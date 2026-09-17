#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <functiondiscoverykeys_devpkey.h>

extern "C" {
#include "wasapi_render.h"
#include "diag_logger.h"
}
#include "wasapi_common.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

static audio_ring_buffer_t s_render_ring_buf;
static HANDLE s_thread_handle = NULL;
static HANDLE s_stop_event = NULL;
static HANDLE s_audio_event = NULL;
static bool s_is_running = false;
static float s_volume = 1.2f; // Calibrated 1.2x volume for balanced caller voice
static volatile uint32_t s_src_sample_rate = 8000;

static IMMDeviceEnumerator *s_pEnumerator = NULL;
static IMMDevice *s_pDevice = NULL;
static IAudioClient *s_pAudioClient = NULL;
static IAudioRenderClient *s_pRenderClient = NULL;
static WAVEFORMATEX *s_pwfx = NULL;
static UINT32 s_bufferFrameCount = 0;

static DWORD WINAPI render_worker_thread(LPVOID lpParam) {
    UNUSED(lpParam);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (!s_pAudioClient || !s_pRenderClient || !s_pwfx) {
        diag_log("[WASAPI_RENDER] Render thread failed: AudioClient or RenderClient is NULL");
        CoUninitialize();
        return 0;
    }

    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    // Reset audio client before pre-roll
    s_pAudioClient->Reset();

    // Pre-roll buffer with silence before starting audio client (essential for WASAPI event-driven render)
    BYTE *pInitialData = NULL;
    HRESULT hr = s_pRenderClient->GetBuffer(s_bufferFrameCount, &pInitialData);
    if (SUCCEEDED(hr) && pInitialData) {
        memset(pInitialData, 0, s_bufferFrameCount * s_pwfx->nBlockAlign);
        s_pRenderClient->ReleaseBuffer(s_bufferFrameCount, 0);
    }

    hr = s_pAudioClient->Start();
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: IAudioClient::Start failed: 0x%08lx", hr);
    } else {
        diag_log("[WASAPI_RENDER] Speaker render engine started successfully (Device Rate: %u Hz, Buffer: %u frames)",
                 s_pwfx->nSamplesPerSec, s_bufferFrameCount);
    }

    HANDLE waitHandles[2] = { s_stop_event, s_audio_event };
    float resample_phase = 0.0f;
    int16_t sample_curr = 0;
    int16_t sample_next = 0;

    bool is_float = false;
    if (s_pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = true;
    } else if (s_pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *pExt = (WAVEFORMATEXTENSIBLE*)s_pwfx;
        if (IsEqualGUID(pExt->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            is_float = true;
        }
    }

    int channels = s_pwfx->nChannels;

    while (s_is_running) {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, 2000);
        if (waitResult == WAIT_OBJECT_0) {
            break; // Stop event signaled
        }

        UINT32 numPaddingFrames = 0;
        hr = s_pAudioClient->GetCurrentPadding(&numPaddingFrames);
        if (FAILED(hr)) continue;

        if (numPaddingFrames > s_bufferFrameCount) {
            numPaddingFrames = s_bufferFrameCount;
        }
        UINT32 numFramesAvailable = s_bufferFrameCount - numPaddingFrames;
        if (numFramesAvailable == 0) continue;

        BYTE *pData = NULL;
        hr = s_pRenderClient->GetBuffer(numFramesAvailable, &pData);
        if (FAILED(hr) || !pData) continue;

        DWORD flags = 0;
        float src_sample_rate = (float)s_src_sample_rate;
        if (src_sample_rate <= 0.0f) src_sample_rate = 8000.0f;
        float dst_sample_rate = (float)s_pwfx->nSamplesPerSec;
        float step = src_sample_rate / dst_sample_rate;

        for (UINT32 i = 0; i < numFramesAvailable; i++) {
            while (resample_phase >= 1.0f) {
                resample_phase -= 1.0f;
                sample_curr = sample_next;
                if (audio_ring_buffer_available(&s_render_ring_buf) > 0) {
                    audio_ring_buffer_pop(&s_render_ring_buf, &sample_next, 1);
                } else {
                    // Buffer underrun: smooth exponential decay towards zero
                    sample_next = (int16_t)((float)sample_curr * 0.85f);
                }
            }

            // High-precision linear interpolation between sample_curr and sample_next
            float interpolated = (float)sample_curr + ((float)sample_next - (float)sample_curr) * resample_phase;
            interpolated *= s_volume;
            resample_phase += step;

            if (is_float) {
                float *float_out = (float*)pData + (i * channels);
                float f_val = interpolated / 32768.0f;
                if (f_val > 1.0f) f_val = 1.0f;
                if (f_val < -1.0f) f_val = -1.0f;
                for (int c = 0; c < channels; c++) {
                    float_out[c] = f_val;
                }
            } else if (s_pwfx->wBitsPerSample == 16) {
                int16_t *pcm_out = (int16_t*)pData + (i * channels);
                int16_t s_val = audio_soft_clip(interpolated);
                for (int c = 0; c < channels; c++) {
                    pcm_out[c] = s_val;
                }
            } else if (s_pwfx->wBitsPerSample == 32) {
                int32_t *pcm32_out = (int32_t*)pData + (i * channels);
                int32_t s_val32 = (int32_t)audio_soft_clip(interpolated) << 16;
                for (int c = 0; c < channels; c++) {
                    pcm32_out[c] = s_val32;
                }
            } else if (s_pwfx->wBitsPerSample == 24) {
                uint8_t *pcm24_out = pData + (i * channels * 3);
                int32_t s_val24 = (int32_t)audio_soft_clip(interpolated) << 8;
                for (int c = 0; c < channels; c++) {
                    pcm24_out[c * 3 + 0] = (uint8_t)(s_val24 & 0xFF);
                    pcm24_out[c * 3 + 1] = (uint8_t)((s_val24 >> 8) & 0xFF);
                    pcm24_out[c * 3 + 2] = (uint8_t)((s_val24 >> 16) & 0xFF);
                }
            }
        }

        s_pRenderClient->ReleaseBuffer(numFramesAvailable, flags);
    }

    s_pAudioClient->Stop();
    s_pAudioClient->Reset();
    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    CoUninitialize();
    diag_log("[WASAPI_RENDER] Speaker render engine stopped");
    return 0;
}

extern "C" int wasapi_render_init(void) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    audio_ring_buffer_init(&s_render_ring_buf, 32000); // 2 seconds of 16kHz buffer

    s_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&s_pEnumerator);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: CoCreateInstance MMDeviceEnumerator failed: 0x%08lx", hr);
        return -1;
    }

    hr = s_pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &s_pDevice);
    if (FAILED(hr)) {
        hr = s_pEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &s_pDevice);
    }
    if (FAILED(hr)) {
        hr = s_pEnumerator->GetDefaultAudioEndpoint(eRender, eCommunications, &s_pDevice);
    }
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: GetDefaultAudioEndpoint failed: 0x%08lx", hr);
        return -2;
    }

    // Log the active render device name
    IPropertyStore *pProps = NULL;
    if (SUCCEEDED(s_pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
            diag_log("[WASAPI_RENDER] Selected Playback Device: %ls", varName.pwszVal);
            PropVariantClear(&varName);
        }
        pProps->Release();
    }

    hr = s_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&s_pAudioClient);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: Activate IAudioClient failed: 0x%08lx", hr);
        return -3;
    }

    hr = s_pAudioClient->GetMixFormat(&s_pwfx);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: GetMixFormat failed: 0x%08lx", hr);
        return -4;
    }

    REFERENCE_TIME hnsRequestedDuration = 40 * REFTIMES_PER_MILLISEC; // 40ms buffer
    hr = s_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsRequestedDuration,
        0,
        s_pwfx,
        NULL
    );
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: Initialize AudioClient failed: 0x%08lx", hr);
        return -5;
    }

    hr = s_pAudioClient->SetEventHandle(s_audio_event);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: SetEventHandle failed: 0x%08lx", hr);
        return -6;
    }

    hr = s_pAudioClient->GetBufferSize(&s_bufferFrameCount);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: GetBufferSize failed: 0x%08lx", hr);
        return -7;
    }

    hr = s_pAudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&s_pRenderClient);
    if (FAILED(hr)) {
        diag_log("[WASAPI_RENDER] ERROR: GetService IAudioRenderClient failed: 0x%08lx", hr);
        return -8;
    }

    bool is_float = false;
    if (s_pwfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        is_float = true;
    } else if (s_pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE *pExt = (WAVEFORMATEXTENSIBLE*)s_pwfx;
        if (IsEqualGUID(pExt->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            is_float = true;
        }
    }

    diag_log("[WASAPI_RENDER] Initialized Speaker playback: %u Hz, %u channels, %u bits, is_float=%d (Buffer: %u frames)",
             s_pwfx->nSamplesPerSec, s_pwfx->nChannels, s_pwfx->wBitsPerSample, (int)is_float, s_bufferFrameCount);
    return 0;
}

extern "C" int wasapi_render_start(void) {
    if (s_is_running) return 0;
    audio_ring_buffer_clear(&s_render_ring_buf);
    s_is_running = true;
    ResetEvent(s_stop_event);
    s_thread_handle = CreateThread(NULL, 0, render_worker_thread, NULL, 0, NULL);
    return s_thread_handle != NULL ? 0 : -1;
}

extern "C" void wasapi_render_stop(void) {
    if (!s_is_running) return;
    s_is_running = false;
    SetEvent(s_stop_event);
    if (s_thread_handle) {
        WaitForSingleObject(s_thread_handle, 1000);
        CloseHandle(s_thread_handle);
        s_thread_handle = NULL;
    }
    if (s_pAudioClient) {
        s_pAudioClient->Stop();
        s_pAudioClient->Reset();
    }
}

extern "C" void wasapi_render_push_samples(const int16_t *samples, int count) {
    audio_ring_buffer_push(&s_render_ring_buf, samples, count);
}

extern "C" void wasapi_render_set_source_sample_rate(uint32_t sample_rate) {
    s_src_sample_rate = sample_rate ? sample_rate : 8000;
}

extern "C" void wasapi_render_set_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 4.0f) volume = 4.0f;
    s_volume = volume;
}

extern "C" void wasapi_render_shutdown(void) {
    wasapi_render_stop();
    if (s_pRenderClient) { s_pRenderClient->Release(); s_pRenderClient = NULL; }
    if (s_pAudioClient) { s_pAudioClient->Release(); s_pAudioClient = NULL; }
    if (s_pDevice) { s_pDevice->Release(); s_pDevice = NULL; }
    if (s_pEnumerator) { s_pEnumerator->Release(); s_pEnumerator = NULL; }
    if (s_pwfx) { CoTaskMemFree(s_pwfx); s_pwfx = NULL; }
    if (s_stop_event) { CloseHandle(s_stop_event); s_stop_event = NULL; }
    if (s_audio_event) { CloseHandle(s_audio_event); s_audio_event = NULL; }
    audio_ring_buffer_free(&s_render_ring_buf);
}
