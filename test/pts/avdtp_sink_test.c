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

#define BTSTACK_FILE__ "avdtp_sink_test.c"

/*
 * avdtp_sink_test.c : Tool for testig AVDTP sink with PTS, see avdtp_sink_test.md and a2dp_sink.md for PTS tests command sequences
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "wav_util.h"
#include "btstack_resample.h"
#include "btstack_ring_buffer.h"

#include "wav_util.h"

#ifdef HAVE_AAC_FDK
#include <fdk-aac/aacdecoder_lib.h>
#endif

#ifdef HAVE_APTX
#include <openaptx.h>
#endif
#define A2DP_CODEC_VENDOR_ID_APT_LTD 0x4f
#define A2DP_CODEC_VENDOR_ID_QUALCOMM 0xd7
#define A2DP_APT_LTD_CODEC_APTX 0x1
#define A2DP_QUALCOMM_CODEC_APTX_HD 0x24

#ifdef HAVE_LDAC_DECODER
#include <ldacdec/ldacdec.h>
#endif
#define A2DP_CODEC_VENDOR_ID_SONY 0x12d
#define A2DP_SONY_CODEC_LDAC 0xaa

#ifdef HAVE_LC3PLUS
#include <LC3plus/lc3.h>
#endif
#define A2DP_CODEC_VENDOR_ID_FRAUNHOFER 0x08A9
#define A2DP_FRAUNHOFER_CODEC_LC3PLUS 0x0001

//
// Configuration
//

#define NUM_CHANNELS 2
#define BYTES_PER_FRAME     (2*NUM_CHANNELS)

// Playback
#define SAMPLE_RATE 48000
#define PREBUFFER_MS        100
#define AUDIO_BUFFER_SIZE   (AUDIO_BUFFER_MS*SAMPLE_RATE/BYTES_PER_FRAME)
#define AUDIO_BUFFER_MS     200

#define MAX_SBC_FRAME_SIZE 120

// ring buffer for SBC Frames
// below 30: add samples, 30-40: fine, above 40: drop samples
#define OPTIMAL_FRAMES_MIN 30
#define OPTIMAL_FRAMES_MAX 40
#define ADDITIONAL_FRAMES  20

static const char * wav_filename = "avdtp_sink_test.wav";

typedef struct {
    // bitmaps
    uint8_t sampling_frequency_bitmap;
    uint8_t channel_mode_bitmap;
    uint8_t block_length_bitmap;
    uint8_t subbands_bitmap;
    uint8_t allocation_method_bitmap;
    uint8_t min_bitpool_value;
    uint8_t max_bitpool_value;
} adtvp_media_codec_information_sbc_t;

typedef struct {
    int reconfigure;
    int num_channels;
    int sampling_frequency;
    int channel_mode;
    int block_length;
    int subbands;
    int allocation_method;
    int min_bitpool_value;
    int max_bitpool_value;
} avdtp_media_codec_configuration_sbc_t;

typedef struct {
    int reconfigure;
    int sampling_frequency;
    int aot;
    int bitrate;
    int channel_mode;
    int vbr;
} avdtp_media_codec_configuration_aac_t;

typedef struct {
    int reconfigure;
    int channel_mode;
    int num_channels;
    int sampling_frequency;
} avdtp_media_codec_configuration_aptx_t;

typedef struct {
    int reconfigure;
    int channel_mode;
    int num_channels;
    int num_samples;
    int sampling_frequency;
} avdtp_media_codec_configuration_ldac_t;

typedef struct {
    int reconfigure;
    int frame_duration;
    int num_channels;
    int sampling_frequency;
    int bitrate;
    int hrmode;
} avdtp_media_codec_configuration_lc3plus_t;


typedef struct {
    uint8_t  num_channels;
    uint32_t sampling_frequency;
} playback_configuration_t;

// REMOTE Device
// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// bt dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
static const char * device_addr_string = "DC:A6:32:62:1C:DD";
static bd_addr_t device_addr;

static uint8_t  sdp_avdtp_sink_service_buffer[150];

// stack stuff
static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint16_t avdtp_cid;
static uint8_t  local_seid;
static uint8_t  remote_seid;

static uint8_t is_cmd_triggered_locally = 0;
static uint8_t is_media_header_reported_once = 0;

// playback
static int  playback_initialized = 0;
static bool playback_active = false;
static playback_configuration_t playback_configuration;

// large audio buffer to store up to AUDIO_MS of audio data
static uint8_t decoded_audio_storage[AUDIO_BUFFER_SIZE];
static btstack_ring_buffer_t decoded_audio_ring_buffer;

// active codec
static avdtp_media_codec_type_t codec_type;

// SBC
static btstack_sbc_decoder_state_t state;
static btstack_sbc_mode_t mode = SBC_MODE_STANDARD;
static adtvp_media_codec_information_sbc_t   sbc_capability;
static avdtp_media_codec_configuration_sbc_t sbc_configuration;
static uint8_t local_seid_sbc;

// AAC
#ifdef HAVE_AAC_FDK
// AAC handle
static HANDLE_AACDECODER aac_handle;
static avdtp_media_codec_configuration_aac_t aac_configuration;
static uint8_t local_seid_aac;
#endif

// APTX
#ifdef HAVE_APTX
static struct aptx_context *aptx_handle;
static avdtp_media_codec_configuration_aptx_t aptx_configuration;
static uint8_t local_seid_aptx;
static avdtp_media_codec_configuration_aptx_t aptxhd_configuration;
static uint8_t local_seid_aptxhd;
#endif

// LDAC
#ifdef HAVE_LDAC_DECODER
static ldacdec_t ldac_handle;
static avdtp_media_codec_configuration_ldac_t ldac_configuration;
static uint8_t local_seid_ldac;
#endif

// LC3PLUS
#ifdef HAVE_LC3PLUS
LC3PLUS_Dec * lc3plus_handle = NULL;
static uint8_t * lc3plus_scratch = NULL;
static avdtp_media_codec_configuration_lc3plus_t lc3plus_configuration;
static uint8_t local_seid_lc3plus;
#endif

static uint32_t vendor_id;
static uint16_t codec_id;

static uint16_t remote_configuration_bitmap;
static avdtp_capabilities_t remote_configuration;
static bool delay_reporting_supported_on_remote = false;

static uint8_t num_remote_seids;
static uint8_t first_remote_seid;

// prototypes
static int read_media_data_header(uint8_t * packet, uint16_t size, uint16_t * offset, avdtp_media_packet_header_t * media_header);
static int read_sbc_header(uint8_t * packet, uint16_t size, uint16_t * offset, avdtp_sbc_codec_header_t * sbc_header);

// Playback: PCM ring buffer
static void playback_handler(int16_t * buffer, uint16_t num_audio_frames){
    int wav_samples = num_audio_frames * playback_configuration.num_channels;

    uint32_t bytes_available = btstack_ring_buffer_bytes_available(&decoded_audio_ring_buffer);
    uint32_t bytes_requested = wav_samples * 2;
    uint32_t bytes_start = playback_configuration.sampling_frequency * PREBUFFER_MS / 1000 * playback_configuration.num_channels;
    bool sufficient_data_now =   bytes_available > bytes_requested;
    bool sufficient_data_start = bytes_available > bytes_start;

    // check for underrun
    if (playback_active && !sufficient_data_now){
        // printf("Playback: Underrun, pause (have %u, need %u\n", bytes_available, bytes_requested);
        playback_active = false;
    }

    // check for start
    if (!playback_active && sufficient_data_start){
        // printf("Playback: start - have %u, start %u\n", bytes_available, bytes_start);
        playback_active = true;
    }

    // play silence
    if (!playback_active){
        playback_active = false;
        memset(buffer, 0, num_audio_frames * BYTES_PER_FRAME);
        return;
    }

    // read data from ring buffer
    uint32_t bytes_read;
    btstack_ring_buffer_read(&decoded_audio_ring_buffer, (uint8_t *) buffer, num_audio_frames * BYTES_PER_FRAME, &bytes_read);
}

static int playback_init(playback_configuration_t * configuration){
    if (playback_initialized) return 0;
    playback_initialized = 1;

    playback_active = false;
    btstack_ring_buffer_init(&decoded_audio_ring_buffer, decoded_audio_storage, sizeof(decoded_audio_storage));

    // wav writer
    printf("WAV Writer: open %s\n", wav_filename);
    wav_writer_open(wav_filename, configuration->num_channels, configuration->sampling_frequency);

    // setup audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        printf("Playback: start stream\n");
        audio->init(NUM_CHANNELS, configuration->sampling_frequency, &playback_handler);
        audio->start_stream();
    }

    return 0;
}

static void playback_start(void){
}

static void playback_pause(void){
    if (!playback_initialized) return;
    // discard audio, play silence
    playback_active = false;
    btstack_ring_buffer_init(&decoded_audio_ring_buffer, decoded_audio_storage, sizeof(decoded_audio_storage));
}

static void playback_close(void){
    if (!playback_initialized) return;
    playback_initialized = 0;

    playback_active = false;

    // close wav
    printf("WAV Writer: close %s\n", wav_filename);
    wav_writer_close();

    // stop audio playback
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio){
        printf("PlaybackL: stop stream\n");
        audio->close();
    }
}

static void playback_queue_audio(int16_t * data, int num_audio_frames, int num_channels){

    // if (playback_active == false) {
    //     return;
    // }

    // write to wav file
    wav_writer_write_int16(num_audio_frames * num_channels, data);

    // do not write into buffer if audio is not present
    const btstack_audio_sink_t * audio = btstack_audio_sink_get_instance();
    if (audio == NULL){
        return;
    }

    // store in audio ring buffer
    int status = btstack_ring_buffer_write(&decoded_audio_ring_buffer, (uint8_t *)data, num_audio_frames * num_channels * 2);
    if (status){
        printf("Error storing samples in PCM ring buffer!!!\n");
    }
    log_info("buffer: store %u frames, have %u bytes", num_audio_frames, btstack_ring_buffer_bytes_available(&decoded_audio_ring_buffer));
}

//
// SBC Codec
//

static void dump_sbc_capability(adtvp_media_codec_information_sbc_t capabilities){
    printf("    - sampling_frequency: 0x%02x\n", capabilities.sampling_frequency_bitmap);
    printf("    - channel_mode: 0x%02x\n", capabilities.channel_mode_bitmap);
    printf("    - block_length: 0x%02x\n", capabilities.block_length_bitmap);
    printf("    - subbands: 0x%02x\n", capabilities.subbands_bitmap);
    printf("    - allocation_method: 0x%02x\n", capabilities.allocation_method_bitmap);
    printf("    - bitpool_value [%d, %d] \n", capabilities.min_bitpool_value, capabilities.max_bitpool_value);
}

static void dump_sbc_configuration(avdtp_media_codec_configuration_sbc_t configuration){
    printf("    - num_channels: %d\n", configuration.num_channels);
    printf("    - sampling_frequency: %d\n", configuration.sampling_frequency);
    printf("    - channel_mode: %d\n", configuration.channel_mode);
    printf("    - block_length: %d\n", configuration.block_length);
    printf("    - subbands: %d\n", configuration.subbands);
    printf("    - allocation_method: %d\n", configuration.allocation_method);
    printf("    - bitpool_value [%d, %d] \n", configuration.min_bitpool_value, configuration.max_bitpool_value);
}

static int read_sbc_header(uint8_t * packet, uint16_t size, uint16_t * offset, avdtp_sbc_codec_header_t * sbc_header){
    int sbc_header_len = 1; // without crc
    int pos = *offset;
    
    if (size - pos < sbc_header_len){
        printf("Not enough data to read SBC header, expected %d, received %d\n", sbc_header_len, size-pos);
        return 0;
    }

    sbc_header->fragmentation = get_bit16(packet[pos], 7);
    sbc_header->starting_packet = get_bit16(packet[pos], 6);
    sbc_header->last_packet = get_bit16(packet[pos], 5);
    sbc_header->num_frames = packet[pos] & 0x0f;
    pos++;
    // printf("SBC HEADER: num_frames %u, fragmented %u, start %u, stop %u\n", sbc_header.num_frames, sbc_header.fragmentation, sbc_header.starting_packet, sbc_header.last_packet);
    *offset = pos;
    return 1;
}

static void sbc_handle_pcm_data(int16_t * data, int num_audio_frames, int num_channels, int sample_rate, void * context){
    UNUSED(sample_rate);
    UNUSED(context);
    playback_queue_audio(data, num_audio_frames, num_channels);
}

static void handle_l2cap_media_data_packet_sbc(uint8_t *packet, uint16_t size){
    uint16_t pos = 0;

    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;

    avdtp_sbc_codec_header_t sbc_header;
    if (!read_sbc_header(packet, size, &pos, &sbc_header)) return;

    btstack_sbc_decoder_process_data(&state, 0, packet+pos, size-pos);
}

// AAC using fdk-aac
#ifdef HAVE_AAC_FDK
static void handle_l2cap_media_data_packet_aac(uint8_t *packet, uint16_t size){
    uint16_t pos = 0;

    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;

    int16_t decode_buf[2048 * 2]; // 2 == num_channel
    uint32_t framesize;
    AAC_DECODER_ERROR err;
    UINT uValid;
    UINT uSize;

    uSize = size - 12; // RTP header length
    uValid = uSize;
    UCHAR *ptr = (UCHAR *) (packet+pos);
    if ((err = aacDecoder_Fill(aac_handle, &ptr, &uSize, &uValid)) != AAC_DEC_OK) return;
    if ((err = aacDecoder_DecodeFrame(aac_handle, (INT_PCM *) decode_buf, sizeof(decode_buf) / sizeof(INT_PCM), 0)) != AAC_DEC_OK) {
        printf("decode error %d\n", err);
        return;
    }
    CStreamInfo *aac_info = aacDecoder_GetStreamInfo(aac_handle);
    framesize = aac_info->frameSize * 2; // 2 channels

    playback_queue_audio(decode_buf, aac_info->frameSize, 2);
}
#endif

static void handle_l2cap_media_data_packet_aptx(uint8_t *packet, uint16_t size) {
#ifdef HAVE_APTX
    size_t written;
    int16_t decode_buf16[2048 * 2]; // 2 == num_channel
    unsigned char decode_buf8[2048 * 2 * 2];
    aptx_decode(aptx_handle, packet, size, decode_buf8, sizeof(decode_buf8), &written);
    // convert to 16-bit
    for (int i = 0; i < written/3; i++) {
        decode_buf16[i] = (decode_buf8[i * 3 + 2] << 8) | decode_buf8[i * 3 + 1];
    }
    playback_queue_audio(decode_buf16, written/(3 * aptx_configuration.num_channels), aptx_configuration.num_channels);
#else
    UNUSED(packet);
    UNUSED(size);
#endif
}

static void handle_l2cap_media_data_packet_aptxhd(uint8_t *packet, uint16_t size) {
#ifdef HAVE_APTX
    uint16_t pos = 0;
    size_t written;
    int16_t decode_buf16[2048 * 2]; // 2 == num_channel
    unsigned char decode_buf8[2048 * 2 * 2];
    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;
    aptx_decode(aptx_handle, packet+pos, size-12, decode_buf8, sizeof(decode_buf8), &written);
    // convert to 16-bit
    for (int i = 0; i < written/3; i++) {
        decode_buf16[i] = (decode_buf8[i * 3 + 2] << 8) | decode_buf8[i * 3 + 1];
    }
    playback_queue_audio(decode_buf16, written/(3 * aptxhd_configuration.num_channels), aptxhd_configuration.num_channels);
#else
    UNUSED(packet);
    UNUSED(size);
#endif
}

static void handle_l2cap_media_data_packet_ldac(uint8_t *packet, uint16_t size) {
#ifdef HAVE_LDAC_DECODER
    uint16_t pos = 0;
    int used;
    int16_t decode_buf16[2048 * 2] = {0}; // 2 == num_channel
    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;
    pos++; // first byte is header
    while (pos < size) {
        ldacDecode(&ldac_handle, packet + pos, decode_buf16, &used);
        playback_queue_audio(decode_buf16, ldac_handle.frame.frameSamples, ldac_configuration.num_channels);
        pos += used;
    }
#else
    UNUSED(packet);
    UNUSED(size);
#endif
}

static void interleave_int24_to_int16(int32_t** in, int16_t* out, int32_t n, int32_t channels)
{
    int32_t ch, i;
    for (ch = 0; ch < channels; ch++) {
        for (i = 0; i < n; i++) {
            out[i * channels + ch] = (in[ch][i] >> 8) & 0xFFFF;
        }
    }
}

static void handle_l2cap_media_data_packet_lc3plus(uint8_t *packet, uint16_t size) {
#ifdef HAVE_LC3PLUS
    uint16_t pos = 0;
    static uint8_t lc3_bytes[LC3PLUS_MAX_BYTES];
    static int32_t lc3_out_ch1[LC3PLUS_MAX_SAMPLES];
    static int32_t lc3_out_ch2[LC3PLUS_MAX_SAMPLES];
    static int16_t out_samples[LC3PLUS_MAX_SAMPLES * 2];
    static uint16_t byte_ptr = 0;

    // read media header
    avdtp_media_packet_header_t media_header;
    if (!read_media_data_header(packet, size, &pos, &media_header)) return;
    size -= pos;

    // read payload header
    uint8_t payload_header = packet[pos];
    size--;
    pos++;
    bool is_fragmented = payload_header & (1 << 7);
    bool is_last_fragment = payload_header & (1 << 5);
    uint8_t number_of_frames = payload_header & 0x0F;

    // check if frame is not complete yet
    if (is_fragmented) {
        memcpy(&lc3_bytes[byte_ptr], &packet[pos], size);
        byte_ptr += size;
        if (!is_last_fragment) {
            return;
        }
    } else {
        byte_ptr = size;
    }

    // get frame len and number of samples
    uint16_t frame_len = byte_ptr / number_of_frames;
    uint16_t num_samples = lc3plus_dec_get_output_samples(lc3plus_handle);

    // prepare output
    int32_t *out_buf[2] = { lc3_out_ch1, lc3_out_ch2 };

    // decode complete fragmented frame
    if (is_fragmented) {
        int err = lc3plus_dec24(lc3plus_handle, lc3_bytes, frame_len, out_buf, lc3plus_scratch, 0);
        if (err == LC3PLUS_DECODE_ERROR)
            printf("using plc\n");
        else if (err != LC3PLUS_OK) {
            printf("LC3plus decoding error: %d\n", err);
            return;
        }
        interleave_int24_to_int16(out_buf, out_samples, num_samples, lc3plus_configuration.num_channels);
        playback_queue_audio(out_samples, num_samples, lc3plus_configuration.num_channels);
        byte_ptr = 0;
        return;
    }

    // decode all non fragmented frames
    int err;
    for (uint16_t i = 0; i < number_of_frames; i++) {
        err = lc3plus_dec24(lc3plus_handle, &packet[pos], frame_len, out_buf, lc3plus_scratch, 0);
        if (err == LC3PLUS_DECODE_ERROR)
            printf("using plc\n");
        else if (err != LC3PLUS_OK) {
            printf("LC3plus decoding error: %d\n", err);
            return;
        }
        interleave_int24_to_int16(out_buf, out_samples, num_samples, lc3plus_configuration.num_channels);
        playback_queue_audio(out_samples, num_samples, lc3plus_configuration.num_channels);
        pos += frame_len;
    }
    byte_ptr = 0;
#else
    UNUSED(packet);
    UNUSED(size);
#endif
}


// Codec XXX
//
//

// A2DP Media Payload Processing
static int read_media_data_header(uint8_t *packet, u_int16_t size, uint16_t *offset, avdtp_media_packet_header_t *media_header){
    int media_header_len = 12; // without crc
    int pos = *offset;

    if (size - pos < media_header_len){
        printf("Not enough data to read media packet header, expected %d, received %d\n", media_header_len, size-pos);
        return 0;
    }

    media_header->version = packet[pos] & 0x03;
    media_header->padding = get_bit16(packet[pos],2);
    media_header->extension = get_bit16(packet[pos],3);
    media_header->csrc_count = (packet[pos] >> 4) & 0x0F;
    pos++;

    media_header->marker = get_bit16(packet[pos],0);
    media_header->payload_type  = (packet[pos] >> 1) & 0x7F;
    pos++;

    media_header->sequence_number = big_endian_read_16(packet, pos);
    pos+=2;

    media_header->timestamp = big_endian_read_32(packet, pos);
    pos+=4;

    media_header->synchronization_source = big_endian_read_32(packet, pos);
    pos+=4;
    *offset = pos;
    // TODO: read csrc list

    // printf_hexdump( packet, pos );
    if (!is_media_header_reported_once){
        is_media_header_reported_once = 1;
        printf("MEDIA HEADER: %u timestamp, version %u, padding %u, extension %u, csrc_count %u\n",
               media_header->timestamp, media_header->version, media_header->padding, media_header->extension, media_header->csrc_count);
        printf("MEDIA HEADER: marker %02x, payload_type %02x, sequence_number %u, synchronization_source %u\n",
               media_header->marker, media_header->payload_type, media_header->sequence_number, media_header->synchronization_source);
    }
    return 1;
}

static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size){
    UNUSED(seid);
    switch (codec_type){
        case AVDTP_CODEC_SBC:
            handle_l2cap_media_data_packet_sbc(packet, size);
            break;
        case AVDTP_CODEC_MPEG_2_4_AAC:
            handle_l2cap_media_data_packet_aac(packet, size);
            break;
        case AVDTP_CODEC_NON_A2DP:
            if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX) {
                handle_l2cap_media_data_packet_aptx(packet, size);
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD) {
                handle_l2cap_media_data_packet_aptxhd(packet, size);
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC) {
                handle_l2cap_media_data_packet_ldac(packet, size);
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_FRAUNHOFER && codec_id == A2DP_FRAUNHOFER_CODEC_LC3PLUS) {
                handle_l2cap_media_data_packet_lc3plus(packet, size);
            } 
            break;
        default:
            printf("Media Codec %u not supported yet\n", codec_type);
            return;
    }
}

static uint32_t get_vendor_id(const uint8_t *codec_info) {
    uint32_t vid = 0;
    vid |= codec_info[0];
    vid |= codec_info[1] << 8;
    vid |= codec_info[2] << 16;
    vid |= codec_info[3] << 24;
    return vid;
}

static uint16_t get_codec_id(const uint8_t *codec_info) {
    uint16_t cid = 0;
    cid |= codec_info[4];
    cid |= codec_info[5] << 8;
    return cid;
}

#if HAVE_APTX
static int convert_aptx_sampling_frequency(uint8_t frequency_bitmap) {
    switch (frequency_bitmap) {
    case 1 << 4:
        return 48000;
    case 1 << 5:
        return 44100;
    case 1 << 6:
        return 32000;
    case 1 <<7:
        return 16000;
    default:
        printf("invalid aptx sampling frequency %d\n", frequency_bitmap);
        return 0;
    }
}

static int convert_aptx_num_channels(uint8_t channel_mode) {
    switch (channel_mode) {
    case 1 << 0:
    case 1 << 1:
    case 1 << 2:
        return 2;
    case 1 << 3:
        return 1;
    default:
        printf("invalid aptx channel mode %d\n", channel_mode);
        return 0;
    }
}
#endif

#ifdef HAVE_LDAC_DECODER
static int convert_ldac_sampling_frequency(uint8_t frequency_bitmap) {
    switch (frequency_bitmap) {
    case 1 << 0:
        return 192000;
    case 1 << 1:
        return 176400;
    case 1 << 2:
        return 96000;
    case 1 << 3:
        return 88200;
    case 1 << 4:
        return 48000;
    case 1 << 5:
        return 44100;
    default:
        printf("invalid ldac sampling frequency %d\n", frequency_bitmap);
        return 0;
    }
}

static int convert_ldac_num_channels(uint8_t channel_mode) {
    switch (channel_mode) {
    case 1 << 0: // stereo
    case 1 << 1: // dual channel
        return 2;
    case 1 << 2:
        return 1;
    default:
        printf("error ldac channel mode\n");
        return 0;
    }
}
#endif

#ifdef HAVE_LC3PLUS
typedef enum {
    A2DP_LC3PLUS_FRAME_DURATION_2_5 = 1 << 4,
    A2DP_LC3PLUS_FRAME_DURATION_5   = 1 << 5,
    A2DP_LC3PLUS_FRAME_DURATION_10  = 1 << 6,
} a2dp_shifted_lc3plus_frame_duration_t;

typedef enum {
    A2DP_LC3PLUS_CHANNEL_COUNT_2 = 1 << 6,
    A2DP_LC3PLUS_CHANNEL_COUNT_1 = 1 << 7,
} a2dp_shifted_lc3plus_channel_count_t;

typedef enum {
    A2DP_LC3PLUS_48000_HR = 1 << 0,
    A2DP_LC3PLUS_96000_HR = 1 << 15,
} a2dp_shifted_lc3plus_samplerate_t;
static uint8_t lc3plus_get_frame_durations(uint8_t *codec_info) { return codec_info[6] & 0xF0; }

static uint8_t lc3plus_get_channel_count(uint8_t *codec_info) { return codec_info[7]; }

uint16_t lc3plus_get_samplerate(uint8_t *codec_info) {
    return (codec_info[9] << 8) | codec_info[8];
}

int convert_lc3plus_frame_duration(int duration) {
    switch (duration) {
    case A2DP_LC3PLUS_FRAME_DURATION_2_5:
        return 25;
    case A2DP_LC3PLUS_FRAME_DURATION_5:
        return 50;
    case A2DP_LC3PLUS_FRAME_DURATION_10:
        return 100;
    default:
        printf("invalid lc3plus frame duration %d\n", duration);
        return 0;
    }
}

int convert_lc3plus_samplerate(int rate) {
    switch (rate) {
    case A2DP_LC3PLUS_96000_HR:
        return 96000;
    case A2DP_LC3PLUS_48000_HR:
        return 48000;
    default:
        printf("invalid lc3plus samplerate %d\n", rate);
        return 0;
    }
}

int convert_lc3plus_channel_count(int channels) {
    switch (channels) {
    case A2DP_LC3PLUS_CHANNEL_COUNT_2:
        return 2;
    case A2DP_LC3PLUS_CHANNEL_COUNT_1:
        return 1;
    default:
        printf("invalid lc3plus channel count %d\n", channels);
        return 0;
    }
}
#endif

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return; 
    
    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status    = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVDTP connection failed with status 0x%02x.\n", status);
                break;    
            }
            printf("AVDTP Sink connected: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            avdtp_cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);
            printf("AVDTP connection released: avdtp_cid 0x%02x.\n", avdtp_cid);
#ifdef HAVE_LC3PLUS
            if (lc3plus_handle) {
                free(lc3plus_handle);
                lc3plus_handle = NULL;
            }
            if (lc3plus_scratch) {
                free(lc3plus_scratch);
                lc3plus_scratch = NULL;
            }
#endif
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_FOUND:
            remote_seid = avdtp_subevent_signaling_sep_found_get_remote_seid(packet);
            printf("Found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", 
                remote_seid, avdtp_subevent_signaling_sep_found_get_in_use(packet), 
                avdtp_subevent_signaling_sep_found_get_media_type(packet), avdtp_subevent_signaling_sep_found_get_sep_type(packet));
            if (num_remote_seids == 0){
                first_remote_seid = remote_seid;
            }
            num_remote_seids++;
            break;

        case AVDTP_SUBEVENT_SIGNALING_SEP_DICOVERY_DONE:
            // select remote if there's only a single remote
            if (num_remote_seids == 1){
                printf("Only one remote Stream Endpoint with SEID %u, select it for initiator commands\n", first_remote_seid);
                remote_seid = first_remote_seid;
            }
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY:
            printf("CAPABILITY - MEDIA_CODEC_SBC: \n");
            sbc_capability.sampling_frequency_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
            sbc_capability.channel_mode_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
            sbc_capability.block_length_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
            sbc_capability.subbands_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
            sbc_capability.allocation_method_bitmap = avdtp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
            sbc_capability.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
            sbc_capability.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
            dump_sbc_capability(sbc_capability);
            local_seid = local_seid_sbc;
            printf("Selecting local SBC endpoint with SEID %u\n", local_seid);
            break;

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CAPABILITY:
            local_seid = local_seid_aac;
            printf("Selecting local MPEG AAC endpoint with SEID %u\n", local_seid);
            break;
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_TRANSPORT_CAPABILITY:
            printf("CAPABILITY - MEDIA_TRANSPORT supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_REPORTING_CAPABILITY:
            printf("CAPABILITY - REPORTING supported on remote.\n");
            break;
        case AVDTP_SUBEVENT_SIGNALING_RECOVERY_CAPABILITY:
            printf("CAPABILITY - RECOVERY supported on remote: \n");
            printf("    - recovery_type                %d\n", avdtp_subevent_signaling_recovery_capability_get_recovery_type(packet));
            printf("    - maximum_recovery_window_size %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_recovery_window_size(packet));
            printf("    - maximum_number_media_packets %d\n", avdtp_subevent_signaling_recovery_capability_get_maximum_number_media_packets(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONTENT_PROTECTION_CAPABILITY:
            printf("CAPABILITY - CONTENT_PROTECTION supported on remote: \n");
            printf("    - cp_type           %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type(packet));
            printf("    - cp_type_value_len %d\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value_len(packet));
            printf("    - cp_type_value     \'%s\'\n", avdtp_subevent_signaling_content_protection_capability_get_cp_type_value(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_MULTIPLEXING_CAPABILITY:
            printf("CAPABILITY - MULTIPLEXING supported on remote: \n");
            printf("    - fragmentation                  %d\n", avdtp_subevent_signaling_multiplexing_capability_get_fragmentation(packet));
            printf("    - transport_identifiers_num      %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_identifiers_num(packet));
            printf("    - transport_session_identifier_1 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_1(packet));
            printf("    - transport_session_identifier_2 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_2(packet));
            printf("    - transport_session_identifier_3 %d\n", avdtp_subevent_signaling_multiplexing_capability_get_transport_session_identifier_3(packet));
            printf("    - tcid_1                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_1(packet));
            printf("    - tcid_2                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_2(packet));
            printf("    - tcid_3                         %d\n", avdtp_subevent_signaling_multiplexing_capability_get_tcid_3(packet));
            break;
        case AVDTP_SUBEVENT_SIGNALING_DELAY_REPORTING_CAPABILITY:
            printf("CAPABILITY - DELAY_REPORTING supported on remote.\n");
            delay_reporting_supported_on_remote = true;
            break;
        case AVDTP_SUBEVENT_SIGNALING_HEADER_COMPRESSION_CAPABILITY:
            printf("CAPABILITY - HEADER_COMPRESSION supported on remote: \n");
            printf("    - back_ch   %d\n", avdtp_subevent_signaling_header_compression_capability_get_back_ch(packet));
            printf("    - media     %d\n", avdtp_subevent_signaling_header_compression_capability_get_media(packet));
            printf("    - recovery  %d\n", avdtp_subevent_signaling_header_compression_capability_get_recovery(packet));
            break;
            
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CAPABILITY:{
            printf("OTHER CAPABILITY\n");
            const uint8_t *media_info = avdtp_subevent_signaling_media_codec_other_capability_get_media_codec_information(packet);
            vendor_id = get_vendor_id(media_info);
            codec_id = get_codec_id(media_info);
            // APTX
            if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX) {
#ifdef HAVE_APTX
                local_seid = local_seid_aptx;
                printf("Selecting local APTX endpoint with SEID %u\n", local_seid);
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD) {
#ifdef HAVE_APTX
                local_seid = local_seid_aptxhd;
                printf("Selecting local APTX HD endpoint with SEID %u\n", local_seid);
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC) {
#ifdef HAVE_LDAC_DECODER
                local_seid = local_seid_ldac;
                printf("Selecting local LDAC endpoint with SEID %u\n", local_seid);
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_FRAUNHOFER && codec_id == A2DP_FRAUNHOFER_CODEC_LC3PLUS) {
#ifdef HAVE_LC3PLUS
                local_seid = local_seid_lc3plus;
                printf("Selecting local LC3plus endpoint with SEID %u\n", local_seid);
#endif
            }
            break;}

        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:
            codec_type = AVDTP_CODEC_SBC;
            local_seid = avdtp_subevent_signaling_media_codec_sbc_configuration_get_local_seid(packet);
            playback_configuration.num_channels = avdtp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            playback_configuration.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);;
            btstack_sbc_decoder_init(&state, mode, sbc_handle_pcm_data, NULL);

            printf("Received SBC codec configuration: avdtp_cid 0x%02x.\n", avdtp_cid);
            sbc_configuration.reconfigure = avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            sbc_configuration.num_channels = avdtp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            sbc_configuration.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sbc_configuration.channel_mode = avdtp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            sbc_configuration.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sbc_configuration.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sbc_configuration.allocation_method = avdtp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            sbc_configuration.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            sbc_configuration.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            dump_sbc_configuration(sbc_configuration);
            
            avdtp_sink_delay_report(avdtp_cid, local_seid, 100);
            break;

#ifdef HAVE_AAC_FDK
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_MPEG_AAC_CONFIGURATION:{
            codec_type = AVDTP_CODEC_MPEG_2_4_AAC;
            local_seid = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_local_seid(packet);
            playback_configuration.num_channels = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_num_channels(packet);
            playback_configuration.sampling_frequency = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_sampling_frequency(packet);;

            aac_configuration.reconfigure = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_reconfigure(packet);
            aac_configuration.sampling_frequency = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_sampling_frequency(packet);
            aac_configuration.aot = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_object_type(packet);
            aac_configuration.channel_mode = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_num_channels(packet);
            aac_configuration.bitrate = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_bit_rate(packet);
            aac_configuration.vbr = a2dp_subevent_signaling_media_codec_mpeg_aac_configuration_get_vbr(packet);
            if (aac_configuration.reconfigure){
                printf("A2DP Sink: Remote requested reconfiguration, ignoring it.\n");
            }

            printf("A2DP Sink: Received AAC configuration! Sampling frequency: %u, object type %u, channel mode %u, bitrate %u, vbr: %u\n",
                   aac_configuration.sampling_frequency, aac_configuration.aot, aac_configuration.channel_mode,
                   aac_configuration.bitrate, aac_configuration.vbr);

            // initialize decoder
            AAC_DECODER_ERROR err;
            aac_handle = aacDecoder_Open(TT_MP4_LATM_MCP1, 1);
            if ((err = aacDecoder_SetParam(aac_handle, AAC_PCM_MAX_OUTPUT_CHANNELS, aac_configuration.channel_mode)) != AAC_DEC_OK) {
                printf("Couldn't set output channels: %d", err);
                break;
            }
            if ((err = aacDecoder_SetParam(aac_handle, AAC_PCM_MIN_OUTPUT_CHANNELS, aac_configuration.channel_mode)) != AAC_DEC_OK) {
                printf("Couldn't set output channels: %d", err);
                break;
            }
            break;
        }
#endif
        case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_OTHER_CONFIGURATION:{
            codec_type = AVDTP_CODEC_NON_A2DP;
            local_seid = a2dp_subevent_signaling_media_codec_other_configuration_get_local_seid(packet);
            const uint8_t *media_info = avdtp_subevent_signaling_media_codec_other_configuration_get_media_codec_information(packet);
            vendor_id = get_vendor_id(media_info);
            codec_id = get_codec_id(media_info);
            // APTX
            if (vendor_id == A2DP_CODEC_VENDOR_ID_APT_LTD && codec_id == A2DP_APT_LTD_CODEC_APTX) {
#if HAVE_APTX
                aptx_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                aptx_configuration.sampling_frequency = media_info[6] & 0xF0;
                aptx_configuration.channel_mode = media_info[6] & 0x0F;
                aptx_configuration.sampling_frequency = convert_aptx_sampling_frequency(aptx_configuration.sampling_frequency);
                aptx_configuration.num_channels = convert_aptx_num_channels(aptx_configuration.channel_mode);
                printf("A2DP Source: Received APTX configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        aptx_configuration.sampling_frequency, aptx_configuration.channel_mode, aptx_configuration.num_channels);
                aptx_handle = aptx_init(0);
                playback_configuration.num_channels = aptx_configuration.num_channels;
                playback_configuration.sampling_frequency = aptx_configuration.sampling_frequency;
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_QUALCOMM && codec_id == A2DP_QUALCOMM_CODEC_APTX_HD) {
#if HAVE_APTX
                aptxhd_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                aptxhd_configuration.sampling_frequency = media_info[6] & 0xF0;
                aptxhd_configuration.channel_mode = media_info[6] & 0x0F;
                aptxhd_configuration.sampling_frequency = convert_aptx_sampling_frequency(aptxhd_configuration.sampling_frequency);
                aptxhd_configuration.num_channels = convert_aptx_num_channels(aptxhd_configuration.channel_mode);
                printf("A2DP Source: Received APTX HD configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        aptxhd_configuration.sampling_frequency, aptxhd_configuration.channel_mode, aptxhd_configuration.num_channels);
                aptx_handle = aptx_init(1);
                playback_configuration.num_channels = aptxhd_configuration.num_channels;
                playback_configuration.sampling_frequency = aptxhd_configuration.sampling_frequency;
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_SONY && codec_id == A2DP_SONY_CODEC_LDAC) {
#ifdef HAVE_LDAC_DECODER
                ldac_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                ldac_configuration.sampling_frequency = media_info[6];
                ldac_configuration.channel_mode = media_info[7];
                ldac_configuration.sampling_frequency = convert_ldac_sampling_frequency(ldac_configuration.sampling_frequency);
                ldac_configuration.num_channels = convert_ldac_num_channels(ldac_configuration.channel_mode);
                printf("A2DP Source: Received LDAC configuration! Sampling frequency: %d, channel mode: %d channels: %d\n",
                        ldac_configuration.sampling_frequency, ldac_configuration.channel_mode, ldac_configuration.num_channels);
                ldacdecInit(&ldac_handle);
                playback_configuration.num_channels = ldac_configuration.num_channels;
                playback_configuration.sampling_frequency = ldac_configuration.sampling_frequency;
#endif
            } else if (vendor_id == A2DP_CODEC_VENDOR_ID_FRAUNHOFER && codec_id == A2DP_FRAUNHOFER_CODEC_LC3PLUS) {
#ifdef HAVE_LC3PLUS
                lc3plus_configuration.reconfigure = a2dp_subevent_signaling_media_codec_other_configuration_get_reconfigure(packet);
                lc3plus_configuration.sampling_frequency = lc3plus_get_samplerate((uint8_t *) media_info);
                lc3plus_configuration.frame_duration = lc3plus_get_frame_durations((uint8_t *) media_info);
                lc3plus_configuration.num_channels = lc3plus_get_channel_count((uint8_t *) media_info);

                lc3plus_configuration.frame_duration = convert_lc3plus_frame_duration(lc3plus_configuration.frame_duration);
                lc3plus_configuration.sampling_frequency = convert_lc3plus_samplerate(lc3plus_configuration.sampling_frequency);
                lc3plus_configuration.num_channels = convert_lc3plus_channel_count(lc3plus_configuration.num_channels);
                playback_configuration.num_channels = lc3plus_configuration.num_channels;
                playback_configuration.sampling_frequency = lc3plus_configuration.sampling_frequency;
                printf("A2DP Sink: Received LC3Plus configuration! Sampling frequency: %d, channels: %d, frame duration %0.1f\n",
                    lc3plus_configuration.sampling_frequency,
                    lc3plus_configuration.num_channels,
                    lc3plus_configuration.frame_duration * 0.1);

                lc3plus_handle = malloc(lc3plus_dec_get_size(lc3plus_configuration.sampling_frequency, lc3plus_configuration.num_channels, LC3PLUS_PLC_ADVANCED));
                if (lc3plus_handle == NULL) {
                    printf("Failed to allocate lc3plus decoder memory\n");
                    break;
                }
                if (lc3plus_dec_init(lc3plus_handle, lc3plus_configuration.sampling_frequency,
                                        lc3plus_configuration.num_channels, LC3PLUS_PLC_ADVANCED, 1) != LC3PLUS_OK) {
                    free(lc3plus_handle);
                    lc3plus_handle = NULL;
                    printf("Failed to initialize lc3plus decoder\n");
                    break;
                }
                if (lc3plus_dec_set_frame_dms(
                        lc3plus_handle, lc3plus_configuration.frame_duration) != LC3PLUS_OK) {
                    free(lc3plus_handle);
                    lc3plus_handle = NULL;
                    printf("Failed to set lc3plus frame duration\n");
                    break;
                }
                lc3plus_scratch = malloc(lc3plus_dec_get_scratch_size(lc3plus_handle));
                if (lc3plus_scratch == NULL) {
                    printf("Failed to allocate lc3plus scratch memory\n");
                    free(lc3plus_handle);
                    lc3plus_handle = NULL;
                    break;
                }
#endif
            }
            break;}

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_streaming_connection_established_get_avdtp_cid(packet);
            printf("Streaming connection opened: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
            printf("Streaming connection released: avdtp_cid 0x%02x.\n", avdtp_cid);
            is_cmd_triggered_locally = 0;
            is_media_header_reported_once = 0;
            playback_close();
            break;

        case AVDTP_SUBEVENT_SIGNALING_ACCEPT:{
            switch (avdtp_subevent_signaling_accept_get_signal_identifier(packet)){
                case  AVDTP_SI_START:
                    printf("Stream started\n");
                    playback_close();
                    playback_init(&playback_configuration);
                    playback_start();
                    break;
                case AVDTP_SI_SUSPEND:
                    printf("Stream paused\n");
                    playback_pause();
                    break;
                case AVDTP_SI_ABORT:
                case AVDTP_SI_CLOSE:
                    printf("Stream stoped\n");
                    playback_close();
                    break;
                default:
                    break;
            }
            if (is_cmd_triggered_locally){
                is_cmd_triggered_locally = 0;
                printf("AVDTP Sink command accepted\n");
            }
            break;
        }
        case AVDTP_SUBEVENT_SIGNALING_REJECT:
        case AVDTP_SUBEVENT_SIGNALING_GENERAL_REJECT:
            if (is_cmd_triggered_locally){
                is_cmd_triggered_locally = 0;
                printf("AVDTP Sink command rejected\n");
            }
            break;
        default:
            if (is_cmd_triggered_locally){
                is_cmd_triggered_locally = 0;
            }
            break; 
    }
}


static uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static uint8_t media_sbc_codec_configuration[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

static uint8_t media_sbc_codec_reconfiguration[] = {
    // (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    // (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_SNR,
    // 32, 32
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_MONO,
    (AVDTP_SBC_BLOCK_LENGTH_8 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    32, 32 
};

static uint8_t media_aac_codec_capabilities[] = {
        0xF0,
        0xFF,
        0xFC,
        0x80,
        0,
        0
};

static uint8_t media_aac_codec_configuration[6];

#ifdef HAVE_APTX
static uint8_t media_aptx_codec_capabilities[] = {
        0x4F, 0x0, 0x0, 0x0,
        0x1, 0,
        0xFF,
};

static uint8_t media_aptx_codec_configuration[7];

static uint8_t media_aptxhd_codec_capabilities[] = {
        0xD7, 0x0, 0x0, 0x0,
        0x24, 0,
        0xFF,
        0, 0, 0, 0
};
static uint8_t media_aptxhd_codec_configuration[11];
#endif

#ifdef HAVE_LDAC_DECODER
static uint8_t media_ldac_codec_capabilities[] = {
        0x2D, 0x1, 0x0, 0x0,
        0xAA, 0,
        0x3C,
        0x7
};
static uint8_t media_ldac_codec_configuration[8];
#endif

#ifdef HAVE_LC3PLUS
static uint8_t media_lc3plus_codec_capabilities[] = {
        0xA9, 0x08, 0x0, 0x0,
        0x01, 0x0,
        0x70,
        0xc0,
        0x01, 0x80
};
static uint8_t media_lc3plus_codec_configuration[] = {
        0xA9, 0x08, 0x0, 0x0,
        0x01, 0x0,
        0x40,
        0x40,
        0x00, 0x80
};
#endif

static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP SINK Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("Supported Codecs: SBC");
#ifdef HAVE_AAC_FDK
    printf(", MPEG AAC");
#endif
#ifdef HAVE_APTX
    printf(", aptX, aptX HD");
#endif
#ifdef HAVE_LDAC_DECODER
    printf(", LDAC");
#endif
#ifdef HAVE_LC3PLUS
    printf(", LC3plus");
#endif
    printf("\n");
    printf("c      - create connection to addr %s\n", device_addr_string);
    printf("d      - discover stream endpoints\n");
    printf("g      - get capabilities\n");
    printf("a      - get all capabilities\n");
    printf("s      - set configuration\n");
    printf("f      - get configuration\n");
    printf("R      - reconfigure stream with local seid %d\n", local_seid);
    printf("o      - establish stream with local seid %d\n", local_seid);
    printf("m      - start stream with local %d\n", local_seid);
    printf("A      - abort stream with local %d\n", local_seid);
    printf("P      - suspend (pause) stream with local %d\n", local_seid);
    printf("S      - stop (release) stream with local %d\n", local_seid);
    printf("D      - send delay report\n");
    printf("C      - disconnect\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    remote_seid = 1;
    is_cmd_triggered_locally = 1;
    switch (cmd){
        case 'c':
            printf("Establish AVDTP Sink connection to %s\n", device_addr_string);
            status = avdtp_sink_connect(device_addr, &avdtp_cid);
            break;
        case 'C':
            printf("Disconnect AVDTP Sink\n");
            status = avdtp_sink_disconnect(avdtp_cid);
            break;
        case 'd':
            printf("Discover stream endpoints of %s\n", device_addr_string);
            first_remote_seid = 0;
            num_remote_seids  = 0;
            status = avdtp_sink_discover_stream_endpoints(avdtp_cid);
            break;
        case 'g':
            printf("Get capabilities of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_capabilities(avdtp_cid, remote_seid);
            break;
        case 'a':
            printf("Get all capabilities of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_all_capabilities(avdtp_cid, remote_seid);
            break;
        case 'f':
            printf("Get configuration of stream endpoint with seid %d\n", remote_seid);
            status = avdtp_sink_get_configuration(avdtp_cid, remote_seid);
            break;
        case 's':
            printf("Set configuration of stream endpoint with local %u and remote seid %d\n", local_seid, remote_seid);
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            if (local_seid == local_seid_sbc) {
                remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
                remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_configuration);
                remote_configuration.media_codec.media_codec_information = media_sbc_codec_configuration;
#ifdef HAVE_LC3PLUS
            } else if (local_seid == local_seid_lc3plus) {
                remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_NON_A2DP;
                remote_configuration.media_codec.media_codec_information_len = sizeof(media_lc3plus_codec_configuration);
                remote_configuration.media_codec.media_codec_information = media_lc3plus_codec_configuration;
#endif
            } else {
                printf("Set configuration for local seid %d not implemented\n", local_seid);
                break;
            }

            if (delay_reporting_supported_on_remote){
                remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_DELAY_REPORTING, 1);
            }

            status = avdtp_sink_set_configuration(avdtp_cid, local_seid, remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'R':
            printf("Reconfigure stream endpoint with seid %d\n", remote_seid);
            remote_configuration_bitmap = 0;
            remote_configuration_bitmap = store_bit16(remote_configuration_bitmap, AVDTP_MEDIA_CODEC, 1);
            remote_configuration.media_codec.media_type = AVDTP_AUDIO;
            remote_configuration.media_codec.media_codec_type = AVDTP_CODEC_SBC;
            remote_configuration.media_codec.media_codec_information_len = sizeof(media_sbc_codec_reconfiguration);
            remote_configuration.media_codec.media_codec_information = media_sbc_codec_reconfiguration;
            status = avdtp_sink_reconfigure(avdtp_cid, local_seid, remote_seid, remote_configuration_bitmap, remote_configuration);
            break;
        case 'o':
            printf("Establish stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_open_stream(avdtp_cid, local_seid, remote_seid);
            break;
        case 'm': 
            printf("Start stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_start_stream(avdtp_cid, local_seid);
            break;
        case 'A':
            printf("Abort stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_abort_stream(avdtp_cid, local_seid);
            break;
        case 'S':
            printf("Release stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_stop_stream(avdtp_cid, local_seid);
            break;
        case 'P':
            printf("Suspend stream between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_suspend(avdtp_cid, local_seid);
            break;
        case 'D':
            printf("Send delay report between local %d and remote %d seid\n", local_seid, remote_seid);
            status = avdtp_sink_delay_report(avdtp_cid, local_seid, 100);
            break;
        case '\n':
        case '\r':
            is_cmd_triggered_locally = 0;
            break;
        default:
            is_cmd_triggered_locally = 0;
            show_usage();
            break;

    }
    if (status != ERROR_CODE_SUCCESS){
        printf("AVDTP Sink cmd \'%c\' failed, status 0x%02x\n", cmd, status);
    }
}


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_sink_init();
    avdtp_sink_register_packet_handler(&packet_handler);

    avdtp_stream_endpoint_t * local_stream_endpoint;

    // in PTS 7.6.1 Build 4, test AVDTP/SNK/ACP/SIG/SMG/BI-08-C fails if AAC Endpoint is defined last
#ifdef HAVE_AAC_FDK
    // Setup AAC Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_aac_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_aac_codec_configuration);
    local_seid_aac = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_aac);
    avdtp_sink_register_media_codec_category(local_seid_aac, AVDTP_AUDIO, AVDTP_CODEC_MPEG_2_4_AAC, media_aac_codec_capabilities, sizeof(media_aac_codec_capabilities));
#endif

#ifdef HAVE_LDAC_DECODER
    // Setup LDAC Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_ldac_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_ldac_codec_configuration);
    local_seid_ldac = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_ldac);
    avdtp_sink_register_media_codec_category(local_seid_ldac, AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, media_ldac_codec_capabilities, sizeof(media_ldac_codec_capabilities));
#endif

#ifdef HAVE_APTX
    // Setup APTX Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_aptx_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_aptx_codec_configuration);
    local_seid_aptx = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_aptx);
    avdtp_sink_register_media_codec_category(local_seid_aptx, AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, media_aptx_codec_capabilities, sizeof(media_aptx_codec_capabilities));

    // Setup APTX HD Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_aptxhd_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_aptxhd_codec_configuration);
    local_seid_aptxhd = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_aptxhd);
    avdtp_sink_register_media_codec_category(local_seid_aptxhd, AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, media_aptxhd_codec_capabilities, sizeof(media_aptxhd_codec_capabilities));
#endif

#ifdef HAVE_LC3PLUS
    // Setup lc3plus Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_lc3plus_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_lc3plus_codec_configuration);
    local_seid_lc3plus = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_lc3plus);
    avdtp_sink_register_media_codec_category(local_seid_lc3plus, AVDTP_AUDIO, AVDTP_CODEC_NON_A2DP, media_lc3plus_codec_capabilities, sizeof(media_lc3plus_codec_capabilities));
#endif

    // Setup SBC Endpoint
    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    btstack_assert(local_stream_endpoint != NULL);
    local_stream_endpoint->media_codec_configuration_info = media_sbc_codec_configuration;
    local_stream_endpoint->media_codec_configuration_len  = sizeof(media_sbc_codec_configuration);
    local_seid_sbc = avdtp_local_seid(local_stream_endpoint);
    avdtp_sink_register_media_transport_category(local_seid_sbc);
    avdtp_sink_register_media_codec_category(local_seid_sbc, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));
    avdtp_sink_register_delay_reporting_category(avdtp_stream_endpoint_seid(local_stream_endpoint));


    avdtp_sink_register_media_handler(&handle_l2cap_media_data_packet);
    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, AVDTP_SINK_FEATURE_MASK_HEADPHONE, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    // printf("BTstack AVDTP Sink, supported features 0x%04x\n", );
    gap_set_local_name("AVDTP Sink 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
