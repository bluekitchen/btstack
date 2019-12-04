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

#define BTSTACK_FILE__ "avrcp_browsing_client.c"

/*
 * avrcp_browsing_client.c
 */

// *****************************************************************************
/* EXAMPLE_START(avrcp_browsing_client): Browse media players and media information on a remote device.
 *
 * @text This example demonstrates how to use the AVRCP Controller Browsing service to 
 * browse madia players and media information on a remote AVRCP Source device. 
 *
 * @test To test with a remote device, e.g. a mobile phone,
 * pair from the remote device with the demo, then use the UI for browsing (tap 
 * SPACE on the console to show the available AVRCP commands).
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

#define AVRCP_BROWSING_ENABLED 

#define AVRCP_BROWSING_MAX_PLAYERS          10
#define AVRCP_BROWSING_MAX_FOLDERS          10
#define AVRCP_BROWSING_MAX_FOLDER_NAME_LEN  30

#ifdef HAVE_BTSTACK_STDIN
// mac 2011: static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
// pts: static bd_addr_t remote = {0x00, 0x1B, 0xDC, 0x08, 0x0A, 0xA5};
// mac 2013: 
// static const char * device_addr_string = "84:38:35:65:d1:15";
// iPhone 5S: static const char * device_addr_string = "54:E4:3A:26:A2:39";
// phone 2013:  
// static const char * device_addr_string = "B0:34:95:CB:97:C4";

static const char * device_addr_string = "B0:34:95:CB:97:C4";

static bd_addr_t device_addr;
#endif

static uint16_t avrcp_cid = 0;
static uint8_t  avrcp_connected = 0;

static uint16_t browsing_cid = 0;
static uint8_t  avrcp_browsing_connected = 0;
static uint8_t  sdp_avrcp_browsing_controller_service_buffer[200];

static uint8_t browsing_query_active = 0;
static avrcp_media_item_context_t media_item_context;

typedef struct {
    uint16_t  charset;
    uint8_t   depth;
    uint16_t  name_len;
    char      name[AVRCP_BROWSING_MAX_FOLDER_NAME_LEN];
} avrcp_browsing_root_folder_t;

typedef struct {
    uint8_t  uid[8];
    uint16_t name_len;
    char     name[AVRCP_BROWSING_MAX_FOLDER_NAME_LEN];
} avrcp_browsing_folders_t;

static uint8_t  parent_folder_set = 0;
static uint8_t  parent_folder_uid[8];
static char     parent_folder_name[AVRCP_BROWSING_MAX_FOLDER_NAME_LEN];
static avrcp_browsing_folders_t folders[AVRCP_BROWSING_MAX_FOLDERS];
static int folder_index = -1;
static uint16_t players[AVRCP_BROWSING_MAX_PLAYERS];
static int player_index = -1;
static uint16_t browsing_uid_counter = 0;

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
    0,      // No FCS
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

static int next_player_index(void){
    return next_index(&player_index, AVRCP_BROWSING_MAX_PLAYERS);
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
#ifdef HAVE_BTSTACK_STDIN
static void stdin_process(char cmd);
#endif


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;

    // Initialize L2CAP.
    l2cap_init();
    
    // Initialize AVRCP Controller.
    avrcp_controller_init();

        // Initialize AVRCP Browsing Controller, HCI events will be sent to the AVRCP Controller callback. 
    avrcp_browsing_controller_init();
    // // Register AVRCP for HCI events.
    avrcp_browsing_controller_register_packet_handler(&avrcp_browsing_controller_packet_handler);
    
    // Initialize SDP. 
    sdp_init();

    // Create AVRCP service record and register it with SDP.
    memset(sdp_avrcp_browsing_controller_service_buffer, 0, sizeof(sdp_avrcp_browsing_controller_service_buffer));

    uint16_t supported_features = (1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_CATEGORY_PLAYER_OR_RECORDER);
#ifdef AVRCP_BROWSING_ENABLED
    supported_features |= (1 << AVRCP_CONTROLLER_SUPPORTED_FEATURE_BROWSING);
#endif
    avrcp_controller_create_sdp_record(sdp_avrcp_browsing_controller_service_buffer, 0x10001, supported_features, NULL, NULL);
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

    // Register for AVRCP events.
    avrcp_controller_register_packet_handler(&avrcp_browsing_controller_packet_handler);


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

static void avrcp_browsing_controller_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    int pos;

    switch(packet_type){
        case AVRCP_BROWSING_DATA_PACKET:
            pos = 0;
            browsing_query_active = 1;
            avrcp_browsing_item_type_t data_type = (avrcp_browsing_item_type_t)packet[pos++];
            pos += 2; // length

            switch (data_type){
                case AVRCP_BROWSING_MEDIA_ROOT_FOLDER:{
                    avrcp_browsing_root_folder_t root_folder;
                    root_folder.charset = big_endian_read_16(packet, pos);
                    pos += 2;
                    root_folder.depth = packet[pos++];
                    root_folder.name_len = big_endian_read_16(packet, pos);
                    pos += 2;

                    memset(root_folder.name, 0, AVRCP_BROWSING_MAX_FOLDER_NAME_LEN);
                    root_folder.name_len = btstack_min(big_endian_read_16(packet, pos), AVRCP_BROWSING_MAX_FOLDER_NAME_LEN - 1);
                    memcpy(root_folder.name, packet+pos, root_folder.name_len);
                    printf("Found root folder: name %s, depth %d \n", (char *)root_folder.name, root_folder.depth);
                    break;
                }
                case AVRCP_BROWSING_MEDIA_PLAYER_ITEM:{
                    printf("Received media player:  ");
                    uint16_t player_id = big_endian_read_16(packet, pos);
                    pos += 2;
                    avrcp_browsing_media_player_major_type_t major_type = packet[pos++];
                    avrcp_browsing_media_player_subtype_t subtype = big_endian_read_32(packet, pos);
                    pos += 4;
                    uint8_t status = packet[pos++];
                    uint8_t feature_bitmask[16];
                    memcpy(feature_bitmask, packet, 16);
                    pos += 16;
                    printf("player ID 0x%04x, major_type %d, subtype %d, status %d\n", player_id, major_type, subtype, status);
                    players[next_player_index()] = player_id;
                    break;
                }
                case AVRCP_BROWSING_FOLDER_ITEM:{
                    int index = next_folder_index();
                    // printf("Found folder [%d]: ", index);
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
                    
                    char value[AVRCP_BROWSING_MAX_FOLDER_NAME_LEN];
                    
                    memset(value, 0, AVRCP_BROWSING_MAX_FOLDER_NAME_LEN);
                    uint16_t value_len = btstack_min(displayable_name_length, AVRCP_BROWSING_MAX_FOLDER_NAME_LEN - 1); 
                    memcpy(value, packet+pos, value_len);

                    printf("UID 0x%08" PRIx32 "%08" PRIx32 ", type 0x%02x, is_playable %d, charset 0x%02x, name %s\n", 
                        folder_uid_high, folder_uid_low, folder_type, is_playable, charset, value);
                    memcpy(folders[index].name, value, value_len);
                    folders[index].name_len = value_len;
                    break;
                }
                case AVRCP_BROWSING_MEDIA_ELEMENT_ITEM:{
                    printf("Found media: ");
            
                    uint32_t media_uid_high = big_endian_read_32(packet, pos);
                    pos += 4;
                    uint32_t media_uid_low  = big_endian_read_32(packet, pos+4);
                    pos += 4;
                    avrcp_browsing_media_type_t media_type = packet[pos++];
                    uint16_t charset = big_endian_read_16(packet, pos);
                    pos += 2;
                    uint16_t displayable_name_length = big_endian_read_16(packet, pos);
                    pos += 2;
                    pos += displayable_name_length;
                    printf("Media UID: 0x%08" PRIx32 "%08" PRIx32 ", media_type 0x%02x, charset 0x%02x, displayable_name_length %d\n", media_uid_high, media_uid_low, media_type, charset, displayable_name_length);
                    
                    uint8_t num_attributes = packet[pos++];
                    printf("Num media attributes %d\n", num_attributes);

                    for (avrcp_media_item_iterator_init(&media_item_context, size-pos, packet+pos); avrcp_media_item_iterator_has_more(&media_item_context); avrcp_media_item_iterator_next(&media_item_context)){
                        uint32_t attr_id            = avrcp_media_item_iterator_get_attr_id(&media_item_context);
                        uint16_t attr_charset       = avrcp_media_item_iterator_get_attr_charset(&media_item_context);
                        uint16_t attr_value_length  = avrcp_media_item_iterator_get_attr_value_len(&media_item_context);
                        const uint8_t * attr_value  = avrcp_media_item_iterator_get_attr_value(&media_item_context);
                    
                        printf("Attr ID 0x%08" PRIx32 ", charset %d, attr_value_length %d, value %s", attr_id, attr_charset, attr_value_length, attr_value);
                    }
                    break;
                }

                default:
                    log_error("AVRCP browsing: unknown browsable item type 0%02x", data_type);
                    break;
            }
            break;
        case HCI_EVENT_PACKET:
            if (hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
            uint16_t local_cid;
            uint8_t  status = 0xFF;
            bd_addr_t address;

            if (packet[0] != HCI_EVENT_AVRCP_META) break;
            switch (packet[2]){
                case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
                    local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
                    printf("AVRCP_SUBEVENT_CONNECTION_ESTABLISHED cid 0x%02x\n", local_cid);
                    
                    if (avrcp_cid != 0 && avrcp_cid != local_cid) {
                        printf("AVRCP Controller connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", avrcp_cid, local_cid);
                        return;
                    }

                    status = avrcp_subevent_connection_established_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("AVRCP Controller connection failed: status 0x%02x\n", status);
                        avrcp_cid = 0;
                        return;
                    }
                    
                    avrcp_cid = local_cid;
                    avrcp_connected = 1;
                    avrcp_subevent_connection_established_get_bd_addr(packet, address);
                    printf("AVRCP Controller connected.\n");
                    return;
                }
                case AVRCP_SUBEVENT_CONNECTION_RELEASED:
                    printf("AVRCP Controller Client released.\n");
                    avrcp_cid = 0;
                    avrcp_browsing_connected = 0;
                    folder_index = 0;
                    memset(folders, 0, sizeof(folders));
                    return;
                
                case AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION:
                    local_cid = avrcp_subevent_incoming_browsing_connection_get_browsing_cid(packet);
                    printf("AVRCP_SUBEVENT_INCOMING_BROWSING_CONNECTION cid 0x%02x\n", local_cid);
                    if (browsing_cid != 0 && browsing_cid != local_cid) {
                        printf("AVRCP Browsing Client connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", browsing_cid, local_cid);
                        avrcp_browsing_controller_decline_incoming_connection(browsing_cid);
                        return;
                    }
                    browsing_cid = local_cid;
                    printf("AVRCP Browsing Client configure incoming connection, browsing cid 0x%02x\n", browsing_cid);
                    avrcp_browsing_controller_configure_incoming_connection(browsing_cid, ertm_buffer, sizeof(ertm_buffer), &ertm_config);
                    break;

                case AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED: {
                    local_cid = avrcp_subevent_browsing_connection_established_get_browsing_cid(packet);
                    printf("AVRCP_SUBEVENT_BROWSING_CONNECTION_ESTABLISHED cid 0x%02x\n", local_cid);
                    if (browsing_cid != 0 && browsing_cid != local_cid) {
                        printf("AVRCP Browsing Client connection failed, expected 0x%02X l2cap cid, received 0x%02X\n", browsing_cid, local_cid);
                        return;
                    }

                    status = avrcp_subevent_browsing_connection_established_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("AVRCP Browsing Client connection failed: status 0x%02x\n", status);
                        browsing_cid = 0;
                        return;
                    }
                    
                    browsing_cid = local_cid;
                    avrcp_browsing_connected = 1;
                    avrcp_subevent_browsing_connection_established_get_bd_addr(packet, address);
                    printf("AVRCP Browsing Client connected\n");
                    return;
                }
                case AVRCP_SUBEVENT_BROWSING_CONNECTION_RELEASED:
                    printf("AVRCP Browsing Controller released\n");
                    browsing_cid = 0;
                    avrcp_browsing_connected = 0;
                    return;
                
                case AVRCP_SUBEVENT_BROWSING_DONE:
                    browsing_query_active = 0;
                    browsing_uid_counter = 0;
                    if (avrcp_subevent_browsing_done_get_browsing_status(packet) != AVRCP_BROWSING_ERROR_CODE_SUCCESS){
                        printf("AVRCP Browsing query done with browsing status 0x%02x, bluetooth status 0x%02x.\n", 
                            avrcp_subevent_browsing_done_get_browsing_status(packet),
                            avrcp_subevent_browsing_done_get_bluetooth_status(packet));
                        break;    
                    }
                    browsing_uid_counter = avrcp_subevent_browsing_done_get_uid_counter(packet);
                    printf("DONE, browsing_uid_counter %d.\n", browsing_uid_counter);
                    break;

                default:
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
    printf("\n--- Bluetooth AVRCP Controller Connection Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - AVRCP Controller create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("e      - AVRCP Browsing Controller create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("E      - AVRCP Browsing Controller disconnect\n");
    printf("C      - AVRCP Controller disconnect\n");

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
        case 'c':
            printf(" - Create AVRCP connection for control to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_controller_connect(device_addr, &avrcp_cid);
            break;
        case 'C':
            if (avrcp_connected){
                printf(" - AVRCP Controller disconnect from addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_controller_disconnect(avrcp_cid);
                break;
            }
            printf("AVRCP Controller already disconnected\n");
            break;

        case 'e':
            if (!avrcp_connected) {
                printf(" You must first create AVRCP connection for control to addr %s.\n", bd_addr_to_str(device_addr));
                break;
            }
            printf(" - Create AVRCP connection for browsing to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_browsing_controller_connect(device_addr, ertm_buffer, sizeof(ertm_buffer), &ertm_config, &browsing_cid);
            break;
        case 'E':
            if (avrcp_browsing_connected){
                printf(" - AVRCP Browsing Controller disconnect from addr %s.\n", bd_addr_to_str(device_addr));
                status = avrcp_browsing_controller_disconnect(browsing_cid);
                break;
            }
            printf("AVRCP Browsing Controller already disconnected\n");
            break;
        case '\n':
        case '\r':
            break;
        
        default:
            if (!avrcp_browsing_connected){
                show_usage();
                printf("Please connect the AVRCP Browsing client\n");
                break;
            }
            
            switch (cmd) {
                case 'I':
                    if (player_index < 0) {
                        printf("Get media players first\n");
                        break;
                    }
                    printf("Set addressed player\n");
                    status = avrcp_controller_set_addressed_player(avrcp_cid, players[0]);
                    break;
                case 'O':
                    if (player_index < 0) {
                        printf("Get media players first\n");
                        break;
                    }
                    printf("Set browsed player\n");
                    status = avrcp_browsing_controller_set_browsed_player(browsing_cid, players[0]);
                    break;
                case 'p':
                    printf("AVRCP Browsing: get media players\n");
                    player_index = -1;
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
        printf("Could not perform command, status 0x%2x\n", status);
    }
}
#endif
/* EXAMPLE_END */
