/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

//#define AVRCP_BROWSING_ENABLED

static btstack_packet_callback_registration_t hci_event_callback_registration;

static bd_addr_t device_addr;
static uint8_t value[100];
// iPhone SE: static const char * device_addr_string = "BC:EC:5D:E6:15:03";
// iPhone 6:  static const char * device_addr_string = "D8:BB:2C:DF:F1:08";
// iPhone 5S:  
static const char * device_addr_string = "54:E4:3A:26:A2:39";
// Wiko Sunny: static const char * device_addr_string = "A0-4C-5B-0F-B2-42";
// pts: static const char * device_addr_string = "00:1B:DC:08:0A:A5";

static uint16_t avrcp_cid = 0;
static uint8_t sdp_avrcp_controller_service_buffer[200];

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status = 0xFF;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit test app
                    printf("AVRCP: HCI_EVENT_DISCONNECTION_COMPLETE\n");
                    break;
                case HCI_EVENT_AVRCP_META:
                    switch (packet[2]){
                        case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
                            local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
                            if (avrcp_cid != local_cid) {
                                printf("Connection is not established, expected 0x%02X l2cap cid, received 0x%02X\n", avrcp_cid, local_cid);
                                return;
                            }

                            status = avrcp_subevent_connection_established_get_status(packet);
                            avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
                            if (status != ERROR_CODE_SUCCESS){
                                printf("AVRCP Connection failed: status 0x%02x\n", status);
                                avrcp_cid = 0;
                                return;
                            }
                            printf("Channel successfully opened: %s, avrcp_cid 0x%02x\n", bd_addr_to_str(event_addr), avrcp_cid);
                            // automatically enable notifications
                            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
                            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_NOW_PLAYING_CONTENT_CHANGED);
                            return;
                        }
                        case AVRCP_SUBEVENT_CONNECTION_RELEASED:
                            printf("Channel released: avrcp_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
                            avrcp_cid = 0;
                            return;
                        default:
                            break;
                    }

                    status = packet[5];
                    local_cid = little_endian_read_16(packet, 3);
                    if (avrcp_cid != local_cid) return;

                    // avoid printing INTERIM status
                    if (status == AVRCP_CTYPE_RESPONSE_INTERIM) return;
                            
                    printf("AVRCP: command status: %s, ", avrcp_ctype2str(status));
                    switch (packet[2]){
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
                            printf("notification absolute volume changed %d\n", avrcp_subevent_notification_volume_changed_get_absolute_volume(packet));
                            return;
                        case AVRCP_SUBEVENT_NOTIFICATION_AVAILABLE_PLAYERS_CHANGED:
                            printf("notification changed\n");
                            return; 
                        case AVRCP_SUBEVENT_SHUFFLE_AND_REPEAT_MODE:{
                            uint8_t shuffle_mode = avrcp_subevent_shuffle_and_repeat_mode_get_shuffle_mode(packet);
                            uint8_t repeat_mode  = avrcp_subevent_shuffle_and_repeat_mode_get_repeat_mode(packet);
                            printf("%s, %s\n", avrcp_shuffle2str(shuffle_mode), avrcp_repeat2str(repeat_mode));
                            break;
                        }
                       
                        case AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO:
                            printf("AVRCP_SUBEVENT_NOW_PLAYING_TITLE_INFO len %d \n", avrcp_subevent_now_playing_title_info_get_value_len(packet));
                            if (avrcp_subevent_now_playing_title_info_get_value_len(packet) > 0){
                                memcpy(value, avrcp_subevent_now_playing_title_info_get_value(packet), avrcp_subevent_now_playing_title_info_get_value_len(packet));
                                printf("    Title: %s\n", value);
                            }  
                            break;

                        case AVRCP_SUBEVENT_NOW_PLAYING_ARTIST_INFO:
                            if (avrcp_subevent_now_playing_artist_info_get_value_len(packet) > 0){
                                memcpy(value, avrcp_subevent_now_playing_artist_info_get_value(packet), avrcp_subevent_now_playing_artist_info_get_value_len(packet));
                                printf("    Title: %s\n", value);
                            }  
                            break;
                        
                        case AVRCP_SUBEVENT_NOW_PLAYING_ALBUM_INFO:
                            if (avrcp_subevent_now_playing_album_info_get_value_len(packet) > 0){
                                memcpy(value, avrcp_subevent_now_playing_album_info_get_value(packet), avrcp_subevent_now_playing_album_info_get_value_len(packet));
                                printf("    Title: %s\n", value);
                            }  
                            break;
                        
                        case AVRCP_SUBEVENT_NOW_PLAYING_GENRE_INFO:
                            if (avrcp_subevent_now_playing_genre_info_get_value_len(packet) > 0){
                                memcpy(value, avrcp_subevent_now_playing_genre_info_get_value(packet), avrcp_subevent_now_playing_genre_info_get_value_len(packet));
                                printf("    Title: %s\n", value);
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
                        default:
                            printf("Not implemented\n");
                            break;
                    }  
                    break;   
                default:
                    break;
            }
            break;
        default:
            // other packet type
            break;
    }

}

