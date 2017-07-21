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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack.h"

#define AVRCP_BROWSING_ENABLED 0

#ifdef HAVE_BTSTACK_STDIN
#include "btstack_stdin.h"
#endif

#ifdef HAVE_BTSTACK_STDIN
// mac 2011:    static const char * device_addr_string = "04:0C:CE:E4:85:D3";
// pts:         
static const char * device_addr_string = "00:1B:DC:08:0A:A5";
// mac 2013:    static const char * device_addr_string = "84:38:35:65:d1:15";
// phone 2013:  static const char * device_addr_string = "D8:BB:2C:DF:F0:F2";
// minijambox:  static const char * device_addr_string = "00:21:3C:AC:F7:38";
// head phones: static const char * device_addr_string = "00:18:09:28:50:18";
// bt dongle:   static const char * device_addr_string = "00:15:83:5F:9D:46";
#endif
static bd_addr_t device_addr;


static btstack_packet_callback_registration_t hci_event_callback_registration;
static bd_addr_t device_addr;

static uint8_t sdp_avdtp_sink_service_buffer[150];
static uint8_t sdp_avrcp_controller_service_buffer[200];

static uint16_t avdtp_cid = 0;
static avdtp_sep_t sep;
static avdtp_stream_endpoint_t * local_stream_endpoint;
static avdtp_context_t a2dp_sink_context;

static uint16_t avrcp_cid = 0;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (packet_type != HCI_EVENT_PACKET) return;
    UNUSED(channel);
    UNUSED(size);
    bd_addr_t event_addr;
    uint16_t local_cid;
    uint8_t  status = 0xFF;
    
    switch (hci_event_packet_get_type(packet)) {   
        case HCI_EVENT_AVDTP_META:
            switch (packet[2]){
                case AVDTP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED:
                    avdtp_cid = avdtp_subevent_signaling_connection_established_get_avdtp_cid(packet);
                    status    = avdtp_subevent_signaling_connection_established_get_status(packet);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("AVDTP connection establishment failed: status 0x%02x.\n", status);
                        break;    
                    }
                    printf("AVDTP connection established: avdtp_cid 0x%02x.\n", avdtp_cid);
                    break;
                case AVDTP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
                    avdtp_cid = avdtp_subevent_signaling_connection_released_get_avdtp_cid(packet);
                    printf("AVDTP connection released: avdtp_cid 0x%02x.\n", avdtp_cid);
                    break;
                default:
                    printf("AVDTP event not parsed.\n");
                    break; 
            }
            break;   
          
        case HCI_EVENT_AVRCP_META:
            switch (packet[2]){
                case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED: {
                    status = avrcp_subevent_connection_established_get_status(packet);
                    avrcp_subevent_connection_established_get_bd_addr(packet, event_addr);
                    if (status != ERROR_CODE_SUCCESS){
                        printf("AVRCP connection establishment failed: status 0x%02x\n", status);
                        avrcp_cid = 0;
                        return;
                    }
                    local_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
                    if (avrcp_cid != 0 && avrcp_cid != local_cid) {
                        printf("AVRCP connection establishment failed: expected avrcp_cid 0x%02X, received 0x%02X\n", avrcp_cid, local_cid);
                        return;
                    }
                    avrcp_cid = local_cid;
                    printf("AVRCP connection established: %s, avrcp_cid 0x%02x\n", bd_addr_to_str(event_addr), avrcp_cid);
                    return;
                }
                case AVRCP_SUBEVENT_CONNECTION_RELEASED:
                    printf("AVRCP connection released: avrcp_cid 0x%02x\n", avrcp_subevent_connection_released_get_avrcp_cid(packet));
                    avrcp_cid = 0;
                    return;
                default:
                    break;
            }

            local_cid = little_endian_read_16(packet, 3);
            if (local_cid != avrcp_cid) return;
            // avoid printing INTERIM status
            status = packet[5];
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
                case AVRCP_SUBEVENT_NOW_PLAYING_INFO:{
                    uint8_t value[100];
                    printf("now playing: \n");
                    if (avrcp_subevent_now_playing_info_get_title_len(packet) > 0){
                        memcpy(value, avrcp_subevent_now_playing_info_get_title(packet), avrcp_subevent_now_playing_info_get_title_len(packet));
                        printf("    Title: %s\n", value);
                    }    
                    if (avrcp_subevent_now_playing_info_get_album_len(packet) > 0){
                        memcpy(value, avrcp_subevent_now_playing_info_get_album(packet), avrcp_subevent_now_playing_info_get_album_len(packet));
                        printf("    Album: %s\n", value);
                    }
                    if (avrcp_subevent_now_playing_info_get_artist_len(packet) > 0){
                        memcpy(value, avrcp_subevent_now_playing_info_get_artist(packet), avrcp_subevent_now_playing_info_get_artist_len(packet));
                        printf("    Artist: %s\n", value);
                    }
                    if (avrcp_subevent_now_playing_info_get_genre_len(packet) > 0){
                        memcpy(value, avrcp_subevent_now_playing_info_get_genre(packet), avrcp_subevent_now_playing_info_get_genre_len(packet));
                        printf("    Genre: %s\n", value);
                    }
                    printf("    Track: %d\n", avrcp_subevent_now_playing_info_get_track(packet));
                    printf("    Total nr. tracks: %d\n", avrcp_subevent_now_playing_info_get_total_tracks(packet));
                    printf("    Song length: %d ms\n", avrcp_subevent_now_playing_info_get_song_length(packet));
                    break;
                }
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
                    printf("AVRCP event not parsed.\n");
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
    printf("b      - AVDTP Sink create  connection to addr %s\n", device_addr_string);
    printf("B      - AVDTP Sink disconnect\n");
    printf("c      - AVRCP create connection to addr %s\n", device_addr_string);
    printf("C      - AVRCP disconnect\n");

    printf("\n--- Bluetooth AVRCP Commands %s ---\n", bd_addr_to_str(iut_address));
    printf("q - get capabilities: supported events\n");
    printf("w - get unit info\n");
    printf("r - get play status\n");
    printf("t - get now playing info\n");
    printf("1 - play\n");
    printf("2 - stop\n");
    printf("3 - pause\n");
    printf("4 - fast forward\n");
    printf("5 - rewind\n");
    printf("6 - forward\n");
    printf("7 - backward\n");
    printf("8 - volume up\n");
    printf("9 - volume down\n");
    printf("0 - mute\n");
    printf("R - absolute volume of 50 percent\n");
    printf("u - skip\n");
    printf("i - query repeat and shuffle mode\n");
    printf("o - repeat single track\n");
    printf("x/X - repeat/disable repeat all tracks\n");
    printf("z/Z - shuffle/disable shuffle all tracks\n");
    
    printf("a/A - register/deregister PLAYBACK_STATUS_CHANGED\n");
    printf("s/S - register/deregister TRACK_CHANGED\n");
    printf("d/D - register/deregister TRACK_REACHED_END\n");
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

