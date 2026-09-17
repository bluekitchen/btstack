#ifndef WASAPI_RENDER_H
#define WASAPI_RENDER_H

#include "audio_render.h"

#define wasapi_render_init                audio_render_init
#define wasapi_render_start               audio_render_start
#define wasapi_render_stop                audio_render_stop
#define wasapi_render_push_samples        audio_render_push_samples
#define wasapi_render_set_source_sample_rate audio_render_set_source_sample_rate
#define wasapi_render_set_volume          audio_render_set_volume
#define wasapi_render_shutdown            audio_render_shutdown

#endif // WASAPI_RENDER_H
