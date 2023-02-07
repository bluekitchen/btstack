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

#define BTSTACK_FILE__ "avrcp_test.c"

/*
 * avrcp_test.c : Tool for testig AVRCP with PTS, see avrcp_test.md for PTS tests command sequences
 */

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"
#include "btstack_stdin_pts.h"

#define AVRCP_BROWSING_ENABLED

#define AVRCP_BROWSING_MAX_FOLDERS                  10
#define AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN  30
#define AVRCP_BROWSING_MAX_MEDIA_ITEMS              10

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin_pts.h"
#endif

#ifdef HAVE_BTSTACK_STDIN
// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// pts:         
static const char * device_addr_string = "00:1B:DC:08:E2:5C";
// iPod 5G-C:   static const char * device_addr_string = "00:1B:DC:08:E2:5C";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// bt dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";

#endif
static bd_addr_t device_addr;
static bd_addr_t device_addr_browsing;

static const uint8_t fragmented_message[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
        0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
        0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F
};

typedef enum {
    AVRCP_BROWSING_STATE_IDLE,
    AVRCP_BROWSING_STATE_W4_GET_PLAYERS,
    AVRCP_BROWSING_STATE_W4_SET_PLAYER,
    AVRCP_BROWSING_STATE_READY
} avrcp_browsing_state_t;

typedef struct {
    uint16_t  charset;
    uint8_t   depth;
    uint16_t  name_len;
    char      name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
} avrcp_browsing_root_folder_t;

typedef struct {
    uint8_t  uid[8];
    uint16_t name_len;
    char     name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
} avrcp_browsable_item_t;

typedef struct {
    uint16_t player_id;
    uint8_t  major_player_type;
    uint32_t player_sub_type;
    uint8_t  play_status;
    uint8_t  feature_bitmask[16];
    uint16_t charset;
    uint16_t name_len;
    char     name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
} avrcp_browsable_media_player_item_t;

typedef struct {
    uint32_t id;
    uint16_t charset;
    uint16_t value_len;
    char     value[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
} avrcp_browsable_media_element_item_attribute_t;

typedef struct {
    uint8_t  uid[8];
    uint8_t  media_type;
    uint16_t charset;
    uint16_t name_len;
    char     name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
    uint8_t  num_attributes;
} avrcp_browsable_media_element_item_t;

void btstack_stdin_pts_setup(void (*stdin_handler)(char * c, int size));

static uint8_t  parent_folder_set = 0;
static uint8_t  parent_folder_uid[8];
static char     parent_folder_name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];

static avrcp_browsable_media_element_item_t media_element_items[AVRCP_BROWSING_MAX_MEDIA_ITEMS];
static int media_element_item_index = -1;

static avrcp_browsable_media_player_item_t media_player_items[AVRCP_BROWSING_MAX_MEDIA_ITEMS];
static int media_player_item_index = -1;

static avrcp_browsable_item_t folders[AVRCP_BROWSING_MAX_FOLDERS];
static int folder_index = -1;

static bd_addr_t device_addr;

static uint16_t avrcp_cid = 0;
static uint8_t  avrcp_connected = 0;

static uint16_t browsing_cid = 0;
static uint8_t  avrcp_browsing_connected = 0;
static int playable_folder_index = 0;
static uint8_t browsing_query_active = 0;

static uint8_t sdp_avdtp_sink_service_buffer[150];
static uint8_t sdp_avdtp_source_service_buffer[150];
static uint8_t sdp_avrcp_controller_service_buffer[200];
static uint8_t sdp_avrcp_target_service_buffer[200];

static uint16_t avdtp_cid = 0;
static avdtp_sep_t sep;
static avdtp_stream_endpoint_t * local_stream_endpoint;
static uint8_t  avrcp_value[500];
static uint16_t browsing_uid_counter = 0;
static avrcp_browsing_state_t browsing_state = AVRCP_BROWSING_STATE_IDLE;

static bool auto_avrcp_browsing = false;
static bool auto_avrcp_regular  = false;

static uint8_t ertm_buffer[10000];
static l2cap_ertm_config_t ertm_config = {
    1,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    144,    // l2cap ertm mtu
    4,
    4,
};

/* AVRCP Target context START */


#define NUM_CHANNELS                2
#define A2DP_SAMPLE_RATE            44100
#define BYTES_PER_AUDIO_SAMPLE      (2*NUM_CHANNELS)
#define AUDIO_TIMEOUT_MS            10 
#define TABLE_SIZE_441HZ            100

#define SBC_STORAGE_SIZE 1030

typedef enum {
    STREAM_SINE = 0,
    STREAM_MOD,
    STREAM_PTS_TEST
} stream_data_source_t;
    
typedef struct {
    uint16_t a2dp_cid;
    uint8_t  local_seid;
    uint8_t  connected;
    uint8_t  stream_opened;

    uint32_t time_audio_data_sent; // ms
    uint32_t acc_num_missed_samples;
    uint32_t samples_ready;
    btstack_timer_source_t audio_timer;
    uint8_t  streaming;
    int      max_media_payload_size;
    
    uint8_t  sbc_storage[SBC_STORAGE_SIZE];
    uint16_t sbc_storage_count;
    uint8_t  sbc_ready_to_send;
} a2dp_media_sending_context_t;


typedef struct {
    int reconfigure;
    
    int num_channels;
    int sampling_frequency;
    int block_length;
    int subbands;
    int min_bitpool_value;
    int max_bitpool_value;
    btstack_sbc_channel_mode_t      channel_mode;
    btstack_sbc_allocation_method_t allocation_method;
} media_codec_configuration_sbc_t;


static uint8_t ertm_buffer[10000];

static uint8_t media_player_list[] = { 
    // num players (2B) 
    0x00, 0x02, 
    // item type:  0x01 (Media Player Item)
    0x01, 
    // Item Length (2B)
    0x00, 0x2A, 
    // Player ID (2B)
    0x00, 0x01, 
    // Major Player Type (1B)
    0x03, 
    // Player Sub Type (4B)
    0x00, 0x00, 0x00, 0x01,
    // Play Status (1B) 
    0x00, 
    // Feature Bit Mask (16B) 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    // Character Set Id (2B): 0x006A (UTF-8)
    0x00, 0x6A, 
    // Displayable Name Length (2B)
    0x00, 0x0E, 
    // Displayable Name 
    0x42, 0x72, 0x6F, 0x77, 0x73, 0x65, 0x64, 0x20, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72, 
    
    // item type:  0x01 (Media Player Item)
    0x01, 
    // Item Length (2B)
    0x00, 0x2C, 
    // Player ID (2B)
    0x00, 0x02, 
    // Major Player Type (1B)
    0x03, 
    // Player Sub Type (4B)
    0x00, 0x00, 0x00, 0x01,
    // Play Status (1B)
    0x00,
    // Feature Bit Mask (16B) 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x77, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    // Character Set Id (2B): 0x006A (UTF-8)
    0x00, 0x6A, 
    // Displayable Name Length (2B)
    0x00, 0x10, 
    // Displayable Name
    0x41, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x65, 0x64, 0x20, 0x50, 0x6C, 0x61, 0x79, 0x65, 0x72}; 


static uint8_t browsed_player_data[]= {
    // number of items (3)
    0x00, 0x00, 0x04,
    // Character Set Id (2B): 0x006A (UTF-8)
    0x00, 0x6A, 
    // Folder depth
    0x03,
    // Folder Name Length (2)
    0x00, 0x01,
    // FOlder Name
    0x41,
    // Folder Name Length (2)
    0x00, 0x01,
    // FOlder Name
    0x42,
    // Folder Name Length (2)
    0x00, 0x01,
    // FOlder Name
    0x43
};

static uint8_t virtual_filesystem_list[] ={ 0x00, 0x07, 0x02, 0x00, 0x14, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x6A, 0x00, 0x06, 0x41, 0x6C, 0x62, 0x75, 0x6D, 0x73, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x6A, 0x00, 0x07, 0x41, 0x72, 0x74, 0x69, 0x73, 0x74, 0x73, 0x02, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x04, 0x00, 0x00, 0x6A, 0x00, 0x06, 0x47, 0x65, 0x6E, 0x72, 0x65, 0x73, 0x02, 0x00, 0x17, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00, 0x6A, 0x00, 0x09, 0x50, 0x6C, 0x61, 0x79, 0x6C, 0x69, 0x73, 0x74, 0x73, 0x02, 0x00, 0x13, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x01, 0x01, 0x00, 0x6A, 0x00, 0x05, 0x53, 0x6F, 0x6E, 0x67, 0x73, 0x02, 0x00, 0x13, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00}; // 0x71, 0x00, 0xA7, 0x04, 0x5A, 0x73,
static uint8_t search_list[] = {};
static uint8_t now_playing_list[] = {};

static avrcp_media_attribute_id_t now_playing_info_attributes [] = {
    AVRCP_MEDIA_ATTR_TITLE
};

typedef struct {
    uint8_t track_id[8];
    uint32_t song_length_ms;
    avrcp_playback_status_t status;
    uint32_t song_position_ms; // 0xFFFFFFFF if not supported
} avrcp_play_status_info_t;

