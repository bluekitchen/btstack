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
#include "wasapi_capture.h"
#include "diag_logger.h"
}
#include "wasapi_common.h"

#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

static HANDLE s_thread_handle = NULL;
static HANDLE s_stop_event = NULL;
static HANDLE s_audio_event = NULL;
static bool s_is_running = false;
static volatile uint32_t s_target_sample_rate = 8000;
static float s_mic_gain = 1.2f; // Calibrated digital gain (1.2x) for balanced microphone audio

static IMMDeviceEnumerator *s_pEnumerator = NULL;
static IMMDevice *s_pDevice = NULL;
static IAudioClient *s_pAudioClient = NULL;
static IAudioCaptureClient *s_pCaptureClient = NULL;
static WAVEFORMATEX *s_pwfx = NULL;

static wasapi_capture_pcm_callback_t s_capture_cb = NULL;

static DWORD WINAPI capture_worker_thread(LPVOID lpParam) {
    UNUSED(lpParam);

    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (!s_pAudioClient || !s_pCaptureClient || !s_pwfx) {
        diag_log("[WASAPI_CAPTURE] Capture thread failed: AudioClient or CaptureClient is NULL");
        CoUninitialize();
        return 0;
    }

    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

    s_pAudioClient->Reset();

    HRESULT hr = s_pAudioClient->Start();
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: IAudioClient::Start failed: 0x%08lx", hr);
    } else {
        diag_log("[WASAPI_CAPTURE] Microphone capture engine started successfully (Device: %u Hz, Target: %u Hz)",
                 s_pwfx->nSamplesPerSec, s_target_sample_rate);
    }

    HANDLE waitHandles[2] = { s_stop_event, s_audio_event };
    float resample_phase = 0.0f;
    float mono_accum = 0.0f;
    int accum_count = 0;

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

        UINT32 packetLength = 0;
        hr = s_pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) continue;

        float target_rate = (float)s_target_sample_rate;
        if (target_rate <= 0.0f) target_rate = 8000.0f;
        float step = (float)s_pwfx->nSamplesPerSec / target_rate;

        while (packetLength > 0) {
            BYTE *pData = NULL;
            UINT32 numFramesAvailable = 0;
            DWORD flags = 0;

            hr = s_pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (FAILED(hr) || !pData) break;

            int16_t downsample_buffer[4096];
            int downsample_out_count = 0;

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                // Buffer is silent
                int out_needed = (int)((float)numFramesAvailable / step);
                if (out_needed > 4096) out_needed = 4096;
                memset(downsample_buffer, 0, out_needed * sizeof(int16_t));
                downsample_out_count = out_needed;
                mono_accum = 0.0f;
                accum_count = 0;
            } else {
                for (UINT32 i = 0; i < numFramesAvailable; i++) {
                    float mono_sample = 0.0f;

                    if (is_float) {
                        float *float_in = (float*)pData + (i * channels);
                        if (channels >= 2) {
                            mono_sample = ((float_in[0] + float_in[1]) * 0.5f) * 32767.0f;
                        } else {
                            mono_sample = float_in[0] * 32767.0f;
                        }
                    } else if (s_pwfx->wBitsPerSample == 16) {
                        int16_t *pcm16_in = (int16_t*)pData + (i * channels);
                        if (channels >= 2) {
                            mono_sample = ((float)pcm16_in[0] + (float)pcm16_in[1]) * 0.5f;
                        } else {
                            mono_sample = (float)pcm16_in[0];
                        }
                    } else if (s_pwfx->wBitsPerSample == 32) {
                        int32_t *pcm32_in = (int32_t*)pData + (i * channels);
                        if (channels >= 2) {
                            mono_sample = ((float)(pcm32_in[0] >> 16) + (float)(pcm32_in[1] >> 16)) * 0.5f;
                        } else {
                            mono_sample = (float)(pcm32_in[0] >> 16);
                        }
                    } else if (s_pwfx->wBitsPerSample == 24) {
                        uint8_t *pcm24_in = pData + (i * channels * 3);
                        int32_t val0 = ((int32_t)pcm24_in[0]) | (((int32_t)pcm24_in[1]) << 8) | (((int32_t)(int8_t)pcm24_in[2]) << 16);
                        if (channels >= 2) {
                            int32_t val1 = ((int32_t)pcm24_in[3]) | (((int32_t)pcm24_in[4]) << 8) | (((int32_t)(int8_t)pcm24_in[5]) << 16);
                            mono_sample = ((float)(val0 >> 8) + (float)(val1 >> 8)) * 0.5f;
                        } else {
                            mono_sample = (float)(val0 >> 8);
                        }
                    }

                    mono_accum += mono_sample;
                    accum_count++;

                    resample_phase += 1.0f;
                    if (resample_phase >= step) {
                        resample_phase -= step;
                        float avg_sample = mono_accum / (float)(accum_count > 0 ? accum_count : 1);
                        mono_accum = 0.0f;
                        accum_count = 0;
                        if (downsample_out_count < 4096) {
                            downsample_buffer[downsample_out_count++] = audio_soft_clip(avg_sample * s_mic_gain);
                        }
                    }
                }
            }

            s_pCaptureClient->ReleaseBuffer(numFramesAvailable);

            if (s_capture_cb && downsample_out_count > 0) {
                s_capture_cb(downsample_buffer, downsample_out_count);
            }

            hr = s_pCaptureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }
    }

    s_pAudioClient->Stop();
    s_pAudioClient->Reset();
    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    CoUninitialize();
    diag_log("[WASAPI_CAPTURE] Microphone capture engine stopped");
    return 0;
}

