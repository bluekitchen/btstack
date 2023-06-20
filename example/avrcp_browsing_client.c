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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "avrcp_browsing_client.c"

/*
 * avrcp_browsing_client.c
 */

// *****************************************************************************
/* EXAMPLE_START(avrcp_browsing_client): AVRCP Browsing - Browse Media Players and Media Information
 *
 * @text This example demonstrates how to use the AVRCP Controller Browsing service to 
 * browse madia players and media information on a remote AVRCP Source device. 
 *
 * @text To test with a remote device, e.g. a mobile phone,
 * pair from the remote device with the demo, then use the UI for browsing. If HAVE_BTSTACK_STDIN is set, 
 * press SPACE on the console to show the available AVDTP and AVRCP commands.
 *
 */
// *****************************************************************************

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#ifndef ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE
#error "AVRCP Browsing requires L2CAP ERTM, please add ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE to your btstack_config.h"
#endif

#define AVRCP_BROWSING_ENABLED 

#define AVRCP_BROWSING_MAX_PLAYERS                  10
#define AVRCP_BROWSING_MAX_FOLDERS                  10
#define AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN  30
#define AVRCP_BROWSING_MAX_MEDIA_ITEMS              10

#ifdef HAVE_BTSTACK_STDIN
// mac 2011: static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: 
// static const char * device_addr_string = "84:38:35:65:d1:15";
// iPhone 5S: static const char * device_addr_string = "54:E4:3A:26:A2:39";
// phone 2013:  
// static const char * device_addr_string = "B0:34:95:CB:97:C4";
// iPod 
// static const char * device_addr_string = "B0:34:95:CB:97:C4";
// iPhone
static const char * device_addr_string = "6C:72:E7:10:22:EE";

static bd_addr_t device_addr;
#endif

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

static avrcp_browsing_state_t browsing_state = AVRCP_BROWSING_STATE_IDLE;
static uint16_t avrcp_cid = 0;
static bool     avrcp_connected = false;

static uint16_t browsing_cid = 0;
static bool     browsing_connected = false;

static uint8_t  sdp_avrcp_browsing_controller_service_buffer[200];
static uint8_t  sdp_avdtp_sink_service_buffer[150];

static uint16_t a2dp_cid = 0;
static uint8_t  a2dp_local_seid = 0;

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

static bool     browsing_query_active = false;
// static avrcp_media_item_context_t media_item_context;

static int playable_folder_index = 0;


static uint16_t browsing_uid_counter = 0;

static uint8_t  parent_folder_set = 0;
static uint8_t  parent_folder_uid[8];
static char     parent_folder_name[AVRCP_BROWSING_MAX_BROWSABLE_ITEM_NAME_LEN];

static avrcp_browsable_item_t folders[AVRCP_BROWSING_MAX_FOLDERS];
static int folder_index = -1;

static avrcp_browsable_media_element_item_t media_element_items[AVRCP_BROWSING_MAX_MEDIA_ITEMS];
static int media_element_item_index = -1;

static avrcp_browsable_media_player_item_t media_player_items[AVRCP_BROWSING_MAX_MEDIA_ITEMS];
static int media_player_item_index = -1;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static uint8_t ertm_buffer[10000];
static l2cap_ertm_config_t ertm_config = {
    1,  // ertm mandatory
    2,  // max transmit, some tests require > 1
    2000,
    12000,
    144,    // l2cap ertm mtu
    4,
    4,
    1,     // 16-bit FCS
};


static inline int next_index(int * index, int max_value){
    if ((*index) < max_value){
        (*index)++;
    } else {
        (*index) = 0;
    }
    return (*index);
}

static int next_folder_index(void){
    return next_index(&folder_index, AVRCP_BROWSING_MAX_FOLDERS);
}

