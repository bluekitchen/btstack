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
#define SCO_DEMO_MODE SCO_DEMO_MODE_ASCII
#define SCO_REPORT_PERIOD 100

#ifdef HAVE_POSIX_FILE_IO
#define SCO_WAV_FILENAME "sco_input.wav"
#define SCO_WAV_DURATION_IN_SECONDS 30
#endif


#if defined(HAVE_PORTAUDIO) && (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE)
#define USE_PORTAUDIO
#endif

#ifdef USE_PORTAUDIO
#include <portaudio.h>
// portaudio config
#define NUM_CHANNELS 1
#define SAMPLE_RATE 8000
#define FRAMES_PER_BUFFER 24
#define PA_SAMPLE_TYPE paInt8
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
static int count_sent = 0;
static int count_received = 0;

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME

static FILE * wav_file;
static int num_samples_to_write;


static void little_endian_fstore_16(FILE * file, uint16_t value){
    uint8_t buf[2];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 2, file);
}

static void little_endian_fstore_32(FILE * file, uint32_t value){
    uint8_t buf[4];
    little_endian_store_32(buf, 0, value);
    fwrite(&buf, 1, 4, file);
}

static FILE * wav_init(const char * filename){
    printf("SCO Demo: creating wav file %s\n", filename);
    return fopen(filename, "wb");
}

static void write_wav_header(FILE * file, int sample_rate, int num_channels, int num_samples, int bytes_per_sample){
    printf("SCO Demo: writing wav header: sample rate %u, num channels %u, duration %u s, bytes per sample %u\n",
        sample_rate, num_channels, num_samples / sample_rate / num_channels, bytes_per_sample);
    
    /* write RIFF header */
    fwrite("RIFF", 1, 4, file);
    // num_samples = blocks * subbands
    uint32_t data_bytes = (uint32_t) (bytes_per_sample * num_samples * num_channels);
    little_endian_fstore_32(file, data_bytes + 36); 
    fwrite("WAVE", 1, 4, file);

    int byte_rate = sample_rate * num_channels * bytes_per_sample;
    int bits_per_sample = 8 * bytes_per_sample;
    int block_align = num_channels * bits_per_sample;
    int fmt_length = 16;
    int fmt_format_tag = 1; // PCM

    /* write fmt chunk */
    fwrite("fmt ", 1, 4, file);
    little_endian_fstore_32(file, fmt_length);
    little_endian_fstore_16(file, fmt_format_tag);
    little_endian_fstore_16(file, num_channels);
    little_endian_fstore_32(file, sample_rate);
    little_endian_fstore_32(file, byte_rate);
    little_endian_fstore_16(file, block_align);   
    little_endian_fstore_16(file, bits_per_sample);
    
    /* write data chunk */
    fwrite("data", 1, 4, file); 
    little_endian_fstore_32(file, data_bytes);
}

static void write_wav_data_uint8(FILE * file, unsigned long num_samples, uint8_t * data){
    fwrite(data, num_samples, 1, file);
}

#endif
#endif

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

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    wav_file = wav_init(SCO_WAV_FILENAME);
    const int sample_rate = 8000;
    const int num_samples = sample_rate * SCO_WAV_DURATION_IN_SECONDS;
    const int num_channels = 1;
    const int bytes_per_sample = 1;
    write_wav_header(wav_file, sample_rate, num_channels, num_samples, bytes_per_sample);
    num_samples_to_write = num_samples;
#endif
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

static void sco_report(void){
    printf("SCO: sent %u, received %u\n", count_sent, count_received);
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
    int j;
    for (j=0;j<frames_per_packet;j++){
        sco_packet[3+j] = phase++;
    }
#endif
#endif
    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

    count_sent++;
    if ((count_sent % SCO_REPORT_PERIOD) == 0) sco_report();
}

/**
 * @brief Process received data
 */
void sco_demo_receive(uint8_t * packet, uint16_t size){


    int dump_data = 1;

    count_received++;
    // if ((count_received % SCO_REPORT_PERIOD) == 0) sco_report();


#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    if (num_samples_to_write){
        const int num_samples = size - 3;
        const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
        // convert 8 bit signed to 8 bit unsigned
        int i;
        for (i=0;i<samples_to_write;i++){
            packet[3+i] += 128;            
        }
        write_wav_data_uint8(wav_file, samples_to_write, &packet[3]);
        num_samples_to_write -= samples_to_write;
        if (num_samples_to_write == 0){
            printf("SCO Demo: closing wav file\n");
            fclose(wav_file);
        }
        dump_data = 0;
    }
#endif
#endif

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
    dump_data = 0;
#endif
#endif

    if (dump_data){
        printf("data: ");
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
        int i;
        for (i=3;i<size;i++){
            printf("%c", packet[i]);
        }
        printf("\n");
        dump_data = 0;
#else
        printf_hexdump(&packet[3], size-3);
#endif
    }
}