static void stdin_process(char cmd){
    sep.seid = 1;
    switch (cmd){
        case 'b':
            printf("Establish AVDTP sink connection to %s\n", device_addr_string);
            avdtp_sink_connect(device_addr, &avdtp_cid);
            break;
        case 'B':
            printf("Disconnect AVDTP sink\n");
            avdtp_sink_disconnect(avdtp_cid);
            break;
        case 'c':
            printf("Establish AVRCP connection to %s.\n", bd_addr_to_str(device_addr));
            avrcp_controller_connect(device_addr, &avrcp_cid);
            break;
        case 'C':
            printf("Disconnect AVRCP\n");
            avrcp_controller_disconnect(avrcp_cid);
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
        case 'r':
            printf("AVRCP: get play status\n");
            avrcp_controller_get_play_status(avrcp_cid);
            break;
        case 't':
            printf("AVRCP: get now playing info\n");
            avrcp_controller_get_now_playing_info(avrcp_cid);
            break;
        case '1':
            printf("AVRCP: play\n");
            avrcp_controller_play(avrcp_cid);
            break;
        case '2':
            printf("AVRCP: stop\n");
            avrcp_controller_stop(avrcp_cid);
            break;
        case '3':
            printf("AVRCP: pause\n");
            avrcp_controller_pause(avrcp_cid);
            break;
        case '4':
            printf("AVRCP: fast forward\n");
            avrcp_controller_fast_forward(avrcp_cid);
            break;
        case '5':
            printf("AVRCP: rewind\n");
            avrcp_controller_rewind(avrcp_cid);
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
        case 'R':
            printf("AVRCP: absolute volume of 50 percent\n");
            avrcp_controller_set_absolute_volume(avrcp_cid, 50);
            break;
        case 'u':
            printf("AVRCP: skip\n");
            avrcp_controller_skip(avrcp_cid);
            break;
        case 'i':
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

        case 'a':
            printf("AVRCP: enable notification PLAYBACK_STATUS_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            break;
        case 's':
            printf("AVRCP: enable notification TRACK_CHANGED\n");
            avrcp_controller_enable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'd':
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

        case 'A':
            printf("AVRCP: disable notification PLAYBACK_STATUS_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_PLAYBACK_STATUS_CHANGED);
            break;
        case 'S':
            printf("AVRCP: disable notification TRACK_CHANGED\n");
            avrcp_controller_disable_notification(avrcp_cid, AVRCP_NOTIFICATION_EVENT_TRACK_CHANGED);
            break;
        case 'D':
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
        default:
            show_usage();
            break;

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
    // Initialize AVDTP Sink
    avdtp_sink_init(&a2dp_sink_context);
    avdtp_sink_register_packet_handler(&packet_handler);

    local_stream_endpoint = avdtp_sink_create_stream_endpoint(AVDTP_SINK, AVDTP_AUDIO);
    local_stream_endpoint->sep.seid = 1;
    avdtp_sink_register_media_transport_category(local_stream_endpoint->sep.seid);
    avdtp_sink_register_media_codec_category(local_stream_endpoint->sep.seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_capabilities, sizeof(media_sbc_codec_capabilities));

    // Initialize AVRCP COntroller
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&packet_handler);
    
    // Initialize SDP 
    sdp_init();
    // setup AVDTP sink
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 1, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);
    
    // setup AVRCP
    memset(sdp_avrcp_controller_service_buffer, 0, sizeof(sdp_avrcp_controller_service_buffer));
    avrcp_controller_create_sdp_record(sdp_avrcp_controller_service_buffer, 0x10001, AVRCP_BROWSING_ENABLED, 1, NULL, NULL);
    sdp_register_service(sdp_avrcp_controller_service_buffer);

    gap_set_local_name("BTstack AVRCP PTS Test");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200408);

#ifdef HAVE_BTSTACK_STDIN
    // parse human readable Bluetooth address
    sscanf_bd_addr(device_addr_string, device_addr);
    btstack_stdin_setup(stdin_process);
#endif
    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}