// python -c "print('a'*512)"
static const char title[] = "Really aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa Really Really Long Song Title Name So That This Response Will Fragment Into Enough Pieces For This Test Case";

// Generated02 Generated3 Generated4 Generated5 Generated6 Generated7 Generated8 Generated9
avrcp_track_t tracks[] = {
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}, 1, "Sine0Sine1", "Generated01", "AVRCP Demo", "monotone01", 1234567890},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02}, 2, "Nao-deceased", "Decease", "AVRCP Demo", "vivid", 12345},
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03}, 3, (char *)title, "Decease", "AVRCP Demo", "vivid", 12345},
};

avrcp_play_status_info_t play_info;
static media_codec_configuration_sbc_t sbc_configuration;
static btstack_sbc_encoder_state_t sbc_encoder_state;

static uint8_t media_sbc_codec_configuration[4];
static a2dp_media_sending_context_t media_tracker;

static uint16_t avrcp_cid;
static uint8_t  avrcp_connected;
static uint16_t uid_counter = 0x5A73;
static stream_data_source_t data_source;

/* AVRCP Target context END */
static void reset_avrcp_context(void){
        avrcp_cid = 0;
        avrcp_connected = 0;
        auto_avrcp_regular  = false;
}

static void reset_avrcp_browsing_context(void){
        memset(folders, 0, sizeof(folders));
        parent_folder_set = 0;
        media_element_item_index = -1;
        media_player_item_index = -1;
        folder_index = -1;

        browsing_cid = 0;
        avrcp_browsing_connected = 0;
        playable_folder_index = 0;
        browsing_query_active = 0;

        browsing_uid_counter = 0;
        browsing_state = AVRCP_BROWSING_STATE_IDLE;

        auto_avrcp_browsing = false;
}

static inline int next_index(int * index, int max_value){
    if ((*index) < max_value){
        (*index)++;
    } else {
        (*index) = 0;
    }
    return (*index);
}

static int next_folder_index(){
    return next_index(&folder_index, AVRCP_BROWSING_MAX_FOLDERS);
}

static int next_media_element_item_index(){
    return next_index(&media_element_item_index, AVRCP_BROWSING_MAX_MEDIA_ITEMS);
}
static int next_media_player_item_index(){
    return next_index(&media_player_item_index, AVRCP_BROWSING_MAX_MEDIA_ITEMS);
}

static void avdtp_sink_connection_establishment_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t  status;

    if (packet_type != HCI_EVENT_PACKET)     return;
    if (packet[0]   != HCI_EVENT_AVDTP_META) return;

    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status    = avdtp_subevent_signaling_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVDTP connection failed: status 0x%02x.\n", status);
                break;    
            }
            printf("AVDTP connection established: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            avdtp_cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);

            printf("AVDTP connection released: avdtp_cid 0x%02x.\n", avdtp_cid);
            break;
        default:
            break; 
    }
}

static void avdtp_source_connection_establishment_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t status;
    uint8_t local_seid;
    bd_addr_t address;
    uint16_t cid;

    uint8_t channel_mode;
    uint8_t allocation_method;

    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_PIN_CODE_REQUEST:
            printf("Pin code request - using '0000'\n");
            hci_event_pin_code_request_get_bd_addr(packet, address);
            gap_pin_code_response(address, "0000");
            break;
            // Test hack: initiate avrcp connection on incoming hci connection request to trigger parallel connect
        case HCI_EVENT_CONNECTION_REQUEST:
            if (auto_avrcp_regular){
                printf("Incoming HCI Connection -> Create regular AVRCP connection to addr %s.\n", bd_addr_to_str(device_addr));
                avrcp_connect(device_addr, &avrcp_cid);
            }
            break;
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            printf("Create AVDTP Source connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = a2dp_source_establish_stream(device_addr, &media_tracker.a2dp_cid);
            if (status != ERROR_CODE_SUCCESS){
                printf("Could not perform command, status 0x%2x\n", status);
            }
            return;
        default:
            break;
    }

    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVDTP_META) return;
    
    switch (packet[2]){
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
            avdtp_subevent_signaling_connection_established_get_bd_addr(packet, address);
            cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
            status = avdtp_subevent_signaling_connection_established_get_status(packet);

            if (status != ERROR_CODE_SUCCESS){
                printf(" AVRCP target test: Connection failed, received status 0x%02x\n", status);
                break;
            }
            if (!media_tracker.a2dp_cid){
                media_tracker.a2dp_cid = cid;
            } else if (cid != media_tracker.a2dp_cid){
                printf(" AVRCP target test: Connection failed, received cid 0x%02x, expected cid 0x%02x\n", cid, media_tracker.a2dp_cid);
                break;
            }
            printf(" AVRCP target test: Connected to address %s, a2dp cid 0x%02x.\n", bd_addr_to_str(address), media_tracker.a2dp_cid);
            break;

         case AVDTP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION:{
            printf(" AVRCP target test: Received SBC codec configuration.\n");
            sbc_configuration.reconfigure = avdtp_subevent_signaling_media_codec_sbc_configuration_get_reconfigure(packet);
            sbc_configuration.num_channels = avdtp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
            sbc_configuration.sampling_frequency = avdtp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
            sbc_configuration.block_length = avdtp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
            sbc_configuration.subbands = avdtp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
            sbc_configuration.min_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
            sbc_configuration.max_bitpool_value = avdtp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
            
            allocation_method = a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);
            channel_mode = a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet);
            
            // Adapt Bluetooth spec definition to SBC Encoder expected input
            sbc_configuration.allocation_method = (btstack_sbc_allocation_method_t)(allocation_method - 1);
            sbc_configuration.num_channels = SBC_CHANNEL_MODE_STEREO;
            switch (channel_mode){
                case AVDTP_SBC_JOINT_STEREO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
                    break;
                case AVDTP_SBC_STEREO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_STEREO;
                    break;
                case AVDTP_SBC_DUAL_CHANNEL:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_DUAL_CHANNEL;
                    break;
                case AVDTP_SBC_MONO:
                    sbc_configuration.channel_mode = SBC_CHANNEL_MODE_MONO;
                    sbc_configuration.num_channels = 1;
                    break;
                default:
                    btstack_assert(false);
                    break;
            }

            btstack_sbc_encoder_init(&sbc_encoder_state, SBC_MODE_STANDARD, 
                sbc_configuration.block_length, sbc_configuration.subbands, 
                sbc_configuration.allocation_method, sbc_configuration.sampling_frequency, 
                sbc_configuration.max_bitpool_value,
                sbc_configuration.channel_mode);
            
            break;
        }  

        case AVDTP_SUBEVENT_STREAMING_CONNECTION_ESTABLISHED:
            avdtp_subevent_streaming_connection_established_get_bd_addr(packet, address);
            status = avdtp_subevent_streaming_connection_established_get_status(packet);
            if (status){
                printf(" AVRCP target test: Stream failed, status 0x%02x.\n", status);
                break;
            }
            local_seid = avdtp_subevent_streaming_connection_established_get_local_seid(packet);
            if (local_seid != media_tracker.local_seid){
                printf(" AVRCP target test: Stream failed, wrong local seid %d, expected %d.\n", local_seid, media_tracker.local_seid);
                break;    
            }
            printf(" AVRCP target test: Stream established, address %s, a2dp cid 0x%02x, local seid %d, remote seid %d.\n", bd_addr_to_str(address),
                media_tracker.a2dp_cid, media_tracker.local_seid, avdtp_subevent_streaming_connection_established_get_remote_seid(packet));
            printf(" AVRCP target test: Start playing mod, a2dp cid 0x%02x.\n", media_tracker.a2dp_cid);
            media_tracker.stream_opened = 1;
            data_source = STREAM_MOD;
            // status = a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
            break;

        // case AVDTP_SUBEVENT_STREAM_STARTED:
        //     play_info.status = AVRCP_PLAYBACK_STATUS_PLAYING;
        //     if (avrcp_connected){
        //         avrcp_target_set_now_playing_info(avrcp_cid, &tracks[data_source], sizeof(tracks)/sizeof(avrcp_track_t));
        //         avrcp_target_set_playback_status(avrcp_cid, AVRCP_PLAYBACK_STATUS_PLAYING);
        //     }
        //     // a2dp_demo_timer_start(&media_tracker);
        //     printf(" AVRCP target test: Stream started.\n");
        //     break;

        // case AVDTP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
        //     // a2dp_demo_send_media_packet();
        //     break;        

        // case AVDTP_SUBEVENT_STREAM_SUSPENDED:
        //     play_info.status = AVRCP_PLAYBACK_STATUS_PAUSED;
        //     if (avrcp_connected){
        //         avrcp_target_set_playback_status(avrcp_cid, AVRCP_PLAYBACK_STATUS_PAUSED);
        //     }
        //     printf(" AVRCP target test: Stream paused.\n");
        //     // a2dp_demo_timer_stop(&media_tracker);
        //     break;

        // case AVDTP_SUBEVENT_STREAMING_CONNECTION_RELEASED:
        //     play_info.status = AVRCP_PLAYBACK_STATUS_STOPPED;
        //     cid = avdtp_subevent_stream_released_get_a2dp_cid(packet);
        //     if (cid == media_tracker.a2dp_cid) {
        //         media_tracker.stream_opened = 0;
        //         printf(" AVRCP target test: Stream released.\n");
        //     }
        //     if (avrcp_connected){
        //         avrcp_target_set_now_playing_info(avrcp_cid, NULL, sizeof(tracks)/sizeof(avrcp_track_t));
        //         avrcp_target_set_playback_status(avrcp_cid, AVRCP_PLAYBACK_STATUS_STOPPED);
        //     }
        //     // a2dp_demo_timer_stop(&media_tracker);
        //     break;
        case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);
            if (cid == media_tracker.a2dp_cid) {
                media_tracker.connected = 0;
                media_tracker.a2dp_cid = 0;
                printf(" AVRCP target test: Signaling released.\n\n");
            }
            break;
        default:
            break; 
    }
}