#ifdef HAVE_BTSTACK_STDIN
static void show_usage(void){
    bd_addr_t      iut_address;
    gap_local_bd_addr(iut_address);
    printf("\n--- Bluetooth AVRCP Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("c      - create connection to addr %s\n", bd_addr_to_str(device_addr));
    printf("D      - disconnect\n");
    printf("\n--- Bluetooth AVRCP Commands ---\n");
    printf("i - get play status\n");
    printf("j - get now playing info\n");
    printf("k - play\n");
    printf("K - stop\n");
    printf("L - pause\n");
    printf("m - start fast forward\n");
    printf("M - stop  fast forward\n");
    printf("n - start rewind\n");
    printf("N - stop rewind\n");
    printf("o - forward\n");
    printf("O - backward\n");
    printf("p - volume up\n");
    printf("P - volume down\n");
    printf("r - absolute volume of 50 percent\n");
    printf("s - mute\n");
    printf("t - skip\n");
    printf("u - query repeat and shuffle mode\n");
    printf("v - repeat single track\n");
    printf("x - repeat all tracks\n");
    printf("X - disable repeat mode\n");
    printf("z - shuffle all tracks\n");
    printf("Z - disable shuffle mode\n");

    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    switch (cmd){
        case 'c':
            printf(" - Create AVRCP connection to addr %s.\n", bd_addr_to_str(device_addr));
            status = avrcp_connect(device_addr, &avrcp_cid);
            break;
        case 'B':
            printf(" - Disconnect\n");
            status = avrcp_disconnect(avrcp_cid);
            break;
        case 'i':
            printf(" - get play status\n");
            status = avrcp_controller_get_play_status(avrcp_cid);
            break;
        case 'j':
            printf(" - get now playing info\n");
            status = avrcp_controller_get_now_playing_info(avrcp_cid);
            break;
        case 'k':
            printf(" - play\n");
            status = avrcp_controller_play(avrcp_cid);
            break;
        case 'K':
            printf(" - stop\n");
            status = avrcp_controller_stop(avrcp_cid);
            break;
        case 'L':
            printf(" - pause\n");
            status = avrcp_controller_pause(avrcp_cid);
            break;
        case 'm':
            printf(" - start fast forward\n");
            status = avrcp_controller_press_and_hold_fast_forward(avrcp_cid);
            break;
        case 'M':
            printf(" - stop fast forward\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_cid);
            break;
        case 'n':
            printf(" - start rewind\n");
            status = avrcp_controller_press_and_hold_rewind(avrcp_cid);
            break;
        case 'N':
            printf(" - stop rewind\n");
            status = avrcp_controller_release_press_and_hold_cmd(avrcp_cid);
            break;
        case 'o':
            printf(" - forward\n");
            status = avrcp_controller_forward(avrcp_cid); 
            break;
        case 'O':
            printf(" - backward\n");
            status = avrcp_controller_backward(avrcp_cid);
            break;
        case 'p':
            printf(" - volume up\n");
            status = avrcp_controller_volume_up(avrcp_cid);
            break;
        case 'P':
            printf(" - volume down\n");
            status = avrcp_controller_volume_down(avrcp_cid);
            break;
        case 'r':
            printf(" - absolute volume of 50 percent\n");
            status = avrcp_controller_set_absolute_volume(avrcp_cid, 50);
            break;
        case 's':
            printf(" - mute\n");
            status = avrcp_controller_mute(avrcp_cid);
            break;
        case 't':
            printf(" - skip\n");
            status = avrcp_controller_skip(avrcp_cid);
            break;
        case 'u':
            printf(" - query repeat and shuffle mode\n");
            status = avrcp_controller_query_shuffle_and_repeat_modes(avrcp_cid);
            break;
        case 'v':
            printf(" - repeat single track\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_SINGLE_TRACK);
            break;
        case 'x':
            printf(" - repeat all tracks\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_ALL_TRACKS);
            break;
        case 'X':
            printf(" - disable repeat mode\n");
            status = avrcp_controller_set_repeat_mode(avrcp_cid, AVRCP_REPEAT_MODE_OFF);
            break;
        case 'z':
            printf(" - shuffle all tracks\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_ALL_TRACKS);
            break;
        case 'Z':
            printf(" - disable shuffle mode\n");
            status = avrcp_controller_set_shuffle_mode(avrcp_cid, AVRCP_SHUFFLE_MODE_OFF);
            break;
        default:
            show_usage();
            return;
    }
    if (status != ERROR_CODE_SUCCESS){
        printf("Could not perform command, status 0x%02x\n", status);
    }
}
#endif

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;
    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    
    // Initialize AVRCP service.
    avrcp_init();
    avrcp_register_packet_handler(&packet_handler);
    // Initialize AVRCP COntroller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&packet_handler);

    // Initialize SDP 
    sdp_init();
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));

    uint16_t supported_features = AVRCP_FEATURE_MASK_CATEGORY_PLAYER_OR_RECORDER;
#ifdef AVRCP_BROWSING_ENABLED
    supported_features |= AVRCP_FEATURE_MASK_BROWSING;
#endif
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10001, supported_features, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);
    
    gap_set_local_name("BTstack AVRCP Test");
    gap_discoverable_control(1);
    // gap_set_class_of_device(0x200408);
    
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);

    // turn on!
    hci_power_control(HCI_POWER_ON);

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif
    return 0;
}
