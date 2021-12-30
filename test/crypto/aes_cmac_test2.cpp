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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci.h"
#include "btstack_util.h"
#include "bluetooth.h"
#include "btstack_crypto.h"

static btstack_crypto_aes128_cmac_t cmac_context;
static uint8_t cmac_calculated[16];

static const char key_string[]        = "2b7e1516 28aed2a6 abf71588 09cf4f3c";
static const char example_16_string[] = "6bc1bee2 2e409f96 e93d7e11 7393172a";
static const char example_40_string[] = "6bc1bee2 2e409f96 e93d7e11 7393172a ae2d8a57 1e03ac9c 9eb76fac 45af8e51 30c81c46 a35ce411";
static const char cmac_0_string[]     = "bb1d6929 e9593728 7fa37d12 9b756746";
static const char cmac_16_string[]    = "070a16b4 6b4d4144 f79bdd9d d04a287c";
static const char cmac_40_string[]    = "dfa66747 de9ae630 30ca3261 1497c827";

static int parse_hex(uint8_t * buffer, const char * hex_string){
    int len = 0;
    while (*hex_string){
        if (*hex_string == ' '){
            hex_string++;
            continue;
        }
        int high_nibble = nibble_for_char(*hex_string++);
        int low_nibble = nibble_for_char(*hex_string++);
        *buffer++ = (high_nibble << 4) | low_nibble;
        len++;
    }
    return len;
}

static void gatt_hash_calculated(void * arg){
    UNUSED(arg);
}

void CHECK_EQUAL_ARRAY(const uint8_t * expected, uint8_t * actual, int size){
    for (int i=0; i<size; i++){
        // printf("%03u: %02x - %02x\n", i, expected[i], actual[i]);
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

// mock
extern "C" {
    void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    }
    bool hci_can_send_command_packet_now(void){
        return true;
    }
    HCI_STATE hci_get_state(void){
        return HCI_STATE_WORKING;
    }
    void hci_halting_defer(void){
    }
    uint8_t hci_send_cmd(const hci_cmd_t *cmd, ...){
        printf("hci_send_cmd opcode %04x\n", cmd->opcode);
        return ERROR_CODE_SUCCESS;
    }
}

TEST_GROUP(AES_CMAC){
};

TEST(AES_CMAC,CMAC_0){
    uint8_t k[16];
    uint8_t cmac[16];
    parse_hex(k, key_string);
    parse_hex(cmac, cmac_0_string);
    btstack_crypto_aes128_cmac_message(&cmac_context, k, 0, NULL, cmac_calculated, gatt_hash_calculated, NULL);
    CHECK_EQUAL_ARRAY(cmac, cmac_calculated, 16);
}

TEST(AES_CMAC,CMAC_16){
    uint8_t k[16];
    uint8_t cmac[16];
    uint8_t m[16];
    parse_hex(k, key_string);
    parse_hex(m, example_16_string);
    parse_hex(cmac, cmac_16_string);
    btstack_crypto_aes128_cmac_message(&cmac_context, k, 16, m, cmac_calculated, gatt_hash_calculated, NULL);
    CHECK_EQUAL_ARRAY(cmac, cmac_calculated, 16);
}

TEST(AES_CMAC,CMAC_40){
    uint8_t k[16];
    uint8_t cmac[16];
    uint8_t m[40];
    parse_hex(k, key_string);
    parse_hex(m, example_40_string);
    parse_hex(cmac, cmac_40_string);
    btstack_crypto_aes128_cmac_message(&cmac_context, k, 40, m, cmac_calculated, gatt_hash_calculated, NULL);
    printf_hexdump(cmac_calculated, 16);
    CHECK_EQUAL_ARRAY(cmac, cmac_calculated, 16);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
