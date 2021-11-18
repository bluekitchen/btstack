/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "le_data_channel.c"

/*
 * le_data_channel.c 
 */ 


#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "btstack_config.h"

#include "ble/le_device_db.h"
#include "ble/sm.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_stdin.h"
 
static void show_usage(void);

#define TEST_PACKET_SIZE       100

#define TSPX_PSM             0x01
#define TSPX_LE_PSM          0x25
#define TSPX_ENHANCED_PSM    0x27
#define TSPX_PSM_UNSUPPORTED 0xf1
#define TSPX_L2CA_CBMPS_MIN    64

#define MTU_MIN                64

#define ENHANCED_MTU_INITIAL (TEST_PACKET_SIZE-1)
#define ENHANCED_MTU_RECONFIGURE (TEST_PACKET_SIZE)

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// static bd_addr_t pts_address = { 0x00, 0x1a, 0x7d, 0xda, 0x71, 0x0a};
static bd_addr_t pts_address = { 0x00, 0x1b, 0xdc, 0x08, 0x0a, 0xa5};

static int pts_address_type = 0;
static int master_addr_type = 0;
static hci_con_handle_t handle_le = HCI_CON_HANDLE_INVALID;
static hci_con_handle_t handle_classic = HCI_CON_HANDLE_INVALID;
static uint32_t ui_passkey;
static int  ui_digits_for_passkey;

// general discoverable flags
static uint8_t adv_general_discoverable[] = { 2, 01, 02 };


static uint16_t initial_credits = L2CAP_LE_AUTOMATIC_CREDITS;

const char * data_short = "a";
const char * data_long  = "0123456789abcdefghijklmnopqrstuvwxyz";
const char * data_classic = "ABCDEF";
static int todo_send_short;
static int todo_send_long;

// note: must be smaller than TSPX_tester_mtu for l2cap/le/cfc/bv-02-c to succeed
static uint8_t buffer_x[500];

static uint16_t cid_le;
static uint16_t cid_classic;
static uint16_t cid_enhanced;

uint8_t receive_buffer_X[100];

static uint8_t data_channel_buffer_1[TEST_PACKET_SIZE];
static uint8_t data_channel_buffer_2[TEST_PACKET_SIZE];

static uint8_t * receive_buffers_2[] = { data_channel_buffer_1, data_channel_buffer_2 };

static bool enhanced_data_channel;

static uint16_t enhanced_reconfigure_index = 0;
static uint16_t enhanced_reconfigure_mps[] = { TEST_PACKET_SIZE, TEST_PACKET_SIZE - 2 };
static uint16_t enhanced_reconfigure_choices = sizeof (enhanced_reconfigure_mps) / sizeof(uint16_t);
static bool     enhanced_authorization_required = false;
static uint16_t enhanced_remote_mtu;
static bool     enhanced_insufficient_key_size;