static void avrcp_connection_establishment_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;

    if (packet_type != HCI_EVENT_PACKET)     return;
    if (packet[0]   != HCI_EVENT_AVRCP_META) return;


    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            if (browsing_cid != 0 && browsing_cid != local_cid) {
                printf("AVRCP connection failed: expected 0x%02X l2cap cid, received 0x%02X\n", browsing_cid, local_cid);
                return;
            }

            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP connection failed: status 0x%02x\n", status);
                browsing_cid = 0;
                return;
            }
            
            avrcp_cid = local_cid;
            avrcp_connected = 1;
            avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
            printf("AVRCP connection established: avrcp_cid 0x%02x.\n", avrcp_cid);
            
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED);
            avrcp_target_support_event(avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);

            avrcp_target_set_now_playing_info(avrcp_cid, &tracks[0], sizeof(tracks)/sizeof(avrcp_track_t));

            // Set PTS default TSPX_max_avc_fragments = 10
            avrcp_controller_set_max_num_fragments(avrcp_cid, 10);

            if (auto_avrcp_browsing){
                printf("Regular AVRCP Connection -> Create AVRCP Browsing connection to addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_browsing_connect(device_addr, ertm_buffer, sizeof(ertm_buffer), &ertm_config, &browsing_cid);
            }
            break;
        }

        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP connection released: avrcp_cid 0x%02x.\n", avrcp_cid);
            reset_avrcp_context();
            return;
        
        default:
            break;
    }
}

static void avrcp_browsing_connection_establishment_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status;

    if (packet_type != HCI_EVENT_PACKET)     return;
    if (packet[0]   != HCI_EVENT_AVRCP_META) return;


    switch (packet[2]){
        case AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION:
            local_cid = avrcp_subevent_incoming_browsing_connection_get_browsing_cid(packet);
            if (browsing_cid != 0 && browsing_cid != local_cid) {
                printf("AVRCP Browsing connection failed: expected 0x%02x l2cap cid, received 0x%02X\n", browsing_cid, local_cid);
                avrcp_browsing_decline_incoming_connection(browsing_cid);
                return;
            }
            browsing_cid = local_cid;
            printf("AVRCP Browsing configure incoming connection: browsing cid 0x%02x\n", browsing_cid);
            avrcp_browsing_configure_incoming_connection(browsing_cid, ertm_buffer, sizeof(ertm_buffer), &ertm_config);
            break;

        case AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_browsing_connection_established_get_browsing_cid(packet);
            if (browsing_cid != 0 && browsing_cid != local_cid) {
                printf("AVRCP Browsing connection failed: expected 0x%02x l2cap cid, received 0x%02X\n", browsing_cid, local_cid);
                return;
            }

            status = avrcp_subevent_browsing_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Browsing connection failed: status 0x%02x\n", status);
                browsing_cid = 0;
                return;
            }
            
            browsing_cid = local_cid;
            avrcp_browsing_connected = 1;
            avrcp_subevent_browsing_connection_established_get_bd_addr(packet, event_addr);
            printf("AVRCP Browsing connected: browsing_cid 0x%02x\n", browsing_cid);

            printf("AVRCP Browsing: get media players.\n");
            media_player_item_index = -1;
            status = avrcp_browsing_controller_get_media_players(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
            
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP Browsing: Could not get players, status 0x%02x\n", status);
                break;
            }         
            browsing_state = AVRCP_BROWSING_STATE_W4_GET_PLAYERS;              
            return;
        }

        case AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED:
            printf("AVRCP Browsing released: browsing_cid 0x%02x\n", browsing_cid);
            reset_avrcp_browsing_context();
            return;
        
        default:
            break;
    }
}


static void avrcp_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t  avrcp_status = ERROR_CODE_SUCCESS;
     // printf("avrcp_target_packet_handler event 0x%02x\n", packet[2]);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_PLAY_STATUS_QUERY:
            avrcp_status = avrcp_target_play_status(avrcp_cid, play_info.song_length_ms, play_info.song_position_ms, play_info.status);            
            break;
        // case AVRCP_SUBEVENT_NOW_PLAYING_INFO_QUERY:
        //     status = avrcp_target_now_playing_info(avrcp_cid);
        //     break;
        case AVRCP_SUBEVENT_OPERATION:{
            avrcp_operation_id_t operation_id = avrcp_subevent_operation_get_operation_id(packet);
            // if (!media_tracker.connected) break;
            switch (operation_id){
                case AVRCP_OPERATION_ID_PLAY:
                    printf("AVRCP Target: received operation PLAY\n");
                    a2dp_source_start_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
                    break;
                case AVRCP_OPERATION_ID_PAUSE:
                    printf("AVRCP Target: received operation PAUSE\n");
                    a2dp_source_pause_stream(media_tracker.a2dp_cid, media_tracker.local_seid);
                    break;
                case AVRCP_OPERATION_ID_STOP:
                    printf("AVRCP Target: received operation STOP\n");
                    a2dp_source_disconnect(media_tracker.a2dp_cid);
                    break;
                case AVRCP_OPERATION_ID_VOLUME_UP:
                    printf("AVRCP Target: received operation VOLUME_UP\n");
                    break;
                case AVRCP_OPERATION_ID_VOLUME_DOWN:
                    printf("AVRCP Target: received operation VOLUME_DOWN\n");
                    break;
                case AVRCP_OPERATION_ID_REWIND:
                    printf("AVRCP Target: received operation REWIND\n");
                    break;
                case AVRCP_OPERATION_ID_FAST_FORWARD:
                    printf("AVRCP Target: received operation FAST_FORWARD\n");
                    break;
                case AVRCP_OPERATION_ID_FORWARD:
                    printf("AVRCP Target: received operation FORWARD\n");
                    break;
                case AVRCP_OPERATION_ID_BACKWARD:
                    printf("AVRCP Target: received operation BACKWARD\n");
                    break;
                case AVRCP_OPERATION_ID_SKIP:
                    printf("AVRCP Target: received operation SKIP\n");
                    break;
                case AVRCP_OPERATION_ID_MUTE:
                    printf("AVRCP Target: received operation MUTE\n");
                    break;
                case AVRCP_OPERATION_ID_CHANNEL_UP:
                    printf("AVRCP Target: received operation CHANNEL_UP\n");
                    break;
                case AVRCP_OPERATION_ID_CHANNEL_DOWN:
                    printf("AVRCP Target: received operation CHANNEL_DOWN\n");
                    break;
                case AVRCP_OPERATION_ID_SELECT:
                    printf("AVRCP Target: received operation SELECT\n");
                    break;
                case AVRCP_OPERATION_ID_UP:
                    printf("AVRCP Target: received operation UP\n");
                    break;
                case AVRCP_OPERATION_ID_DOWN:
                    printf("AVRCP Target: received operation DOWN\n");
                    break;
                case AVRCP_OPERATION_ID_LEFT:
                    printf("AVRCP Target: received operation LEFT\n");
                    break;
                case AVRCP_OPERATION_ID_RIGHT:
                    printf("AVRCP Target: received operation RIGTH\n");
                    break;
                case AVRCP_OPERATION_ID_ROOT_MENU:
                    printf("AVRCP Target: received operation ROOT_MENU\n");
                    break;
                
                default:
                    return;
            }
            break;
        }
        default:
            printf("AVRCP target: event not parsed\n");
            break;
    }

    if (avrcp_status != ERROR_CODE_SUCCESS){
        printf("Responding to AVRCP event 0x%02x failed with status 0x%02x\n", packet[2], avrcp_status);
    }
}

