#include "hal_audio_dma.h"
#include "string.h"
#include "hxcmod.h"
#include "mods/mod.h"
#include "btstack_config.h"

#ifndef HAVE_AUDIO_DMA
#error "MOD-Player requires HAVE_AUDIO_DMA implementation, please enable in btstack_config.h"
#endif

#define NUM_SAMPLES 100
static uint16_t audio_samples1[NUM_SAMPLES*2];
static uint16_t audio_samples2[NUM_SAMPLES*2];
static volatile int active_buffer;
static int hxcmod_initialized = 0;
static modcontext mod_context;

void audio_transfer_complete(void){
	if (active_buffer){
		hal_audio_dma_play((uint8_t*) &audio_samples1[0], sizeof(audio_samples1));
		active_buffer = 0;
	} else {
		hal_audio_dma_play((uint8_t*)&audio_samples2[0], sizeof(audio_samples2));
		active_buffer = 1;
	}
}

void mod_player(void){

	hal_audio_dma_init(44100);
	hal_audio_dma_set_audio_played(&audio_transfer_complete);

	hxcmod_initialized = hxcmod_init(&mod_context);
    if (hxcmod_initialized){
        hxcmod_setcfg(&mod_context, 44100, 16, 1, 1, 1);
        hxcmod_load(&mod_context, (void *) &mod_data, mod_len);
    }

	active_buffer = 0;
	hxcmod_fillbuffer(&mod_context, (unsigned short *) &audio_samples1[0], NUM_SAMPLES, NULL);
	hal_audio_dma_play((uint8_t*) &audio_samples1[0], sizeof(audio_samples1));

	while (1){
		hxcmod_fillbuffer(&mod_context, (unsigned short *) &audio_samples2[0], NUM_SAMPLES, NULL);
		while (active_buffer == 0){
			__asm__("wfe");
		}
		hxcmod_fillbuffer(&mod_context, (unsigned short *) &audio_samples1[0], NUM_SAMPLES, NULL);
		while (active_buffer == 1){
			__asm__("wfe");
		}
	}
}
