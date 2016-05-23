/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
/*
 * sco_demo_util.c - send/receive test data via SCO, used by hfp_*_demo and hsp_*_demo
 */

#include "sco_demo_util.h"
#include <stdio.h>

// configure test mode
#define SCO_DEMO_MODE_SINE		0
#define SCO_DEMO_MODE_ASCII		1
#define SCO_DEMO_MODE_COUNTER	2

// SCO demo configuration
#define SCO_DEMO_MODE SCO_DEMO_MODE_SINE
#define SCO_REPORT_PERIOD 100

// portaudio config
#define NUM_CHANNELS 1
#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 24
#define PA_SAMPLE_TYPE paInt8

#if defined(HAVE_PORTAUDIO) && (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE)
#define USE_PORTAUDIO
#endif


#ifdef USE_PORTAUDIO
#include <portaudio.h>
// portaudio globals
static  PaStream * stream;
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
// input signal: pre-computed sine wave, 160 Hz
static const uint8_t sine[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};
#endif

static int phase;

void sco_demo_init(void){

	// status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef HAVE_PORTAUDIO
	printf("SCO Demo: Sending sine wave, audio output via portaudio.\n");
#else
	printf("SCO Demo: Sending sine wave, hexdump received data.\n");
#endif
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
	printf("SCO Demo: Sending ASCII blocks, print received data.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
	printf("SCO Demo: Sending counter value, hexdump received data.\n");
#endif

#ifdef USE_PORTAUDIO
    int err;
    PaStreamParameters outputParameters;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if( err != paNoError ) return;
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL, // &inputParameters,
           &outputParameters,
           SAMPLE_RATE,
           FRAMES_PER_BUFFER,
           paClipOff, /* we won't output out of range samples so don't bother clipping them */
           NULL, 	  /* no callback, use blocking API */
           NULL ); 	  /* no callback, so no callback userData */
    if( err != paNoError ) return;
    /* -- start stream -- */
    err = Pa_StartStream( stream );
    if( err != paNoError ) return;
#endif	

#if SCO_DEMO_MODE != SCO_DEMO_MODE_SINE
    hci_set_sco_voice_setting(0x03);    // linear, unsigned, 8-bit, transparent
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    phase = 'a';
#endif
}


void sco_demo_send(hci_con_handle_t sco_handle){

    if (!sco_handle) return;
    
    const int sco_packet_length = 24 + 3; // hci_get_sco_packet_length();
    const int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    const int frames_per_packet = sco_payload_length;    // for 8-bit data. for 16-bit data it's /2

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    int i;
    for (i=0;i<frames_per_packet;i++){
        sco_packet[3+i] = sine[phase];
        phase++;
        if (phase >= sizeof(sine)) phase = 0;
    }
#else
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    memset(&sco_packet[3], phase++, frames_per_packet);
    if (phase > 'z') phase = 'a';
#else
    for (i=0;i<frames_per_packet;i++){
        sco_packet[3+i] = phase++;
    }
#endif
#endif
    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

    static int count = 0;
    count++;
    if ((count % SCO_REPORT_PERIOD) == 0) printf("SCO: sent %u\n", count);
}

/**
 * @brief Process received data
 */
void sco_demo_receive(uint8_t * packet, uint16_t size){

	if (packet[1] & 0xf0){
	    printf("SCO CRC Error: %x - data: ", packet[1] >> 4);
	    printf_hexdump(&packet[3], size-3);
	    return;
	}

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef USE_PORTAUDIO
    uint32_t start = btstack_run_loop_get_time_ms();
    Pa_WriteStream( stream, &packet[3], size -3);
    uint32_t end   = btstack_run_loop_get_time_ms();
    if (end - start > 5){
        printf("Portaudio: write stream took %u ms\n", end - start);
    }
#else
    printf_hexdump(&packet[3], size-3);
#endif
#else
    printf("data: ");
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    int i;
    for (i=3;i<size;i++){
        printf("%c", packet[i]);
    }
    printf("\n");
#endif
    printf_hexdump(&packet[3], size-3);
#endif

    static int count = 0;
    count++;
    if ((count % SCO_REPORT_PERIOD) == 0) printf("SCO: received %u\n", count);
}