static int next_media_element_item_index(void){
    return next_index(&media_element_item_index, AVRCP_BROWSING_MAX_MEDIA_ITEMS);
}
static int next_media_player_item_index(void){
    return next_index(&media_player_item_index, AVRCP_BROWSING_MAX_MEDIA_ITEMS);
}

/* @section Main Application Setup
 *
 * @text The Listing MainConfiguration shows how to setup AVRCP Controller Browsing service. 
 * To announce AVRCP Controller Browsing service, you need to create corresponding
 * SDP record and register it with the SDP service. 
 * You'll also need to register several packet handlers:
 * - stdin_process callback - used to trigger AVRCP commands, such are get media players, playlists, albums, etc. Requires HAVE_BTSTACK_STDIN.
 * - avrcp_browsing_controller_packet_handler - used to receive answers for AVRCP commands.
 *
 */

/* LISTING_START(MainConfiguration): Setup Audio Sink and AVRCP Controller services */
static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd);
#endif


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // Initialize L2CAP.
    l2cap_init();

#ifdef ENABLE_BLE
    // Initialize LE Security Manager. Needed for cross-transport key derivation
    sm_init();
#endif

    a2dp_sink_init();
    a2dp_sink_register_packet_handler(&a2dp_sink_packet_handler);

    avdtp_stream_endpoint_t * local_stream_endpoint = a2dp_sink_create_stream_endpoint(AVDTP_AUDIO, 
        AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities), 
        media_sbc_codec_configuration, sizeof(media_sbc_codec_configuration));
    if (!local_stream_endpoint){
        printf("A2DP Sink: not enough memory to create local stream endpoint\n");
        return 1;
    }
    a2dp_local_seid = avdtp_local_seid(local_stream_endpoint);

    // Initialize AVRCP service.
    avrcp_init();
    // Initialize AVRCP Controller & Target Service.
    avrcp_controller_init();
    avrcp_target_init();

    avrcp_register_packet_handler(&avrcp_packet_handler);
    avrcp_controller_register_packet_handler(&avrcp_packet_handler);
    avrcp_target_register_packet_handler(&avrcp_packet_handler);

    // Initialize AVRCP Browsing Service. 
    avrcp_browsing_init();
    avrcp_browsing_controller_init();
    avrcp_browsing_target_init();
    
    // Register for HCI events.
    avrcp_browsing_controller_register_packet_handler(&avrcp_browsing_controller_packet_handler);
    avrcp_browsing_target_register_packet_handler(&avrcp_browsing_controller_packet_handler);
    avrcp_browsing_register_packet_handler(&avrcp_browsing_controller_packet_handler);
    
    // Initialize SDP. 
    sdp_init();
    // setup AVDTP sink
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, sdp_create_service_record_handle(), AVDTP_SINK_FEATURE_MASK_HEADPHONE, NULL, NULL);
    btstack_assert(de_get_len( sdp_avdtp_sink_service_buffer) <= sizeof(sdp_avdtp_sink_service_buffer));
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    
    // Create AVRCP service record and register it with SDP.
    memset(sdp_avrcp_browsing_controller_service_buffer, 0, sizeof(sdp_avrcp_browsing_controller_service_buffer));

    uint16_t supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    avrcp_controller_create_sdp_record(sdp_avrcp_browsing_controller_service_buffer, sdp_create_service_record_handle(), supported_features, NULL, NULL);
    btstack_assert(de_get_len( sdp_avrcp_browsing_controller_service_buffer) <= sizeof(sdp_avrcp_browsing_controller_service_buffer));
    sdp_register_service(sdp_avrcp_browsing_controller_service_buffer);
    
    // Set local name with a template Bluetooth address, that will be automatically
    // replaced with a actual address once it is available, i.e. when BTstack boots
    // up and starts talking to a Bluetooth module.
    gap_set_local_name("AVRCP Browsing Client 00:00:00:00:00:00");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);
    
    // Register for HCI events.
    hci_event_callback_registration.callback = &avrcp_browsing_controller_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);
    

