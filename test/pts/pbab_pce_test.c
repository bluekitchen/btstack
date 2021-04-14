/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "pbap_pce_test.c"

// test console for PTS PBAP PCE Test cases

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_run_loop.h"
#include "l2cap.h"
#include "classic/rfcomm.h"
#include "btstack_event.h"
#include "classic/goep_client.h"
#include "classic/pbap_client.h"

#include "btstack_stdin.h"
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static bd_addr_t    remote_addr;
// MBP2016 "F4-0F-24-3B-1B-E1"
// Nexus 7 "30-85-A9-54-2E-78"
// iPhone SE "BC:EC:5D:E6:15:03"
// iPhone 12 Mini "DC:52:85:B4:AD:2B"
// PTS "001BDC080AA5"
// PTS "00:1B:DC:08:E2:72"
static  char * remote_addr_string = "00:1B:DC:08:E2:72";
static char * phone_number = "PTS";

static const char * pb_name   = "pb";
static const char * fav_name  = "fav";
static const char * ich_name  = "ich";
static const char * och_name  = "och";
static const char * mch_name  = "mch";
static const char * cch_name  = "cch";
static const char * spd_name  = "spd";

static const char * bt_uid = "X-BT-UID:1234567890ABCDEF1234567890000001";
static const char * bt_uci = "X-BT-UCI;skype:PTS1";

static const char * phonebook_name;
static char phonebook_folder[30];
static char phonebook_path[30];

static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint16_t pbap_cid;

static int sim1_selected;

static void select_phonebook(const char * phonebook){
    phonebook_name = phonebook;
    sprintf(phonebook_path, "%s%s.vcf", sim1_selected ? "SIM1/telecom/" : "telecom/", phonebook);
    sprintf(phonebook_folder, "%s%s",   sim1_selected ? "SIM1/telecom/" : "telecom/", phonebook);
    printf("[-] Phonebook name   '%s'\n", phonebook_name);
    printf("[-] Phonebook folder '%s'\n", phonebook_folder);
    printf("[-] Phonebook path   '%s'\n", phonebook_path);
}


// Testig User Interface 
static void show_usage(void){
    bd_addr_t iut_address;
    gap_local_bd_addr(iut_address);

    printf("\n--- Bluetooth PBAP Client (HF) Test Console %s ---\n", bd_addr_to_str(iut_address));
    printf("OBEX transport:  ");
#ifdef ENABLE_GOEP_L2CAP
    printf("L2CAP, ");
#endif
    printf("RFCOMM\n");
    printf("Phonebook:       '%s'\n", phonebook_folder);
    printf("Phonebook name   '%s'\n", phonebook_name);
    printf("Phonebook path   '%s'\n", phonebook_path);
    printf("\n");
    printf("a - establish PBAP connection to %s\n", bd_addr_to_str(remote_addr));
    printf("b - select SIM1\n");
    printf("r - set path to '/telecom'\n");
    printf("R - set path to '/SIM1/telecom'\n");
    printf("u - set path to '%s'\n", phonebook_folder);
    printf("v - set vCardSelector to N and TEL\n");
    printf("V - set vCardSelectorOperator to AND\n");

    printf("e - select phonebook '%s'\n", pb_name);
    printf("f - select phonebook '%s'\n", fav_name);
    printf("i - select phonebook '%s'\n", ich_name);
    printf("o - select phonebook '%s'\n", och_name);
    printf("m - select phonebook '%s'\n", mch_name);
    printf("c - select phonebook '%s'\n", cch_name);
    printf("s - select phonebook '%s'\n", spd_name);

    printf("d - get size of    '%s'\n",             phonebook_path);
    printf("g - pull phonebook '%s'\n",             phonebook_path);
    printf("h - pull vCard listing '%s'\n",         phonebook_name);
    printf("l - get vCard '0.vcf'\n");
    printf("L - get vCard '%s'\n", bt_uid);
    printf("M - get vCard '%s'\n", bt_uci);
    printf("j - Lookup contact with number '%s'\n", phone_number);
    printf("t - disconnect\n");
    printf("p - authenticate using password '0000'\n");
    printf("x - abort operation\n");
    printf("\n");
}