static void avrcp_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET)     return;
    if (packet[0]   != HCI_EVENT_AVRCP_META) return;
    
    uint16_t local_cid = little_endian_read_16(packet, 3);
    if (local_cid != avrcp_cid) return;

    int volume_percentage;
    switch (packet[2]){
        case AVRCP_SUBEVENT_NOTIFICATION_STATE:
            printf("Notification %s - %s\n", 
                avrcp_event2str(avrcp_subevent_notification_state_get_event_id(packet)), 
                avrcp_subevent_notification_state_get_enabled(packet) != 0 ? "enabled" : "disabled");
            break;
        case AVRCP_SUBEVENT_NOW_PLAYING_TRACK_INFO:
            printf("Now playing:     Track: %d\n", avrcp_subevent_now_playing_track_info_get_track(packet));
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_TOTAL_TRACKS_INFO:
            printf("Now playing:     Total Tracks: %d\n", avrcp_subevent_now_playing_total_tracks_info_get_total_tracks(packet));
            break;
        case AVRCP_SUBEVENT_NOTIFICATION_PLAYBACK_STATUS_CHANGED:
            printf("notification, playback status changed %s\n", avrcp_play_status2str(avrcp_subevent_notification_playback_status_changed_get_play_status(packet)));
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_NOW_PLAYING_CONTENT_CHANGED:
            printf("notification, playing content changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_TRACK_CHANGED:
            printf("notification track changed\n");
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
            volume_percentage = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet) * 100 / 127;
            printf("notification absolute volume changed %d %%\n", volume_percentage);
            return;
        case AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED:
            printf("notification changed\n");
            return; 
        case AVRCP_SUBEVENT_NOTIFICATION_EVENT_UIDS_CHANGED:{
            printf("UUIDS changed 0x%2x\n", avrcp_subevent_notification_event_uids_changed_get_uid_counter(packet));
            // reset to root folder
            media_element_item_index = -1;
            playable_folder_index = 0;
            folder_index = -1;
            parent_folder_set = 0;
            return;
        }
        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
            printf("%s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
            break;
        }
        case AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO:
            if (avrcp_subevent_now_playing_title_info_get_value_len(packet) > 0){
                memcpy(avrcp_value, avrcp_subevent_now_playing_title_info_get_value(packet), avrcp_subevent_now_playing_title_info_get_value_len(packet));
                printf("    Title: %s\n", avrcp_value);
            }  
            break;

        case AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO:
            if (avrcp_subevent_now_playing_artist_info_get_value_len(packet) > 0){
                memcpy(avrcp_value, avrcp_subevent_now_playing_artist_info_get_value(packet), avrcp_subevent_now_playing_artist_info_get_value_len(packet));
                printf("    Artist: %s\n", avrcp_value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO:
            if (avrcp_subevent_now_playing_album_info_get_value_len(packet) > 0){
                memcpy(avrcp_value, avrcp_subevent_now_playing_album_info_get_value(packet), avrcp_subevent_now_playing_album_info_get_value_len(packet));
                printf("    Album: %s\n", avrcp_value);
            }  
            break;
        
        case AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO:
            if (avrcp_subevent_now_playing_genre_info_get_value_len(packet) > 0){
                memcpy(avrcp_value, avrcp_subevent_now_playing_genre_info_get_value(packet), avrcp_subevent_now_playing_genre_info_get_value_len(packet));
                printf("    Genre: %s\n", avrcp_value);
            }  
            break;
        
        case AVRCP_SUBEVENT_PLAY_STATUS:
            printf("song length: %d ms, song position: %d ms, play status: %s\n", 
                avrcp_subevent_play_status_get_song_length(packet), 
                avrcp_subevent_play_status_get_song_position(packet),
                avrcp_play_status2str(avrcp_subevent_play_status_get_play_status(packet)));
            break;
        case AVRCP_SUBEVENT_OPERATION_COMPLETE:
            printf("operation done %s\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        case AVRCP_SUBEVENT_OPERATION_START:
            printf("operation start %s\n", avrcp_operation2str(avrcp_subevent_operation_complete_get_operation_id(packet)));
            break;
        case AVRCP_SUBEVENT_PLAYER_APPLICATION_VALUE_RESPONSE:
            // response to set shuffle and repeat mode
            printf("\n");
            break;
        case AVRCP_SUBEVENT_SET_ABSOLUTE_VOLUME_RESPONSE:
            volume_percentage = avrcp_subevent_set_absolute_volume_response_get_absolute_volume(packet) * 100 / 127;
            printf("absolute volume response %d %%\n", volume_percentage);
            break;
        case AVRCP_SUBEVENT_NOW_PLAYING_INFO_DONE:
            printf("Playing info done with receiving\n");
            break;
        case AVRCP_SUBEVENT_CUSTOM_COMMAND_RESPONSE:
            printf("Custom command response: pdu id 0x%2X, param len %d\n", avrcp_subevent_custom_command_response_get_pdu_id(packet), avrcp_subevent_custom_command_response_get_params_len(packet));
            printf_hexdump(avrcp_subevent_custom_command_response_get_params(packet), avrcp_subevent_custom_command_response_get_params_len(packet));
            break;
        default:
            printf("AVRCP controller: event not parsed 0x%02x\n", packet[2]);
            break;
    }             
}


static void avrcp_browsing_target_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t  avrcp_status = ERROR_CODE_SUCCESS;
    // printf("avrcp_browsing_target_packet_handler event 0x%02x\n", packet[2]);
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    
    switch (packet[2]){
        case AVRCP_SUBEVENT_BROWSING_SET_BROWSED_PLAYER:{
            uint16_t browsed_player_id =  avrcp_subevent_browsing_set_browsed_player_get_player_id(packet);
            if (browsed_player_id == 1){
                avrcp_status = avrcp_browsing_target_send_accept_set_browsed_player(browsing_cid, uid_counter, browsed_player_id, browsed_player_data, sizeof(browsed_player_data));
            } else {
                avrcp_status = avrcp_browsing_target_send_reject_set_browsed_player(browsing_cid, AVRCP_STATUS_PLAYER_NOT_BROWSABLE);
            }
            break;
        }
        case AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS:{
            avrcp_browsing_scope_t scope = avrcp_subevent_browsing_get_folder_items_get_scope(packet);
            switch (scope){
                case AVRCP_BROWSING_MEDIA_PLAYER_LIST:
                    printf("Send media player list: \n");
                    char name[30];
                    uint16_t pos = 2;
                    for (int i = 0; i < 2; i++){
                        memset(name, 0, sizeof(name));
                        pos += 29;
                        uint16_t name_size = big_endian_read_16(media_player_list , pos);
                        pos += 2;
                        memcpy(name, media_player_list+pos, name_size);
                        pos += name_size;
                        printf(" - %s\n", name);
                    }
                    
                    avrcp_status = avrcp_browsing_target_send_get_folder_items_response(browsing_cid, uid_counter, media_player_list, sizeof(media_player_list));
                    break;
                case AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM:
                    avrcp_status = avrcp_browsing_target_send_get_folder_items_response(browsing_cid, uid_counter, virtual_filesystem_list, sizeof(virtual_filesystem_list));
                    break;
                case AVRCP_BROWSING_SEARCH:
                    avrcp_status = avrcp_browsing_target_send_get_folder_items_response(browsing_cid, uid_counter, search_list, sizeof(search_list));
                    break;
                case AVRCP_BROWSING_NOW_PLAYING:
                    avrcp_status = avrcp_browsing_target_send_get_folder_items_response(browsing_cid, uid_counter, now_playing_list, sizeof(now_playing_list));
                    break;
            }
            break;
        }
        case AVRCP_SUBEVENT_BROWSING_GET_TOTAL_NUM_ITEMS:{
            avrcp_browsing_scope_t scope = avrcp_subevent_browsing_get_folder_items_get_scope(packet);
            uint32_t total_num_items = 0;
            switch (scope){
                case AVRCP_BROWSING_MEDIA_PLAYER_LIST:
                    total_num_items = big_endian_read_16(media_player_list, 0);
                    avrcp_status = avrcp_browsing_target_send_get_total_num_items_response(browsing_cid, uid_counter, total_num_items);
                    break;
                case AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM:
                    total_num_items = big_endian_read_16(virtual_filesystem_list, 0);
                    avrcp_status = avrcp_browsing_target_send_get_total_num_items_response(browsing_cid, uid_counter, total_num_items);
                    break;
                default:
                    avrcp_status = avrcp_browsing_target_send_get_total_num_items_response(browsing_cid, uid_counter, total_num_items);
                    break;
            }
            break;
        }
        
        default:
            printf("AVRCP Browsing Target: event 0x%02x not parsed \n", packet[2]);
            break;
    }

    if (avrcp_status != ERROR_CODE_SUCCESS){
        printf("Responding to AVRCP event 0x%02x failed with status 0x%02x\n", packet[2], avrcp_status);
    }
}

static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint8_t  status;
    uint16_t pos = 0;
    
    // printf("avrcp_browsing_controller_packet_handler packet type 0x%02X, subevent 0x%02X\n", packet_type, packet[2]);
    switch (packet_type) {   
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case HCI_EVENT_AVRCP_META:
                    if (packet[2] != AVRCP_SUBEVENT_BROWSING_DONE) return;
                    
                    browsing_query_active = 0;
                    browsing_uid_counter = 0;
                    if (avrcp_subevent_browsing_done_get_browsing_status(packet) != AVRCP_BROWSING_ERROR_CODE_SUCCESS){
                        printf("AVRCP Browsing query done with browsing status 0x%02x, bluetooth status 0x%02x.\n", 
                            avrcp_subevent_browsing_done_get_browsing_status(packet),
                            avrcp_subevent_browsing_done_get_bluetooth_status(packet));
                        return;    
                    }
                    browsing_uid_counter = avrcp_subevent_browsing_done_get_uid_counter(packet);
                    printf("DONE, browsing_uid_counter %d.\n", browsing_uid_counter);
                    
                    switch (browsing_state){
                        case AVRCP_BROWSING_STATE_W4_GET_PLAYERS:
                            if (media_player_item_index < 0) {
                                printf("Get media players first\n");
                                break;
                            }
                            printf("Set browsed player\n");
                            browsing_state = AVRCP_BROWSING_STATE_W4_SET_PLAYER;
                            status = avrcp_browsing_controller_set_browsed_player(browsing_cid, media_player_items[0].player_id);
                            if (status != ERROR_CODE_SUCCESS){
                                printf("Could not set player, status 0x%02X\n", status);
                                status = AVRCP_BROWSING_STATE_W4_GET_PLAYERS;
                                break;
                            }         
                            break;
                        case AVRCP_BROWSING_STATE_W4_SET_PLAYER:
                            browsing_state = AVRCP_BROWSING_STATE_READY;
                            break;
                        default:
                            break; 
                    }
                    break;
                default:
                    break;
            }
            break;

        case AVRCP_BROWSING_DATA_PACKET:
            browsing_query_active = 1;
            avrcp_browsing_item_type_t data_type = (avrcp_browsing_item_type_t)packet[pos++];

            switch (data_type){
                case AVRCP_BROWSING_MEDIA_ROOT_FOLDER:{
                    uint16_t folder_name_length = size - pos;
                    char folder_name[AVRCP_MAX_FOLDER_NAME_SIZE];
                    memcpy(folder_name, &packet[pos], folder_name_length);
                    folder_name[folder_name_length] = 0;
                    printf("Found root folder: name %s \n", folder_name);
                    break;
                }

                case AVRCP_BROWSING_FOLDER_ITEM:{
                    int index = next_folder_index();
                    memcpy(folders[index].uid, packet+pos, 8);
                
                    uint32_t folder_uid_high = big_endian_read_32(packet, pos);
                    pos += 4;
                    uint32_t folder_uid_low  = big_endian_read_32(packet, pos);
                    pos += 4;
                    avrcp_browsing_folder_type_t folder_type = packet[pos++];
                    uint8_t  is_playable = packet[pos++];
                    uint16_t charset = big_endian_read_16(packet, pos);
                    pos += 2;
                    uint16_t displayable_name_length = big_endian_read_16(packet, pos);
                    pos += 2;
                    
                    char value[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
                    memset(value, 0, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN);
                    uint16_t value_len = btstack_min(displayable_name_length, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN - 1); 
                    memcpy(value, packet+pos, value_len);

                    printf("Folder UID 0x%08" PRIx32 "%08" PRIx32 ", type 0x%02x, is_playable %d, charset 0x%02x, name %s\n", 
                        folder_uid_high, folder_uid_low, folder_type, is_playable, charset, value);
                    if (is_playable){
                        playable_folder_index = index;
                    }
                    memcpy(folders[index].name, value, value_len);
                    folders[index].name_len = value_len;
                    break;
                }

                case AVRCP_BROWSING_MEDIA_PLAYER_ITEM:{
                    printf("Received media player item:  ");
                    uint16_t player_id = big_endian_read_16(packet, pos);
                    pos += 2;
                    avrcp_browsing_media_player_major_type_t major_type = packet[pos++];
                    avrcp_browsing_media_player_subtype_t subtype = big_endian_read_32(packet, pos);
                    pos += 4;
                    status = packet[pos++];
                    uint8_t feature_bitmask[16];
                    memcpy(feature_bitmask, packet, 16);
                    pos += 16;
                    printf("player ID 0x%04x, major_type %d, subtype %d, status %d\n", player_id, major_type, subtype, status);
                    media_player_items[next_media_player_item_index()].player_id = player_id;
                    break;
                }
                
                case AVRCP_BROWSING_MEDIA_ELEMENT_ITEM:{
                    int index = next_media_element_item_index();
                    memcpy(media_element_items[index].uid, packet+pos, 8);
                    printf("Received media element item UID (index %d): ", index);
                    
                    // uint32_t media_uid_high = big_endian_read_32(packet, pos);
                    pos += 4;
                    // uint32_t media_uid_low  = big_endian_read_32(packet, pos+4);
                    pos += 4;
                    
                    avrcp_browsing_media_type_t media_type = packet[pos++];
                    uint16_t charset = big_endian_read_16(packet, pos);
                    pos += 2;
                    uint16_t displayable_name_length = big_endian_read_16(packet, pos);
                    pos += 2;
                    
                    char value[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];
                    memset(value, 0, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN);
                    uint16_t value_len = btstack_min(displayable_name_length, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN - 1); 
                    memcpy(value, packet+pos, value_len);
                    memcpy(media_element_items[index].name, value, value_len);

                    pos += displayable_name_length;
                    // printf("Media UID: 0x%08" PRIx32 "%08" PRIx32 ", media_type 0x%02x, charset 0x%02x, actual len %d, name %s\n", media_uid_high, media_uid_low, media_type, charset, value_len, value);
                    
                    printf_hexdump(media_element_items[index].uid, 8);
                    uint8_t num_attributes = packet[pos++];
                    printf("  Media type 0x%02x, charset 0x%02x, actual len %d, name %s, num attributes %d:\n", media_type, charset, value_len, value, num_attributes);
                    
                    avrcp_media_item_context_t media_item_context;
                    for (avrcp_media_item_iterator_init(&media_item_context, size-pos, packet+pos); avrcp_media_item_iterator_has_more(&media_item_context); avrcp_media_item_iterator_next(&media_item_context)){
                        uint32_t attr_id            = avrcp_media_item_iterator_get_attr_id(&media_item_context);
                        uint16_t attr_charset       = avrcp_media_item_iterator_get_attr_charset(&media_item_context);
                        uint16_t attr_value_length  = avrcp_media_item_iterator_get_attr_value_len(&media_item_context);
                        const uint8_t * attr_value  = avrcp_media_item_iterator_get_attr_value(&media_item_context);
                        
                        memset(value, 0, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN);
                        value_len = btstack_min(attr_value_length, AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN - 1); 
                        memcpy(value, attr_value, value_len);
                        
                        printf("    - attr ID 0x%08" PRIx32 ", charset 0x%02x, actual len %d, name %s\n", attr_id, attr_charset, value_len, value);
                    }
                    break;
                }

                case AVRCP_BROWSING_MEDIA_ELEMENT_ITEM_ATTRIBUTE:{
                    uint8_t num_attributes = packet[pos++];
                    printf("Num media attributes %d:\n", num_attributes);
                    avrcp_media_item_context_t media_item_context;
                    for (avrcp_media_item_iterator_init(&media_item_context, size-pos, packet+pos); avrcp_media_item_iterator_has_more(&media_item_context); avrcp_media_item_iterator_next(&media_item_context)){
                        uint32_t attr_id            = avrcp_media_item_iterator_get_attr_id(&media_item_context);
                        uint16_t attr_charset       = avrcp_media_item_iterator_get_attr_charset(&media_item_context);
                        uint16_t attr_value_length  = avrcp_media_item_iterator_get_attr_value_len(&media_item_context);
                        const uint8_t * attr_value  = avrcp_media_item_iterator_get_attr_value(&media_item_context);
                        printf("    - attr ID 0x%08" PRIx32 ", charset 0x%02x, actual len %d, name %s\n", attr_id, attr_charset, attr_value_length, attr_value);
                    }
                }
                default:
                    printf("AVRCP browsing: unknown browsable item type 0%02x\n", data_type);
                    break;
            }
            break;
        default:
            break;
    }
}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVDTP Sink/AVRCP Connection Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("a      - AVDTP Sink create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("A      - AVDTP Sink disconnect\n");
    printf("b      - AVDTP Source create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("B      - AVDTP Sink disconnect\n");
    
    printf("c      - AVRCP create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("C      - AVRCP disconnect\n");
    printf("d      - AVRCP Browsing Controller create connection to addr %s\n", bd_addr_to_str(device_addr_browsing));
    printf("D      - AVRCP Browsing Controller disconnect\n");
    printf("#3     - trigger outgoing regular AVRCP connection on incoming HCI Connection to test reject and retry\n");
    printf("#4     - trigger outgoing browsing AVRCP connection on incoming AVRCP connection to test reject and retry\n");

    printf("\n--- Bluetooth AVRCP Commands %s ---\n", bd_addr_to_str(iut_address));
    printf("q - get capabilities: supported events\n");
    printf("w - get unit info\n");
    printf("* - get subunit info\n");
    printf("r - get play status\n");
    printf("/ - get now playing info\n");
    printf("$ - get TITLE of now playing song\n");

    printf("01 - play\n");
    printf("02 - pause\n");
    printf("03 - stop\n");
    printf("04 - rewind\n");
    printf("05 - fast forward\n");
    printf("06 - forward\n");
    printf("07 - backward\n");
    printf("08 - volume up\n");
    printf("09 - volume down\n");
    printf("00 - mute\n");

    printf("11 - press and hold: play\n");
    printf("12 - press and hold: pause\n");
    printf("13 - press and hold: stop\n");
    printf("14 - press and hold: rewind\n");
    printf("15 - press and hold: fast forward\n");
    printf("16 - press and hold: forward\n");
    printf("17 - press and hold: backward\n");
    printf("18 - press and hold: volume up\n");
    printf("19 - press and hold: volume down\n");
    printf("10 - press and hold: mute\n");
    printf("2  - release press and hold cmd\n");

    printf("R - absolute volume of 50 percent\n");
    printf("u - skip\n");
    printf(". - query repeat and shuffle mode\n");
    printf("o - repeat single track\n");
    printf("x/X - repeat/disable repeat all tracks\n");
    printf("z/Z - shuffle/disable shuffle all tracks\n");
    
    printf("i/I - register/deregister PLAYBACK_STATUS_CHANGED\n");
    printf("s/S - register/deregister TRACK_CHANGED\n");
    printf("e/E - register/deregister TRACK_REACHED_END\n");
    printf("f/F - register/deregister TRACK_REACHED_START\n");
    printf("g/G - register/deregister PLAYBACK_POS_CHANGED\n");
    printf("h/H - register/deregister BATT_STATUS_CHANGED\n");
    printf("j/J - register/deregister SYSTEM_STATUS_CHANGED\n");
    printf("k/K - register/deregister PLAYER_APPLICATION_SETTING_CHANGED\n");
    printf("l/L - register/deregister NOW_PLAYING_CONTENT_CHANGED\n");
    printf("m/M - register/deregister AVAILABLE_PLAYERS_CHANGED\n");
    printf("n/N - register/deregister ADDRESSED_PLAYER_CHANGED\n");
    printf("y/Y - register/deregister UIDS_CHANGED\n");
    printf("v/V - register/deregister VOLUME_CHANGED\n");

    printf("pp - get media players. Browsing cid 0x%02X\n", browsing_cid);
    printf("pI - Set addressed player\n");
    printf("pO - Set browsed player\n");
    
    printf("pW - go up one level\n");
    printf("pT - go down one level of %s\n", (char *)parent_folder_name);
    
    printf("pQ - browse folders\n");
    printf("pP - browse media items\n");
    printf("pj - browse now playing items\n");
    printf("ps - browse search folder\n");
    printf("pn - search 3\n");
    
    printf("pm - Set max num fragments to 0x02\n");
    printf("pM - Set max num fragments to 0xFF\n");
    
    printf("p1 - Get item attributes of first media element for virtual file system scope\n");
    printf("p2 - Get item attributes of first media element for now playing scope\n");
    printf("p3 - Get item attributes of first media element for search scope\n");
    printf("p4 - Get item attributes of first media element for media player list scope\n");
    
    printf("pl - Get total num items for media player list scope\n");
    printf("pL - Get total num items for now playing scope\n");
    printf("pS - Get total num items for search scope\n");

    printf("pi - Play first media item for virtual filesystem scope \n");
    printf("pt - Play first media item for search scope \n");
    printf("pr - Play first media item for now playing scope\n");

    printf("p5 - Add to now playing: first media item from virtual file system\n");
    printf("p6 - Add to now playing: first media item from search folder\n");

    printf("pf - Send fragmented (long) command\n");

    printf("tv - trigger notification VOLUME_CHANGED 20percent \n");
    printf("tt - trigger notification TRACK_CHANGED\n");
    printf("tb - trigger notification BATT_STATUS_CHANGED\n");
    printf("tp - trigger notification NOW_PLAYING_CONTENT_CHANGED\n");

    printf("ts - select track with short info\n");
    printf("tl - select track with long info\n");

    printf("Ctrl-c - exit\n");
    printf("---\n");
}

#endif

static uint8_t media_sbc_codec_capabilities[] = {
    0xFF,//(AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    0xFF,//(AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 53
}; 

#ifdef HAVE_BTSTACK_STDIN

static void stdin_process(char * cmd, int size){
    UNUSED(size);
    uint8_t status = ERROR_CODE_SUCCESS;
    sep.seid = 1;
    
    switch (cmd[0]){
        case 'a':
            printf("Establish AVDTP sink connection to %s\n", bd_addr_to_str(device_addr));
            avdtp_sink_connect(device_addr, &avdtp_cid);
            break;
        case 'A':
            printf("Disconnect AVDTP sink\n");
            avdtp_sink_disconnect(avdtp_cid);
            break;
        case 'b':
            printf("Establish AVDTP Source connection to %s\n", bd_addr_to_str(device_addr));
            avdtp_source_connect(device_addr, &avdtp_cid);
            break;
        case 'B':
             printf("Disconnect AVDTP Source\n");
            avdtp_source_disconnect(avdtp_cid);
            break;
        case 'c':
            printf("Establish AVRCP connection to %s.\n", bd_addr_to_str(device_addr));
            avrcp_connect(device_addr, &avrcp_cid);
            break;
        case 'C':
            printf("Disconnect AVRCP\n");
            avrcp_disconnect(avrcp_cid);
            break;

         case 'd':
            if (!avrcp_connected) {
                printf(" You must first create AVRCP connection for control to addr %s.\n", bd_addr_to_str(device_addr));
                break;
            }
            printf(" - Create AVRCP connection for browsing to addr %s.\n", bd_addr_to_str(device_addr_browsing));
            status = avrcp_browsing_connect(device_addr_browsing, ertm_buffer, sizeof(ertm_buffer), &ertm_config, &browsing_cid);
            break;
        case 'D':
            if (avrcp_browsing_connected){
                printf(" - AVRCP Browsing Controller disconnect from addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_browsing_disconnect(browsing_cid);
                break;
            }
            printf("AVRCP Browsing Controller already disconnected\n");
            break;
        case '#':
            switch (cmd[1]){
                case '3':
                    printf("- Auto-connect AVRCP on incoming HCI connection enabled\n");
                    auto_avrcp_regular = true;
                    break;
                case '4':
                    printf("- Auto-connect AVRCP Browsing on AVRCP Connection enabled\n");
                    auto_avrcp_browsing = true;
                    break;
                default:
                    break;
            }
            break;
        
        case '\n':
        case '\r':
            break;
        case 'q': 
            printf("AVRCP: get capabilities: supported events\n");
            avrcp_controller_get_supported_events(avrcp_cid);
            break;
        case 'w':
            printf("AVRCP: get unit info\n");
            avrcp_controller_unit_info(avrcp_cid);
            break;
        case '*':
            printf("AVRCP: get subunit info\n");
            avrcp_controller_subunit_info(avrcp_cid);
            break;
        case 'r':
            printf("AVRCP: get play status\n");
            avrcp_controller_get_play_status(avrcp_cid);
            break;
        case '/':
            printf("AVRCP: get now playing info\n");
            avrcp_controller_get_now_playing_info_for_media_attribute_id(avrcp_cid, AVRCP_MEDIA_ATTR_TITLE);
            break;
        case '$':
            printf("AVRCP: get TITLE of now playing song\n");
            avrcp_controller_get_element_attributes(avrcp_cid, sizeof(now_playing_info_attributes)/4, now_playing_info_attributes);
            break;

        case '0':
            switch (cmd[1]){
                case '1':
                    printf("AVRCP: play\n");
                    avrcp_controller_play(avrcp_cid);
                    break;
                case '2':
                    printf("AVRCP: pause\n");
                    avrcp_controller_pause(avrcp_cid);
                    break;
                case '3':
                    printf("AVRCP: stop\n");
                    avrcp_controller_stop(avrcp_cid);
                    break;
                case '4':
                    printf("AVRCP: rewind\n");
                    avrcp_controller_rewind(avrcp_cid);
                    break;
                case '5':
                    printf("AVRCP: fast forward\n");
                    avrcp_controller_fast_forward(avrcp_cid);
                    break;
                case '6':
                    printf("AVRCP: forward\n");
                    avrcp_controller_forward(avrcp_cid); 
                    break;
                case '7':
                    printf("AVRCP: backward\n");
                    avrcp_controller_backward(avrcp_cid);
                    break;
                case '8':
                    printf("AVRCP: volume up\n");
                    avrcp_controller_volume_up(avrcp_cid);
                    break;
                case '9':
                    printf("AVRCP: volume down\n");
                    avrcp_controller_volume_down(avrcp_cid);
                    break;
                case '0':
                    printf("AVRCP: mute\n");
                    avrcp_controller_mute(avrcp_cid);
                    break;
                default:
                    break;
            }
            break;
         
         case '1':
            switch (cmd[1]){
                case '1':
                    printf("AVRCP: play\n");
                    avrcp_controller_press_and_hold_play(avrcp_cid);
                    break;
                case '2':
                    printf("AVRCP: pause\n");
                    avrcp_controller_press_and_hold_pause(avrcp_cid);
                    break;
                case '3':
                    printf("AVRCP: stop\n");
                    avrcp_controller_press_and_hold_stop(avrcp_cid);
                    break;
                case '4':
                    printf("AVRCP: rewind\n");
                    avrcp_controller_press_and_hold_rewind(avrcp_cid);
                    break;
                case '5':
                    printf("AVRCP: fast forward\n");
                    avrcp_controller_press_and_hold_fast_forward(avrcp_cid);
                    break;
                case '6':
                    printf("AVRCP: forward\n");
                    avrcp_controller_press_and_hold_forward(avrcp_cid); 
                    break;
                case '7':
                    printf("AVRCP: backward\n");
                    avrcp_controller_press_and_hold_backward(avrcp_cid);
                    break;
                case '8':
                    printf("AVRCP: volume up\n");
                    avrcp_controller_press_and_hold_volume_up(avrcp_cid);
                    break;
                case '9':
                    printf("AVRCP: volume down\n");
                    avrcp_controller_press_and_hold_volume_down(avrcp_cid);
                    break;
                case '0':
                    printf("AVRCP: mute\n");
                    avrcp_controller_press_and_hold_mute(avrcp_cid);
                    break;
                default:
                    break;
            }
            break;

         case '2':
            avrcp_controller_release_press_and_hold_cmd(avrcp_cid);
            break;
        case 'R':
            printf("AVRCP: absolute volume of 50 percent\n");
            avrcp_controller_set_absolute_volume(avrcp_cid, 50);
            break;
        case 'u':
            printf("AVRCP: skip\n");
            avrcp_controller_skip(avrcp_cid);
            break;
        case '.':
            printf("AVRCP: query repeat and shuffle mode\n");
            avrcp_controller_query_shuffle_and_repeat_modes(avrcp_cid);
            break;
        case 'o':
            printf("AVRCP: repeat single track\n");
            avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_SINGLE_TRACK);
            break;
        case 'x':
            printf("AVRCP: repeat all tracks\n");
            avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_ALL_TRACKS);
            break;
        case 'X':
            printf("AVRCP: disable repeat mode\n");
            avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_OFF);
            break;
        case 'z':
            printf("AVRCP: shuffle all tracks\n");
            avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_ALL_TRACKS);
            break;
        case 'Z':
            printf("AVRCP: disable shuffle mode\n");
            avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_OFF);
            break;

        case 'i':
            printf("AVRCP: enable notification PLAYBACK_STATUS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            break;
        case 's':
            printf("AVRCP: enable notification TRACK_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'e':
            printf("AVRCP: enable notification TRACK_REACHED_END\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END);
            break;
        case 'f':
            printf("AVRCP: enable notification TRACK_REACHED_START\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START);
            break;
        case 'g':
            printf("AVRCP: enable notification PLAYBACK_POS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;
        case 'h':
            printf("AVRCP: enable notification BATT_STATUS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            break;
        case 'j':
            printf("AVRCP: enable notification SYSTEM_STATUS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED);
            break;
        case 'k':
            printf("AVRCP: enable notification PLAYER_APPLICATION_SETTING_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED);
            break;
        case 'l':
            printf("AVRCP: enable notification NOW_PLAYING_CONTENT_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            break;
        case 'm':
            printf("AVRCP: enable notification AVAILABLE_PLAYERS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED);
            break;
        case 'n':
            printf("AVRCP: enable notification ADDRESSED_PLAYER_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED);
            break;
        case 'y':
            printf("AVRCP: enable notification UIDS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED);
            break;
        case 'v':
            printf("AVRCP: enable notification VOLUME_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            break;

        case 'I':
            printf("AVRCP: disable notification PLAYBACK_STATUS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            break;
        case 'S':
            printf("AVRCP: disable notification TRACK_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'E':
            printf("AVRCP: disable notification TRACK_REACHED_END\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_END);
            break;
        case 'F':
            printf("AVRCP: disable notification TRACK_REACHED_START\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_REACHED_START);
            break;
        case 'G':
            printf("AVRCP: disable notification PLAYBACK_POS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_POS_CHANGED);
            break;
        case 'H':
            printf("AVRCP: disable notification BATT_STATUS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_BATT_STATUS_CHANGED);
            break;
        case 'J':
            printf("AVRCP: disable notification SYSTEM_STATUS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_SYSTEM_STATUS_CHANGED);
            break;
        case 'K':
            printf("AVRCP: disable notification PLAYER_APPLICATION_SETTING_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYER_APPLICATION_SETTING_CHANGED);
            break;
        case 'L':
            printf("AVRCP: disable notification NOW_PLAYING_CONTENT_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
            break;
        case 'M':
            printf("AVRCP: disable notification AVAILABLE_PLAYERS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_AVAILABLE_PLAYERS_CHANGED);
            break;
        case 'N':
            printf("AVRCP: disable notification ADDRESSED_PLAYER_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_ADDRESSED_PLAYER_CHANGED);
            break;
        case 'Y':
            printf("AVRCP: disable notification UIDS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_UIDS_CHANGED);
            break;
        case 'V':
            printf("AVRCP: disable notification VOLUME_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
            break;

       
        
        case 'p':
            switch (cmd[1]){
                 case 'p':
                    printf("AVRCP Browsing: get media players. Browsing cid 0x%02X\n", browsing_cid);
                    media_player_item_index = -1;
                    status = avrcp_browsing_controller_get_media_players(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                 case 'I':
                    if (media_player_item_index < 0) {
                        printf("Get media players first\n");
                        break;
                    }
                    printf("Set addressed player\n");
                    status = avrcp_controller_set_addressed_player(avrcp_cid, media_player_items[0].player_id);
                    break;
                case 'O':
                    if (media_player_item_index < 0) {
                        printf("Get media players first\n");
                        break;
                    }
                    printf("Set browsed player\n");
                    status = avrcp_browsing_controller_set_browsed_player(browsing_cid, media_player_items[0].player_id);
                    break;
                case 'Q':
                    printf("AVRCP Browsing: browse folders\n");
                    playable_folder_index = 0;
                    folder_index = -1;
                    status = avrcp_browsing_controller_browse_file_system(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'P':
                    printf("AVRCP Browsing: browse media items\n");
                    avrcp_browsing_controller_browse_media(browsing_cid, 1, 10, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'W':
                    printf("AVRCP Browsing: go up one level\n");
                    status = avrcp_browsing_controller_go_up_one_level(browsing_cid);
                    folder_index = -1;
                    break;
                case 'T':
                    if (folder_index < 0 && !parent_folder_set){
                        printf("AVRCP Browsing: no folders available\n");
                        break;
                    }
                    if (!parent_folder_set){
                        parent_folder_set = 1;
                        memcpy(parent_folder_name, folders[playable_folder_index].name, folders[playable_folder_index].name_len);
                        memcpy(parent_folder_uid, folders[playable_folder_index].uid, 8);
                    }
                    printf("AVRCP Browsing: go down one level of %s\n", (char *)parent_folder_name);
                    status = avrcp_browsing_controller_go_down_one_level(browsing_cid, parent_folder_uid);
                    folder_index = -1;
                    break;
                
                case 'j':
                    printf("AVRCP Browsing: browse now playing items\n");
                    playable_folder_index = 0;
                    folder_index = -1;
                    status = avrcp_browsing_controller_browse_now_playing_list(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'n':
                    printf("AVRCP Browsing: search 3\n");
                    status = avrcp_browsing_controller_search(browsing_cid, 1, "3");
                    break;
                case 's':
                    printf("AVRCP Browsing: browse search folder\n");
                    media_element_item_index = -1;
                    status = avrcp_browsing_controller_browse_media(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'm':
                    printf("Set max num fragments to 0x02\n");
                    status = avrcp_controller_set_max_num_fragments(avrcp_cid, 0x02);
                    break;
                case 'M':
                    printf("Set max num fragments to 0xFF\n");
                    status = avrcp_controller_set_max_num_fragments(avrcp_cid, 0xFF);
                    break;
                case 'A':
                    if (folder_index < 0 && !parent_folder_set){
                        printf("AVRCP Browsing: no folders available\n");
                        break;
                    }
                    if (!parent_folder_set){
                        parent_folder_set = 1;
                        memcpy(parent_folder_name, folders[playable_folder_index].name, folders[playable_folder_index].name_len);
                        memcpy(parent_folder_uid, folders[playable_folder_index].uid, 8);
                    }
                    printf("AVRCP Browsing: go down one level of %s\n", (char *)parent_folder_name);
                    status = avrcp_browsing_controller_go_down_one_level(browsing_cid, parent_folder_uid);
                    folder_index = -1;
                    break;
                case '1':
                    printf("Get item attributes of first media item for virtual file system scope\n");
                    avrcp_browsing_controller_get_item_attributes_for_scope(browsing_cid, media_element_items[0].uid, browsing_uid_counter, 1 << AVRCP_MEDIA_ATTR_TITLE, AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM);
                    break;
                case '2':
                    printf("Get item attributes of first media item for now playing scope\n");
                    avrcp_browsing_controller_get_item_attributes_for_scope(browsing_cid, media_element_items[0].uid, browsing_uid_counter, 1 << AVRCP_MEDIA_ATTR_TITLE, AVRCP_BROWSING_NOW_PLAYING);
                    break;
                case '3':
                    printf("Get item attributes of first media item for search scope\n");
                    avrcp_browsing_controller_get_item_attributes_for_scope(browsing_cid, media_element_items[0].uid, browsing_uid_counter, 1 << AVRCP_MEDIA_ATTR_TITLE, AVRCP_BROWSING_SEARCH);
                    break;
                case '4':
                    printf("Get item attributes of first media item for media player list scope\n");
                    avrcp_browsing_controller_get_item_attributes_for_scope(browsing_cid, media_element_items[0].uid, browsing_uid_counter, 1 << AVRCP_MEDIA_ATTR_TITLE, AVRCP_BROWSING_MEDIA_PLAYER_LIST);
                    break;
                
                case 'l': 
                    printf("Get total num items for media player list scope\n");
                    status = avrcp_browsing_controller_get_total_nr_items_for_scope(browsing_cid, AVRCP_BROWSING_MEDIA_PLAYER_LIST);
                    break;
                case 'L': 
                    printf("Get total num items for now playing scope.\n");
                    status = avrcp_browsing_controller_get_total_nr_items_for_scope(browsing_cid, AVRCP_BROWSING_NOW_PLAYING);
                    break;
                case 'S': 
                    printf("Get total num items for search scope.\n");
                    status = avrcp_browsing_controller_get_total_nr_items_for_scope(browsing_cid, AVRCP_BROWSING_SEARCH);
                    break;
                
                case 'i':
                    printf("Play first media item for virtual filesystem scope\n");
                    if (media_element_item_index < 0){
                        printf("AVRCP Browsing: no media items found\n");
                        break;
                    }
                    // printf_hexdump(media_element_items[0].uid, 8);
                    status = avrcp_controller_play_item_for_scope(avrcp_cid, media_element_items[0].uid, media_element_item_index, AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM);
                    break;
                case 't':
                    printf("Play first media item for search scope\n");
                    if (media_element_item_index < 0){
                        printf("AVRCP Browsing: no media items found\n");
                        break;
                    }
                    // printf_hexdump(media_element_items[0].uid, 8);
                    status = avrcp_controller_play_item_for_scope(avrcp_cid, media_element_items[0].uid, media_element_item_index, AVRCP_BROWSING_SEARCH);
                    break;
                case 'r':
                    printf("Play first media item for now playing scope\n");
                    if (media_element_item_index < 0){
                        printf("AVRCP Browsing: no media items found\n");
                        break;
                    }
                    // printf_hexdump(media_element_items[0].uid, 8);
                    status = avrcp_controller_play_item_for_scope(avrcp_cid, media_element_items[0].uid, media_element_item_index, AVRCP_BROWSING_NOW_PLAYING);
                    break;

                case '5':
                    printf("Add to now playing: first media item from virtual file system\n");
                    if (media_element_item_index < 0){
                        printf("AVRCP Browsing: no media items found\n");
                        break;
                    }
                    // printf_hexdump(media_element_items[0].uid, 8);
                    status = avrcp_controller_add_item_from_scope_to_now_playing_list(avrcp_cid, media_element_items[0].uid, media_element_item_index, AVRCP_BROWSING_MEDIA_PLAYER_VIRTUAL_FILESYSTEM);
                    break;

                case '6':
                    printf("Add to now playing: first media item from search folder\n");
                    if (media_element_item_index < 0){
                        printf("AVRCP Browsing: no media items found\n");
                        break;
                    }
                    // printf_hexdump(media_element_items[0].uid, 8);
                    status = avrcp_controller_add_item_from_scope_to_now_playing_list(avrcp_cid, media_element_items[0].uid, media_element_item_index, AVRCP_BROWSING_SEARCH);
                    break;

                case 'f':
                    printf("Send fragmented command\n");
                    status = avrcp_controller_send_custom_command(avrcp_cid, AVRCP_CTYPE_CONTROL, 
                        AVRCP_SUBUNIT_TYPE_PANEL, AVRCP_SUBUNIT_ID, 
                        0xAA, BT_SIG_COMPANY_ID,
                        fragmented_message, sizeof(fragmented_message));
                    break;

                default:
                    break;
            }
            break;

            case 't':
                switch(cmd[1]){
                    case 'v':
                        printf("AVRCP: trigger notification VOLUME_CHANGED 20per\n");
                        avrcp_target_volume_changed(avrcp_cid, 20);
                        break;
                    case 't':{
                        printf("AVRCP: trigger notification TRACK_CHANGED\n");
                        uint8_t track_id[8] = {0x00, 0x03, 0x02, 0x01, 0x00, 0x03, 0x02, 0x01};
                        avrcp_target_track_changed(avrcp_cid, track_id);
                        break;
                    }
                    case 'T':
                        printf("AVRCP: Unselect track\n");
                        avrcp_target_track_changed(avrcp_cid, NULL);
                        break;
                    case 'b':
                        printf("AVRCP: trigger notification BATT_STATUS_CHANGED\n");
                        avrcp_target_battery_status_changed(avrcp_cid, AVRCP_BATTERY_STATUS_CRITICAL);
                        break;
                    case 'p':
                        printf("AVRCP: trigger notification NOW_PLAYING_CONTENT_CHANGED\n");
                        avrcp_target_playing_content_changed(avrcp_cid);
                        break;
                    case 's':
                        if (avrcp_connected){
                            printf("AVRCP: Set now playing info, with short info\n");
                            avrcp_target_set_now_playing_info(avrcp_cid, &tracks[0], sizeof(tracks)/sizeof(avrcp_track_t));
                        }
                        break;
                    case 'l':
                        if (avrcp_connected){
                            printf("AVRCP: Set now playing info, with long info\n");
                            avrcp_target_set_now_playing_info(avrcp_cid, &tracks[2], sizeof(tracks)/sizeof(avrcp_track_t));
                        }
                        break;
                    default:
                        break;
                }
                break;
        default:
            show_usage();
            break;

    }
    if (status != ERROR_CODE_SUCCESS){
        printf("Could not complete cmd %s, status 0x%02X\n", cmd, status);
    }
}
#endif

static bool avrcp_set_addressed_player_handler(uint16_t player_id){
    if (player_id < 1 || player_id > 2) return false;
    return true;
}

static void handle_l2cap_media_data_packet(uint8_t seid, uint8_t *packet, uint16_t size){
    UNUSED(seid);
    UNUSED(packet);
    UNUSED(size);
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_sink_init();
    avdtp_sink_register_packet_handler(&avdtp_sink_connection_establishment_packet_handler);

    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    if (!local_stream_endpoint) {
        printf("AVRCP/AVDTP Sink: not enough memory to create local_stream_endpoint\n");
        return 1;
    }

    local_stream_endpoint->sep.seid = 1;
    avdtp_sink_register_media_transport_category(local_stream_endpoint->sep.seid);
    avdtp_sink_register_media_codec_category(local_stream_endpoint->sep.seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));
    avdtp_sink_register_media_handler(&handle_l2cap_media_data_packet);

    avdtp_source_init();
    avdtp_source_register_packet_handler(&avdtp_source_connection_establishment_packet_handler);
    a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, (uint8_t *) media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), (uint8_t*) media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    

    // Initialize AVRCP service
    avrcp_init();
    avrcp_register_packet_handler(&avrcp_connection_establishment_packet_handler);
    // Initialize AVRCP Controller and Target
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_packet_handler);
    avrcp_target_init();
    avrcp_target_register_packet_handler(&avrcp_target_packet_handler);
    avrcp_target_register_set_addressed_player_handler(&avrcp_set_addressed_player_handler);

    // Initialize AVRCP Browsing service
    avrcp_browsing_init();
    avrcp_browsing_register_packet_handler(&avrcp_browsing_connection_establishment_packet_handler);
    // Initialize AVRCP Browsing Controller and Target
    avrcp_browsing_controller_init();
    avrcp_browsing_controller_register_packet_handler(&avrcp_browsing_controller_packet_handler);
    avrcp_browsing_target_init();
    avrcp_browsing_target_register_packet_handler(&avrcp_browsing_target_packet_handler);
    
    // Initialize SDP 
    sdp_init();
    // setup AVDTP
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    memset(sdp_avdtp_source_service_buffer, 0, sizeof(sdp_avdtp_source_service_buffer));
    a2dp_source_create_sdp_record(sdp_avdtp_source_service_buffer, 0x10002, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_source_service_buffer);

    // setup AVRCP
    uint16_t avrcp_controller_supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
    uint16_t avrcp_target_supported_features     = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    avrcp_controller_supported_features |= AVRCP_FEATURE_MASK_BROWSING;
    avrcp_target_supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10003, avrcp_controller_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);

    memset(sdp_avrcp_target_service_buffer, 0, sizeof(sdp_avrcp_target_service_buffer));
    avrcp_target_create_sdp_record(sdp_avrcp_target_service_buffer, 0x10004, avrcp_target_supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_target_service_buffer);

    gap_set_local_name("BTstack AVRCP Service PTS 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

    printf("TSPX_avctp_iut_command_data: ");
    int i; for (i=0;i<sizeof(fragmented_message);i++){ printf("%02x", fragmented_message[i]);}
    printf("\n");

    browsing_state = AVRCP_BROWSING_STATE_IDLE;

    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    sscanf_bd_addr(device_addr_string, device_addr_browsing);
    btstack_stdin_pts_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