#ifdef HAVE_BTSTACK_STDIN
    // Parse human readable Bluetooth address.
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif
    printf("Starting BTstack ...\n");
    hci_power_control(HCI_POWER_ON);
    return 0;
}
/* LISTING_END */

static void avrcp_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    uint16_t local_cid;
    uint8_t  status = 0xFF;
    bd_addr_t adress;
    
    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    switch (packet[2]){
        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
            status = avrcp_subevent_connection_established_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                printf("AVRCP: Connection failed, status 0x%02x\n", status);
                avrcp_cid = 0;
                return;
            }
            
            avrcp_cid = local_cid;
            avrcp_connected = true;
            avrcp_subevent_connection_established_get_bd_addr(packet, adress);
            printf("AVRCP: Connected to %s, cid 0x%02x\n", bd_addr_to_str(adress), avrcp_cid);
            return;
        }
        
        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
            printf("AVRCP: Channel released: cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
            avrcp_cid = 0;
            avrcp_connected = false;
            return;
        default:
            break;
    }
}

static void a2dp_sink_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t address;
    uint8_t status;

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;

    switch (packet[2]){
        case A2DP_SUBEVENT_STREAM_ESTABLISHED:
            a2dp_subevent_stream_established_get_bd_addr(packet, address);
            status = a2dp_subevent_stream_established_get_status(packet);
            
            if (status != ERROR_CODE_SUCCESS){
                printf("A2DP  Sink      : Streaming connection failed, status 0x%02x\n", status);
                break;
            }
            
            a2dp_cid = a2dp_subevent_stream_established_get_a2dp_cid(packet);
            memcpy(device_addr, address, 6);
            printf("A2DP  Sink: Connection established, address %s, a2dp_cid 0x%02x\n", bd_addr_to_str(address), a2dp_cid);
            break;

        case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
            printf("A2DP  Sink: Connection released\n");
            break;
        
        default:
            break; 
    }
}


