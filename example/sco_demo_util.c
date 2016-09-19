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


#include <stdio.h>

#include "sco_demo_util.h"
#include "btstack_debug.h"
#include "btstack_sbc.h"
#include "btstack_cvsd_plc.h"
#include "hfp_msbc.h"
#include "hfp.h"

#include "wav_util.h"

// configure test mode
#define SCO_DEMO_MODE_SINE		0
#define SCO_DEMO_MODE_ASCII		1
#define SCO_DEMO_MODE_COUNTER	2


// SCO demo configuration
#define SCO_DEMO_MODE SCO_DEMO_MODE_SINE
#define SCO_REPORT_PERIOD 100

#ifdef HAVE_POSIX_FILE_IO
#define SCO_WAV_FILENAME      "sco_input.wav"
#define SCO_MSBC_OUT_FILENAME "sco_output.msbc"
#define SCO_MSBC_IN_FILENAME  "sco_input.mscb"

#define SCO_WAV_DURATION_IN_SECONDS 15
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


static int dump_data = 1;

static int count_sent = 0;
static int count_received = 0;
static uint8_t negotiated_codec = 0; 
static int num_audio_frames = 0;

FILE * msbc_file_in;
FILE * msbc_file_out;

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE


#ifdef SCO_WAV_FILENAME

static int num_samples_to_write;

static btstack_sbc_decoder_state_t decoder_state;
static btstack_cvsd_plc_state_t cvsd_plc_state;

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    log_info("handle_pcm_data num samples %u / %u", num_samples, num_samples_to_write);
    if (!num_samples_to_write) return;
    
    num_samples = btstack_min(num_samples, num_samples_to_write);
    num_samples_to_write -= num_samples;

    wav_writer_write_int16(num_samples, data);

    if (num_samples_to_write == 0){
        sco_demo_close();
    }
}

static void sco_demo_fill_audio_frame(void){
    if (!hfp_msbc_can_encode_audio_frame_now()) return;
    int num_samples = hfp_msbc_num_audio_samples_per_frame();
    int16_t sample_buffer[num_samples];
    wav_synthesize_sine_wave_int16(num_samples, sample_buffer);
    hfp_msbc_encode_audio_frame(sample_buffer);
    num_audio_frames++;
}

static void sco_demo_init_mSBC(void){
    int sample_rate = 16000;
    wav_writer_open(SCO_WAV_FILENAME, 1, sample_rate);
    btstack_sbc_decoder_init(&decoder_state, SBC_MODE_mSBC, &handle_pcm_data, NULL);    

    num_samples_to_write = sample_rate * SCO_WAV_DURATION_IN_SECONDS;
    
    hfp_msbc_init();
    sco_demo_fill_audio_frame();

#ifdef SCO_MSBC_IN_FILENAME
    msbc_file_in = fopen(SCO_MSBC_IN_FILENAME, "wb");
    printf("SCO Demo: creating mSBC in file %s, %p\n", SCO_MSBC_IN_FILENAME, msbc_file_in);
#endif   
#ifdef SCO_MSBC_OUT_FILENAME
    msbc_file_out = fopen(SCO_MSBC_OUT_FILENAME, "wb");
    printf("SCO Demo: creating mSBC out file %s, %p\n", SCO_MSBC_OUT_FILENAME, msbc_file_out);
#endif   
}

static void sco_demo_receive_mSBC(uint8_t * packet, uint16_t size){
    if (num_samples_to_write){
        if (msbc_file_in){
            // log incoming mSBC data for testing
            fwrite(packet+3, size-3, 1, msbc_file_in);
        }
        btstack_sbc_decoder_process_data(&decoder_state, (packet[1] >> 4) & 3, packet+3, size-3);
        dump_data = 0;
    }
}

static void sco_demo_init_CVSD(void){
    int sample_rate = 8000;
    wav_writer_open(SCO_WAV_FILENAME, 1, sample_rate);
    btstack_cvsd_plc_init(&cvsd_plc_state);
    num_samples_to_write = sample_rate * SCO_WAV_DURATION_IN_SECONDS;
}

static void sco_demo_receive_CVSD(uint8_t * packet, uint16_t size){
    if (num_samples_to_write){
        const int num_samples = size - 3;
        const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
        int8_t audio_frame_out[24];
        
        // memcpy(audio_frame_out, (int8_t*)(packet+3), 24);
        btstack_cvsd_plc_process_data(&cvsd_plc_state, (int8_t *)(packet+3), num_samples, audio_frame_out);

        wav_writer_write_int8(samples_to_write, audio_frame_out);
        num_samples_to_write -= samples_to_write;
        if (num_samples_to_write == 0){
            sco_demo_close();
        }
        dump_data = 0;
    }
}

#endif
#endif

void sco_demo_close(void){    
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#if defined(SCO_WAV_FILENAME) || defined(SCO_SBC_FILENAME)
    wav_writer_close();
    printf("SCO demo statistics: ");
    if (negotiated_codec == HFP_CODEC_MSBC){
        printf("Used mSBC with PLC, number of processed frames: \n - %d good frames, \n - %d zero frames, \n - %d bad frames.", decoder_state.good_frames_nr, decoder_state.zero_frames_nr, decoder_state.bad_frames_nr);
    } else {
        printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
    }
#endif
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    
#if 0
    printf("SCO Demo: closing wav file\n");
    if (negotiated_codec == HFP_CODEC_MSBC){
        wav_writer_state_t * writer_state = &wav_writer_state;
        if (!writer_state->wav_file) return;
        rewind(writer_state->wav_file);
        write_wav_header(writer_state->wav_file, writer_state->total_num_samples, btstack_sbc_decoder_num_channels(&decoder_state), btstack_sbc_decoder_sample_rate(&decoder_state),2);
        fclose(writer_state->wav_file);
        writer_state->wav_file = NULL;
    }
#endif
#endif
#endif
}

void sco_demo_set_codec(uint8_t codec){
    if (negotiated_codec == codec) return;
    negotiated_codec = codec;
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#if defined(SCO_WAV_FILENAME) || defined(SCO_SBC_FILENAME)
    if (negotiated_codec == HFP_CODEC_MSBC){
        sco_demo_init_mSBC();
    } else {
        sco_demo_init_CVSD();
    }
#endif
#endif
}

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
    const int audio_samples_per_packet = sco_payload_length;    // for 8-bit data. for 16-bit data it's /2

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    if (negotiated_codec == HFP_CODEC_MSBC){

        if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
            log_error("mSBC stream is empty.");
        }
        hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
        if (msbc_file_out){
            // log outgoing mSBC data for testing
            fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
        }

        sco_demo_fill_audio_frame();
    } else {
        wav_synthesize_sine_wave_int8(audio_samples_per_packet, (int8_t *) (sco_packet+3));
    }
#else
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    memset(&sco_packet[3], phase++, audio_samples_per_packet);
    if (phase > 'z') phase = 'a';
#else
    int j;
    for (j=0;j<audio_samples_per_packet;j++){
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

    dump_data = 1;

    count_received++;
    // if ((count_received % SCO_REPORT_PERIOD) == 0) sco_report();


#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    if (negotiated_codec == HFP_CODEC_MSBC){
        sco_demo_receive_mSBC(packet, size);
    } else {
        sco_demo_receive_CVSD(packet, size);
    }
#endif
#endif

    if (packet[1] & 0x30){
        printf("SCO CRC Error: %x - data: ", (packet[1] & 0x30) >> 4);
        log_info("SCO CRC Error: %x - data: ", (packet[1] & 0x30) >> 4);
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