extern "C" int wasapi_capture_init(wasapi_capture_pcm_callback_t pcm_callback) {
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    s_capture_cb = pcm_callback;

    s_stop_event = CreateEvent(NULL, TRUE, FALSE, NULL);
    s_audio_event = CreateEvent(NULL, FALSE, FALSE, NULL);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&s_pEnumerator);
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: CoCreateInstance MMDeviceEnumerator failed: 0x%08lx", hr);
        return -1;
    }

    // Use eCommunications or eConsole for microphone endpoint
    hr = s_pEnumerator->GetDefaultAudioEndpoint(eCapture, eCommunications, &s_pDevice);
    if (FAILED(hr)) {
        hr = s_pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &s_pDevice);
    }
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: GetDefaultAudioEndpoint failed: 0x%08lx", hr);
        return -2;
    }

    // Log the active capture device name
    IPropertyStore *pProps = NULL;
    if (SUCCEEDED(s_pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
        PROPVARIANT varName;
        PropVariantInit(&varName);
        if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
            diag_log("[WASAPI_CAPTURE] Selected Capture Device: %ls", varName.pwszVal);
            PropVariantClear(&varName);
        }
        pProps->Release();
    }

    hr = s_pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&s_pAudioClient);
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: Activate IAudioClient failed: 0x%08lx", hr);
        return -3;
    }

    hr = s_pAudioClient->GetMixFormat(&s_pwfx);
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: GetMixFormat failed: 0x%08lx", hr);
        return -4;
    }

    REFERENCE_TIME hnsRequestedDuration = 40 * REFTIMES_PER_MILLISEC; // 40ms
    hr = s_pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
        hnsRequestedDuration,
        0,
        s_pwfx,
        NULL
    );
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: Initialize AudioClient failed: 0x%08lx", hr);
        return -5;
    }

    hr = s_pAudioClient->SetEventHandle(s_audio_event);
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: SetEventHandle failed: 0x%08lx", hr);
        return -6;
    }

    hr = s_pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&s_pCaptureClient);
    if (FAILED(hr)) {
        diag_log("[WASAPI_CAPTURE] ERROR: GetService IAudioCaptureClient failed: 0x%08lx", hr);
        return -7;
    }

    diag_log("[WASAPI_CAPTURE] Initialized PC Microphone capture (%u Hz, %u channels)",
             s_pwfx->nSamplesPerSec, s_pwfx->nChannels);
    return 0;
}

extern "C" int wasapi_capture_start(void) {
    if (s_is_running) return 0;
    s_is_running = true;
    ResetEvent(s_stop_event);
    s_thread_handle = CreateThread(NULL, 0, capture_worker_thread, NULL, 0, NULL);
    return s_thread_handle != NULL ? 0 : -1;
}

extern "C" void wasapi_capture_stop(void) {
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

extern "C" void wasapi_capture_set_target_sample_rate(uint32_t sample_rate) {
    if (sample_rate == 8000 || sample_rate == 16000) {
        s_target_sample_rate = sample_rate;
        diag_log("[WASAPI_CAPTURE] Target capture sample rate set to %u Hz", sample_rate);
    }
}

extern "C" void wasapi_capture_set_gain(float gain) {
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 8.0f) gain = 8.0f;
    s_mic_gain = gain;
}

extern "C" void wasapi_capture_shutdown(void) {
    wasapi_capture_stop();
    if (s_pCaptureClient) { s_pCaptureClient->Release(); s_pCaptureClient = NULL; }
    if (s_pAudioClient) { s_pAudioClient->Release(); s_pAudioClient = NULL; }
    if (s_pDevice) { s_pDevice->Release(); s_pDevice = NULL; }
    if (s_pEnumerator) { s_pEnumerator->Release(); s_pEnumerator = NULL; }
    if (s_pwfx) { CoTaskMemFree(s_pwfx); s_pwfx = NULL; }
    if (s_stop_event) { CloseHandle(s_stop_event); s_stop_event = NULL; }
    if (s_audio_event) { CloseHandle(s_audio_event); s_audio_event = NULL; }
}