static void gap_run(void){
}

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_address;
    uint16_t psm;
    uint16_t cid;
    hci_con_handle_t handle;
    uint16_t cids[2];
    uint8_t status;
    uint16_t mps, mtu;

    switch (packet_type) {
        
        case L2CAP_DATA_PACKET:
            printf("L2CAP data cid 0x%02x len %u: ", channel, size);
            printf_hexdump(packet, size);
            break;

        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;
                
                case HCI_EVENT_LE_META:
                    switch (hci_event_le_meta_get_subevent_code(packet)) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            handle_le = little_endian_read_16(packet, 4);
                            printf("HCI: LE Connection Complete, connection handle 0x%04x\n", handle_le);
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_CONNECTION_COMPLETE:
                    handle_classic = little_endian_read_16(packet, 3);
                    printf("HCI: ACL Connection Complete, connection handle 0x%04x\n", handle_classic);
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    break;

                // Classic
                case L2CAP_EVENT_INCOMING_CONNECTION: 
                    psm = l2cap_event_incoming_connection_get_psm(packet);
                    cid = l2cap_event_incoming_connection_get_local_cid(packet);
                    if (psm != TSPX_PSM) break;
                    printf("L2CAP: Accepting incoming Classic connection request for 0x%02x, PSM %02x\n", cid, psm); 
                    l2cap_accept_connection(cid);
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_channel_opened_get_address(packet, event_address);
                    psm = l2cap_event_channel_opened_get_psm(packet); 
                    cid = l2cap_event_channel_opened_get_local_cid(packet); 
                    handle = l2cap_event_channel_opened_get_handle(packet);
                    if (packet[2] == 0) {
                        enhanced_data_channel = false;
                        printf("L2CAP: Classic Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_address), handle, psm, cid,  l2cap_event_channel_opened_get_remote_cid(packet));
                        switch (psm){
                            case TSPX_LE_PSM:
                                cid_le = cid;
                                break;
                            case TSPX_PSM:
                                cid_classic = cid;
                                break;
                            default:
                                break;
                        }
                    } else {
                        printf("L2CAP: classic connection to device %s failed. status code %u\n", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    cid = l2cap_event_channel_closed_get_local_cid(packet);
                    printf("L2CAP: Clasic Channel closed 0x%02x\n", cid); 
                    break;

                // LE Data Channels
                case L2CAP_EVENT_LE_INCOMING_CONNECTION: 
                    psm = l2cap_event_le_incoming_connection_get_psm(packet);
                    cid = l2cap_event_le_incoming_connection_get_local_cid(packet);
                    if (psm != TSPX_LE_PSM) break;
                    printf("L2CAP: Accepting incoming LE connection request for 0x%02x, PSM %02x\n", cid, psm); 
                    l2cap_le_accept_connection(cid, receive_buffer_X, sizeof(receive_buffer_X), initial_credits);
                    break;


                case L2CAP_EVENT_LE_CHANNEL_OPENED:
                    // inform about new l2cap connection
                    l2cap_event_le_channel_opened_get_address(packet, event_address);
                    psm = l2cap_event_le_channel_opened_get_psm(packet); 
                    cid = l2cap_event_le_channel_opened_get_local_cid(packet); 
                    handle = l2cap_event_le_channel_opened_get_handle(packet);
                    if (packet[2] == 0) {
                        enhanced_data_channel = false;
                        printf("L2CAP: LE Data Channel successfully opened: %s, handle 0x%02x, psm 0x%02x, local cid 0x%02x, remote cid 0x%02x\n",
                               bd_addr_to_str(event_address), handle, psm, cid,  little_endian_read_16(packet, 15));
                        switch (psm){
                            case TSPX_LE_PSM:
                                cid_le = cid;
                                break;
                            case TSPX_PSM:
                                cid_classic = cid;
                                break;
                            default:
                                break;
                        }
                    } else {
                        printf("L2CAP: LE Data Channel connection to device %s failed. status code %u\n", bd_addr_to_str(event_address), packet[2]);
                    }
                    break;

                case L2CAP_EVENT_LE_CAN_SEND_NOW:
                    if (todo_send_short){
                        todo_send_short = 0;
                        if (enhanced_data_channel){
                            l2cap_enhanced_send_data(cid_enhanced, (uint8_t *) data_short, strlen(data_short));
                        } else {
                            l2cap_le_send_data(cid_le, (uint8_t *) data_short, strlen(data_short));
                        }
                        break;
                    }
                    if (todo_send_long){
                        todo_send_long = 0;                        
                        if (enhanced_data_channel){
                            // l2cap_enhanced_send_data(cid_enhanced, (uint8_t *) data_long, strlen(data_long));
                            memset(receive_buffer_X, 0x33, enhanced_remote_mtu);
                            l2cap_enhanced_send_data(cid_enhanced, (uint8_t *) receive_buffer_X, enhanced_remote_mtu);
                        } else {
                            l2cap_le_send_data(cid_le, (uint8_t *) data_long, strlen(data_long));
                        }
                    }
                    break;

                case L2CAP_EVENT_LE_CHANNEL_CLOSED:
                    cid = l2cap_event_le_channel_closed_get_local_cid(packet);
                    printf("L2CAP: LE Data Channel closed 0x%02x\n", cid); 
                    break;

               case L2CAP_EVENT_LE_PACKET_SENT:
                    cid = l2cap_event_le_packet_sent_get_local_cid(packet);
                    printf("L2CAP: LE Data Channel Packet sent0x%02x\n", cid); 
                    break;

                case L2CAP_EVENT_DATA_CHANNEL_OPENED:
                    enhanced_data_channel = true;
                    cid = l2cap_event_data_channel_opened_get_local_cid(packet);
                    status = l2cap_event_channel_opened_get_status(packet);
                    if (l2cap_event_channel_opened_get_status(packet) == 0){
                        cid_enhanced = cid;
                        enhanced_remote_mtu = l2cap_event_data_channel_opened_get_remote_mtu(packet);
                    }
                    printf("L2CAP_EVENT_DATA_CHANNEL_OPENED - cid 0x%04x mtu %u, status 0x%02x\n", cid, enhanced_remote_mtu, status);
                    break;

                case L2CAP_EVENT_DATA_CHANNEL_INCOMING:
                    printf("L2CAP_EVENT_DATA_CHANNEL_INCOMING - remote mtu %u\n", mtu);
                    cid = l2cap_event_data_channel_incoming_get_local_cid(packet);
                    if (enhanced_authorization_required){
                        enhanced_authorization_required = false;
                        l2cap_enhanced_decline_data_channels(cid, 0x0006);
                        break;
                    }
                    if (enhanced_insufficient_key_size){
                        enhanced_insufficient_key_size = false;
                        l2cap_enhanced_decline_data_channels(cid, 0x0007);
                        break;
                    }
                    l2cap_enhanced_accept_data_channels(cid, 2, initial_credits, ENHANCED_MTU_INITIAL, receive_buffers_2, cids);
                    break;

                case L2CAP_EVENT_DATA_CHANNEL_RECONFIGURED:
                    enhanced_remote_mtu = l2cap_event_data_channel_reconfigured_get_mtu(packet);
                    printf("L2CAP_EVENT_DATA_CHANNEL_RECONFIGURED - remote mps %u, remote mtu %u\n",
                           l2cap_event_data_channel_reconfigured_get_mps(packet), enhanced_remote_mtu);
                    break;

                case SM_EVENT_JUST_WORKS_REQUEST:
                    printf("SM_EVENT_JUST_WORKS_REQUEST\n");
                    sm_just_works_confirm(little_endian_read_16(packet, 2));
                    break;

                case SM_EVENT_PASSKEY_INPUT_NUMBER:
                    // display number
                    master_addr_type = packet[4];
                    reverse_bd_addr(&packet[5], event_address);
                    printf("\nGAP Bonding %s (%u): Enter 6 digit passkey: '", bd_addr_to_str(pts_address), master_addr_type);
                    fflush(stdout);
                    ui_passkey = 0;
                    ui_digits_for_passkey = 6;
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
                    // display number
                    printf("\nGAP Bonding %s (%u): Display Passkey '%06u\n", bd_addr_to_str(pts_address), master_addr_type, little_endian_read_32(packet, 11));
                    break;

                case SM_EVENT_PASSKEY_DISPLAY_CANCEL: 
                    printf("\nGAP Bonding %s (%u): Display cancel\n", bd_addr_to_str(pts_address), master_addr_type);
                    break;

                case SM_EVENT_AUTHORIZATION_REQUEST:
                    // auto-authorize connection if requested
                    sm_authorization_grant(little_endian_read_16(packet, 2));
                    break;

                default:
                    break;
            }
    }
    gap_run();
}

void show_usage(void){
    bd_addr_t iut_address;
    uint8_t uit_addr_type;

    gap_le_get_own_address(&uit_addr_type, iut_address);
    
    printf("\n--- CLI for L2CAP and LE Data Channel %s ---\n", bd_addr_to_str(iut_address));
    printf("a - create HCI LE connection to type %u address %s\n", pts_address_type, bd_addr_to_str(pts_address));
    printf("A - create HCI BR/EDR connection to     address %s\n",                   bd_addr_to_str(pts_address));
    printf("b - connect to PSM 0x%02x  CFC (TSPX_LE_PSM - LE)\n", TSPX_LE_PSM);
    printf("B - connect to PSM 0x%02x  CFC (TSPX_PSM_UNSUPPORTED - LE)\n", TSPX_PSM_UNSUPPORTED);
    printf("c - send 10 credits\n");
    printf("d - connect to PSM 0x%02x ECFC (TSPX_LE_PSM)\n", TSPX_LE_PSM);
    printf("D - connect to PSM 0x%02x ECFC (TSPX_PSM_UNSUPPORTED - LE)\n", TSPX_PSM_UNSUPPORTED);
    printf("e - require encrypted connection for ECFC / insufficint key size - LEVEL_2\n");
    printf("E - require authenticated connection for ECFC - LEVEL_3\n");
    printf("f - require authorized connection for ECFC\n");
    printf("m - enable manual credit management (incoming connections only)\n");
    printf("r - send EFCF Reconfigure Request for MTU %u\n", ENHANCED_MTU_RECONFIGURE);
    printf("s - send short data %s\n", data_short);
    printf("S - send long data %s\n", data_long);
    printf("y - connect to address %s PSM 0x%02x (TSPX_PSM - Classic)\n", bd_addr_to_str(pts_address), TSPX_PSM);
    printf("z - send classic data Classic %s\n", data_classic);
    printf("t - disconnect channel\n");
    printf("---\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(char buffer){
    // passkey input
    if (ui_digits_for_passkey){
        if (buffer < '0' || buffer > '9') return;
        printf("%c", buffer);
        fflush(stdout);
        ui_passkey = ui_passkey * 10 + buffer - '0';
        ui_digits_for_passkey--;
        if (ui_digits_for_passkey == 0){
            printf("\nSending Passkey '%06x'\n", ui_passkey);
            sm_passkey_input(handle_le, ui_passkey);
        }
        return;
    }

    uint16_t cids[2];

    switch (buffer){
        case 'a':
            printf("Direct LE Connection Establishment to type %u, addr %s\n", pts_address_type, bd_addr_to_str(pts_address));
            gap_advertisements_enable(0);   // controller with single role cannot create connection while advertising
            gap_connect(pts_address, pts_address_type);
            break;

        case 'A':
            printf("BR/EDR Connection Establishment to addr %s\n", bd_addr_to_str(pts_address));
            gap_connect(pts_address, BD_ADDR_TYPE_ACL);
            break;

        case 'b':
            printf("Connect to PSM 0x%02x - CFC LE\n", TSPX_LE_PSM);
            l2cap_le_create_channel(&app_packet_handler, handle_le, TSPX_LE_PSM, buffer_x, 
                                    sizeof(buffer_x), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &cid_le);
            break;

        case 'B':
            printf("Creating connection to %s 0x%02x - CFC LE\n", bd_addr_to_str(pts_address), TSPX_PSM_UNSUPPORTED);
            l2cap_le_create_channel(&app_packet_handler, handle_le, TSPX_PSM_UNSUPPORTED, buffer_x, 
                                    sizeof(buffer_x), L2CAP_LE_AUTOMATIC_CREDITS, LEVEL_0, &cid_le);
            break;

        case 'd':
            if (handle_le != HCI_CON_HANDLE_INVALID){
                printf("Connect to PSM 0x%02x - ECFC LE\n", TSPX_LE_PSM);
                l2cap_enhanced_create_channels(&app_packet_handler, handle_le, LEVEL_0, TSPX_ENHANCED_PSM,
                                               2, L2CAP_LE_AUTOMATIC_CREDITS, ENHANCED_MTU_INITIAL, receive_buffers_2, cids);
                break;
            }
            if (handle_classic != HCI_CON_HANDLE_INVALID){
                printf("Connect to PSM 0x%02x - ECFC Classic\n", TSPX_LE_PSM);
                l2cap_enhanced_create_channels(&app_packet_handler, handle_classic, LEVEL_0, TSPX_ENHANCED_PSM,
                                               2, L2CAP_LE_AUTOMATIC_CREDITS, ENHANCED_MTU_INITIAL, receive_buffers_2, cids);
                break;
            }
            printf("Neither Classic nor LE connected\n");
            break;

        case 'D':
            printf("Creating connection to %s 0x%02x - ECFC LE\n", bd_addr_to_str(pts_address), TSPX_PSM_UNSUPPORTED);
            l2cap_enhanced_create_channels(&app_packet_handler, handle_le, LEVEL_0, TSPX_PSM_UNSUPPORTED,
                                           2, L2CAP_LE_AUTOMATIC_CREDITS, ENHANCED_MTU_INITIAL, receive_buffers_2, cids);
            break;

        case 'c':
            printf("Provide 10 credits\n");
            if (enhanced_data_channel){
                l2cap_enhanced_provide_credits(cid_enhanced, 10);
            } else {
                l2cap_le_provide_credits(cid_le, 10);
            }
            break;

        case 'e':
            printf("Re-register L2CAP ECFC channel as LEVEL_2 / insufficient key size\n");
            l2cap_enhanced_unregister_service(TSPX_ENHANCED_PSM);
            l2cap_enhanced_register_service(&app_packet_handler, TSPX_ENHANCED_PSM, MTU_MIN, LEVEL_2);
            enhanced_insufficient_key_size = true;
            break;

        case 'E':
            printf("Re-register L2CAP ECFC channel as LEVEL_3\n");
            l2cap_enhanced_unregister_service(TSPX_ENHANCED_PSM);
            l2cap_enhanced_register_service(&app_packet_handler, TSPX_ENHANCED_PSM, MTU_MIN, LEVEL_3);
            break;

        case 'f':
            printf("Require Authentication for ECFC\n");
            l2cap_enhanced_unregister_service(TSPX_ENHANCED_PSM);
            l2cap_enhanced_register_service(&app_packet_handler, TSPX_ENHANCED_PSM, MTU_MIN, LEVEL_2);
            enhanced_authorization_required = true;
            break;

        case 'm':
            printf("Enable manual credit management for incoming connections\n");
            initial_credits = 5;
            break;

        case 'r':
            printf("Reconfigure MTU = %u, MPS = %u\n", ENHANCED_MTU_RECONFIGURE, enhanced_reconfigure_mps[enhanced_reconfigure_index]);
            l2cap_enhanced_mps_set_max(enhanced_reconfigure_mps[enhanced_reconfigure_index++]);
            l2cap_enhanced_reconfigure(1, &cid_enhanced, ENHANCED_MTU_RECONFIGURE, receive_buffers_2);
            if (enhanced_reconfigure_index == enhanced_reconfigure_choices){
                enhanced_reconfigure_index = 0;
            }
            break;

        case 's':
            printf("Send L2CAP Data Short %s\n", data_short);
            todo_send_short = 1;
            if (enhanced_data_channel){
                l2cap_enhanced_data_channel_request_can_send_now_event(cid_enhanced);
            } else {
                l2cap_le_request_can_send_now_event(cid_le);
            }
            break;

        case 'S':
            printf("Send L2CAP Data Long %s\n", data_long);
            todo_send_long = 1;
            if (enhanced_data_channel){
                l2cap_enhanced_data_channel_request_can_send_now_event(cid_enhanced);
            } else {
                l2cap_le_request_can_send_now_event(cid_le);
            }
            break;

        case 'y':
            printf("Creating connection to %s 0x%02x - Classic\n", bd_addr_to_str(pts_address), TSPX_PSM);
            l2cap_create_channel(&app_packet_handler, pts_address, TSPX_PSM, 100, &cid_classic);
            break;

        case 'z':
            printf("Send L2CAP Data Classic %s\n", data_classic);
            l2cap_send(cid_classic, (uint8_t *) data_classic, strlen(data_classic));
            break;

        case 't':
            if (enhanced_data_channel){
                printf("Disconnect channel 0x%02x\n", cid_enhanced);
                l2cap_enhanced_disconnect(cid_enhanced);
            } else {
                printf("Disconnect channel 0x%02x\n", cid_le);
                l2cap_le_disconnect(cid_le);
            }
            break;

        default:
            show_usage();
            break;

    }
    return;
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    (void)argc;
    (void)argv;
    printf("BTstack LE Data Channel test starting up...\n");

    // register for HCI events
    hci_event_callback_registration.callback = &app_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    hci_disable_l2cap_timeout_check();

    // set up l2cap_le
    l2cap_init();
    
    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 
    sm_event_callback_registration.callback = &app_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    btstack_stdin_setup(stdin_process);

    // gap_random_address_set_update_period(5000);
    // gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
    gap_advertisements_set_data(sizeof(adv_general_discoverable), adv_general_discoverable);
    gap_advertisements_enable(1);
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(0);

    // le data channel setup
    l2cap_le_register_service(&app_packet_handler, TSPX_LE_PSM, LEVEL_0);

    // l2cap data channel setup
    l2cap_enhanced_register_service(&app_packet_handler, TSPX_ENHANCED_PSM, MTU_MIN, LEVEL_0);

    l2cap_enhanced_mps_set_min(TSPX_L2CA_CBMPS_MIN);
    // classic data channel setup
    l2cap_register_service(&app_packet_handler, TSPX_PSM, 100, LEVEL_0);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    return 0;
}