static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t address;
    uint8_t  status;
    uint16_t pos = 0;
    uint16_t local_cid;

    // printf("avrcp_browsing_controller_packet_handler packet type 0x%02X, subevent 0x%02X\n", packet_type, packet[2]);
    switch (packet_type) {   
        case HCI_EVENT_PACKET:
            switch (packet[0]){
                case HCI_EVENT_AVRCP_META:
                     switch (packet[2]){
                        case AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED: {
                            local_cid = avrcp_subevent_browsing_connection_established_get_browsing_cid(packet);
                            status = avrcp_subevent_browsing_connection_established_get_status(packet);
                            if (status != ERROR_CODE_SUCCESS){
                                printf("AVRCP: Connection failed, status 0x%02x\n", status);
                                browsing_cid = 0;
                                return;
                            }
                            avrcp_subevent_browsing_connection_established_get_bd_addr(packet, address);
                            browsing_cid = local_cid;
                            browsing_connected = true;
                            printf("AVRCP Browsing: Connection established, address %s, browsing_cid 0x%02x\n", bd_addr_to_str(address), browsing_cid);
                            return;
                        }
                        
                        case AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED:
                            printf("AVRCP: Connection released, cid 0x%02x\n", avrcp_subevent_browsing_connection_released_get_browsing_cid(packet));
                            browsing_cid = 0;
                            browsing_connected = false;
                            return;
                        case AVRCP_SUBEVENT_BROWSING_DONE:

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
                                        printf("Could not set player, status 0x%02x\n", status);
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
                    printf("player ID 0x%04x, major_type %d, subtype %d, status 0x%02x\n", player_id, major_type, subtype, status);
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
    printf("\n--- Bluetooth AVRCP Browsing Service Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - AVRCP Service create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("C      - AVRCP Service disconnect\n");
    printf("e      - AVRCP Browsing Service create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("E      - AVRCP Browsing Service disconnect\n");
    
    printf("I      - Set first found player as addressed player\n");
    printf("O      - Set first found player as browsed player\n");
    
    printf("p      - Get media players\n");
    printf("Q      - Browse folders\n");
    printf("P      - Go up one level\n");
    printf("W      - Go down one level\n");
    printf("T      - Browse media items\n");
    printf("---\n");
}
#endif


#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    
    if (cmd != 'a' && cmd != 'A' && cmd != 'c' && cmd != 'C'){
        if (browsing_query_active){
            printf("Query active, try later!\n");
            return;    
        }    
    }

    switch (cmd){
        case 'b':
            status = a2dp_sink_establish_stream(device_addr, &a2dp_cid);
            printf(" - Create AVDTP connection to addr %s, and local seid %d, expected cid 0x%02x.\n", bd_addr_to_str(device_addr), a2dp_local_seid, a2dp_cid);
            break;
        case 'B':
            printf(" - AVDTP disconnect from addr %s.\n", bd_addr_to_str(device_addr));
            a2dp_sink_disconnect(a2dp_cid);
            break;

        case 'c':
            printf(" - Connect to AVRCP Service on addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_connect(device_addr, &avrcp_cid);
            break;
        case 'C':
            if (avrcp_connected){
                printf(" - AVRCP Service disconnect from addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_disconnect(avrcp_cid);
                break;
            }
            printf("AVRCP Service already disconnected\n");
            break;

        case 'e':
            if (!avrcp_connected) {
                printf(" You must first connect to AVRCP Service on addr %s.\n", bd_addr_to_str(device_addr));
                break;
            }
            printf(" - Connect to AVRCP Browsing Service at addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_browsing_connect(device_addr, ertm_buffer, sizeof(ertm_buffer), &ertm_config, &browsing_cid);
            break;
        case 'E':
            if (browsing_connected){
                printf(" - AVRCP Browsing Service disconnect from addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_browsing_disconnect(browsing_cid);
                break;
            }
            printf("AVRCP Browsing Service already disconnected\n");
            break;
        case '\n':
        case '\r':
            break;
        
        default:
            if (!browsing_connected){
                show_usage();
                break;
            }
            
            switch (cmd) {
                case 'I':
                    if (media_player_item_index < 0) {
                        printf("AVRCP Browsing:Get media players first\n");
                        break;
                    }
                    printf("AVRCP Browsing:Set addressed player\n");
                    status = avrcp_controller_set_addressed_player(avrcp_cid, media_player_items[0].player_id);
                    break;
                case 'O':
                    if (media_player_item_index < 0) {
                        printf("AVRCP Browsing:Get media players first\n");
                        break;
                    }
                    printf("Set browsed player\n");
                    status = avrcp_browsing_controller_set_browsed_player(browsing_cid, media_player_items[0].player_id);
                    break;
                case 'p':
                    printf("AVRCP Browsing: get media players\n");
                    media_player_item_index = -1;
                    status = avrcp_browsing_controller_get_media_players(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'Q':
                    printf("AVRCP Browsing: browse folders\n");
                    folder_index = -1;
                    status = avrcp_browsing_controller_browse_file_system(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
                    break;
                case 'P':
                    printf("AVRCP Browsing: browse media items\n");
                    avrcp_browsing_controller_browse_media(browsing_cid, 0, 0xFFFFFFFF, AVRCP_MEDIA_ATTR_ALL);
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
                        memcpy(parent_folder_name, folders[0].name, folders[0].name_len);
                        memcpy(parent_folder_uid, folders[0].uid, 8);
                    }
                    printf("AVRCP Browsing: go down one level of %s\n", (char *)parent_folder_name);
                    status = avrcp_browsing_controller_go_down_one_level(browsing_cid, parent_folder_uid);
                    folder_index = -1;
                    break;
                default:
                    show_usage();
                    break;
            }
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("AVRCP Browsing: Could not perform command, status 0x%2x\n", status);
    }
}
#endif
/* EXAMPLE_END */
