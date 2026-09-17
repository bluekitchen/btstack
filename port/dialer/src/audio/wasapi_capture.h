#ifndef WASAPI_CAPTURE_H
#define WASAPI_CAPTURE_H

#include "audio_capture.h"

typedef audio_capture_pcm_callback_t wasapi_capture_pcm_callback_t;

#define wasapi_capture_init                audio_capture_init
#define wasapi_capture_start               audio_capture_start
#define wasapi_capture_stop                audio_capture_stop
#define wasapi_capture_set_target_sample_rate audio_capture_set_target_sample_rate
#define wasapi_capture_set_gain            audio_capture_set_gain
#define wasapi_capture_shutdown            audio_capture_shutdown

#endif // WASAPI_CAPTURE_H