static void stdin_process(char c){
    switch (c){
        case 'a':
            printf("[+] Connecting to %s...\n", bd_addr_to_str(remote_addr));
            pbap_connect(&packet_handler, remote_addr, &pbap_cid);
            break;
        case 'b':
            printf("[+] SIM1 selected'\n");
            sim1_selected = 1;
            select_phonebook(phonebook_name);
            break;
        case 'd':
            printf("[+] Get size of phonebook '%s'\n", phonebook_path);
            pbap_get_phonebook_size(pbap_cid, phonebook_path);
            break;
        case 'g':
            printf("[+] Pull phonebook '%s'\n", phonebook_path);
            pbap_pull_phonebook(pbap_cid, phonebook_path);
            break;
        case 'h':
            printf("[+] Pull vCard list for '%s'\n", phonebook_name);
            pbap_pull_vcard_listing(pbap_cid, phonebook_name);
            break;
        case 'j':
            printf("[+] Lookup name for number '%s'\n", phone_number);
            pbap_lookup_by_number(pbap_cid, phone_number);
            break;
        case 'l':
            printf("[+] Pull vCard '1.vcf'\n");
            pbap_pull_vcard_entry(pbap_cid, "1.vcf");
            break;
        case 'L':
            printf("[+] Pull vCard '%s'\n", bt_uid);
            pbap_pull_vcard_entry(pbap_cid, bt_uid);
            break;
        case 'M':
            printf("[+] Pull vCard '%s'\n", bt_uci);
            pbap_pull_vcard_entry(pbap_cid, bt_uci);
            break;
        case 'c':
            printf("[+] Select phonebook '%s'\n", cch_name);
            select_phonebook(cch_name);
            break;
        case 'e':
            printf("[+] Select phonebook '%s'\n", pb_name);
            select_phonebook(pb_name);
            break;
        case 'f':
            printf("[+] Select phonebook '%s'\n", fav_name);
            select_phonebook(fav_name);
            break;
        case 'i':
            printf("[+] Select phonebook '%s'\n", ich_name);
            select_phonebook(ich_name);
            break;
        case 'm':
            printf("[+] Select phonebook '%s'\n", mch_name);
            select_phonebook(mch_name);
            break;
        case 'o':
            printf("[+] Select phonebook '%s'\n", och_name);
            select_phonebook(och_name);
            break;
        case 's':
            printf("[+] Select phonebook '%s'\n", spd_name);
            select_phonebook(spd_name);
            break;

        case 'p':
            pbap_authentication_password(pbap_cid, "0000");
            break;
        case 'v':
            printf("[+] Set vCardSelector 'N' and 'TEL'\n");
            pbap_set_vcard_selector(pbap_cid, PBAP_PROPERTY_MASK_N | PBAP_PROPERTY_MASK_TEL);
            break;
        case 'V':
            printf("[+] Set vCardSelectorOperator 'AND'\n");
            pbap_set_vcard_selector_operator(pbap_cid, PBAP_VCARD_SELECTOR_OPERATOR_AND);
            break;
        case 'r':
            printf("[+] Set path to '/telecom'\n");
            pbap_set_phonebook(pbap_cid, "telecom");
            break;
        case 'R':
            printf("[+] Set path to '/SIM1/telecom'\n");
            pbap_set_phonebook(pbap_cid, "SIM1/telecom");
            break;
        case 'u':
            printf("[+] Set path to '%s'\n", phonebook_folder);
            pbap_set_phonebook(pbap_cid, phonebook_folder);
            break;
        case 'x':
            printf("[+] Abort'\n");
            pbap_abort(pbap_cid);
            break;
        case 't':
            pbap_disconnect(pbap_cid);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }
}

// packet handler for interactive console
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    int i;
    uint8_t status;
    char buffer[32];
    switch (packet_type){
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    // BTstack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        show_usage();
                    }
                    break;
                case HCI_EVENT_PBAP_META:
                    switch (hci_event_pbap_meta_get_subevent_code(packet)){
                        case PBAP_SUBEVENT_CONNECTION_OPENED:
                            status = pbap_subevent_connection_opened_get_status(packet);
                            if (status){
                                printf("[!] Connection failed, status 0x%02x\n", status);
                            } else {
                                printf("[+] Connected\n");
                            }
                            break;
                        case PBAP_SUBEVENT_CONNECTION_CLOSED:
                            printf("[+] Connection closed\n");
                            break;
                        case PBAP_SUBEVENT_OPERATION_COMPLETED:
                            printf("[+] Operation complete\n");
                            break;
                        case PBAP_SUBEVENT_AUTHENTICATION_REQUEST:
                            printf("[?] Authentication requested\n");
                            break;
                        case PBAP_SUBEVENT_PHONEBOOK_SIZE:
                            status = pbap_subevent_phonebook_size_get_status(packet);
                            if (status){
                                printf("[!] Get Phonebook size error: 0x%x\n", status);
                            } else {
                                printf("[+] Phonebook size: %u\n", pbap_subevent_phonebook_size_get_phoneboook_size(packet));
                            }
                            break;
                        case PBAP_SUBEVENT_CARD_RESULT:
                            memcpy(buffer, pbap_subevent_card_result_get_name(packet), pbap_subevent_card_result_get_name_len(packet));
                            buffer[pbap_subevent_card_result_get_name_len(packet)] = 0;
                            printf("[-] Name:   '%s'\n", buffer);
                            memcpy(buffer, pbap_subevent_card_result_get_handle(packet), pbap_subevent_card_result_get_handle_len(packet));
                            buffer[pbap_subevent_card_result_get_handle_len(packet)] = 0;
                            printf("[-] Handle: '%s'\n", buffer);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
            }
            break;
        case PBAP_DATA_PACKET:
            for (i=0;i<size;i++){
                printf("%c", packet[i]);
            }
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;
        
    // init L2CAP
    l2cap_init();

    // init RFCOM
    rfcomm_init();

    // init GOEP Client
    goep_client_init();

    // init PBAP Client
    pbap_client_init();

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sscanf_bd_addr(remote_addr_string, remote_addr);

    select_phonebook(pb_name);

    btstack_stdin_setup(stdin_process);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
