/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_chipset_nxp.c"

#include "btstack_chipset_nxp.h"
#include "btstack_debug.h"
#include "btstack_event.h"

#include <stdio.h>

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

// Firmware download protocol constants
#define NXP_V1_FW_REQ_PKT	0xa5
#define NXP_V1_CHIP_VER_PKT	0xaa
#define NXP_V3_FW_REQ_PKT	0xa7
#define NXP_V3_CHIP_VER_PKT	0xab

#define NXP_ACK_V1		0x5a
#define NXP_NAK_V1		0xbf
#define NXP_ACK_V3		0x7a
#define NXP_NAK_V3		0x7b
#define NXP_CRC_ERROR_V3	0x7c

// chip ids
#define NXP_CHIP_ID_W9098		0x5c03
#define NXP_CHIP_ID_IW416		0x7201
#define NXP_CHIP_ID_IW612		0x7601

#define NXP_MAX_RESEND_COUNT 5

// prototypes
static void nxp_w4_fw_req(void);
static void nxp_done(void);
static void nxp_prepare_chunk(void);
static void nxp_send_chunk(btstack_timer_source_t * ts);
static void nxp_done_with_status(uint8_t status);

// globals
static void (*nxp_download_complete)(uint8_t status);
static const btstack_uart_t * nxp_uart_driver;
static btstack_timer_source_t nxp_timer;
static bool                   nxp_have_firmware;

static const uint8_t * nxp_fw_data;
static uint32_t        nxp_fw_size;
static uint32_t        nxp_fw_offset;

static uint8_t       nxp_response_buffer[5];
static const uint8_t nxp_ack_buffer_v1[] = {NXP_ACK_V1 };
static const uint8_t nxp_ack_buffer_v3[] = {NXP_ACK_V3, 0x92 };
static uint16_t      nxp_fw_request_len;
static uint8_t       nxp_fw_resend_count;
static uint8_t       nxp_output_buffer[2048 + 1];

#ifdef HAVE_POSIX_FILE_IO
static char   nxp_firmware_path[1000];
static FILE * nxp_firmware_file;

static void nxp_load_firmware(void) {
    if (nxp_firmware_file == NULL){
        log_info("chipset-bcm: open file %s", nxp_firmware_path);
        nxp_firmware_file = fopen(nxp_firmware_path, "rb");
        if (nxp_firmware_file != NULL){
            nxp_have_firmware = true;
        }
    }

}
static uint16_t nxp_read_firmware(uint16_t bytes_to_read, uint8_t * buffer) {
    size_t bytes_read = fread(buffer, 1, bytes_to_read, nxp_firmware_file);
    return bytes_read;
}

static void nxp_unload_firmware(void) {
    btstack_assert(nxp_firmware_file != NULL);
    fclose(nxp_firmware_file);
    nxp_firmware_file = NULL;
}
#else
void nxp_load_firmware(void){
    nxp_have_firmware = true;
}

// read bytes from firmware file
static uint16_t nxp_read_firmware(uint16_t bytes_to_read, uint8_t * buffer){
    if (nxp_fw_request_len > nxp_fw_size - nxp_fw_offset){
        printf("fw_request_len %u > remaining file len %u\n", nxp_fw_request_len, nxp_fw_size - nxp_fw_offset);
        return nxp_fw_size - nxp_fw_offset;
    }
    memcpy(buffer, &nxp_fw_data[nxp_fw_offset], bytes_to_read);
    nxp_fw_offset += nxp_fw_request_len;
    return bytes_to_read;
}
static void nxp_unload_firmware(void) {
}
#endif

// first two uint16_t should xor to 0xffff
static bool nxp_valid_packet(void){
    switch (nxp_response_buffer[0]){
        case NXP_V1_FW_REQ_PKT:
        case NXP_V1_CHIP_VER_PKT:
            return ((nxp_response_buffer[1] ^ nxp_response_buffer[3]) == 0xff) && ((nxp_response_buffer[2] ^ nxp_response_buffer [4]) == 0xff);
        case NXP_V3_FW_REQ_PKT:
        case NXP_V3_CHIP_VER_PKT:
            // TODO: check crc-7
            return true;
        default:
            return false;
    }
}

static void nxp_start(){
    // start to read
    nxp_fw_resend_count = 0;
    nxp_fw_offset = 0;
    nxp_have_firmware = false;
    nxp_uart_driver->set_block_received(&nxp_w4_fw_req);
    nxp_uart_driver->receive_block(nxp_response_buffer, 5);
    log_info("nxp_start: wait for 0x%02x", NXP_V1_FW_REQ_PKT);
}

static void nxp_send_ack_v1(void (*done_handler)(void)){
    nxp_uart_driver->set_block_sent(done_handler);
    nxp_uart_driver->send_block(nxp_ack_buffer_v1, sizeof(nxp_ack_buffer_v1));
}

static void nxp_send_ack_v3(void (*done_handler)(void)){
    nxp_uart_driver->set_block_sent(done_handler);
    nxp_uart_driver->send_block(nxp_ack_buffer_v3, sizeof(nxp_ack_buffer_v3));
}

static void nxp_prepare_chunk(void){
    // delay chunk send by 5 ms
    btstack_run_loop_set_timer_handler(&nxp_timer, nxp_send_chunk);
    btstack_run_loop_set_timer(&nxp_timer, 5);
    btstack_run_loop_add_timer(&nxp_timer);
}

static void nxp_dummy(void){
}

static void nxp_w4_fw_req(void){
    // validate checksum
    if (nxp_valid_packet()){
        printf("RECV: ");
        printf_hexdump(nxp_response_buffer, sizeof(nxp_response_buffer));
        switch (nxp_response_buffer[0]){
            case NXP_V1_FW_REQ_PKT:
                // get firmware
                if (nxp_have_firmware == false){
                    nxp_load_firmware();
                }
                if (nxp_have_firmware == false){
                    printf("No firmware found, abort\n");
                    break;
                }
                nxp_fw_request_len = little_endian_read_16(nxp_response_buffer, 1);
                printf("RECV: NXP_V1_FW_REQ_PKT, len %u\n", nxp_fw_request_len);
                if (nxp_fw_request_len == 0){
                    printf("last chunk sent!\n");
                    nxp_unload_firmware();
                    nxp_send_ack_v1(nxp_done);
                } else {
                    nxp_send_ack_v1(nxp_prepare_chunk);
                }
                return;
            case NXP_V1_CHIP_VER_PKT:
                printf("RECV: NXP_V1_CHIP_VER_PKT, id = 0x%x02, revision = 0x%02x\n", nxp_response_buffer[0], nxp_response_buffer[1]);
                nxp_send_ack_v1(nxp_dummy);
                break;
            case NXP_V3_CHIP_VER_PKT:
                printf("RECV: NXP_V3_CHIP_VER_PKT , id = 0x%04x, loader 0x%02x\n", little_endian_read_16(nxp_response_buffer, 1), nxp_response_buffer[3]);
                nxp_send_ack_v3(nxp_dummy);
                break;
            default:
                printf("RECV: unknown packet type 0x%02x\n", nxp_response_buffer[0]);
                break;
        }
        nxp_start();
    }
    // drop byte and read another byte
    memmove(&nxp_response_buffer[0], &nxp_response_buffer[1], 4);
    nxp_uart_driver->receive_block(&nxp_response_buffer[4], 1);
}

static void nxp_send_chunk(btstack_timer_source_t * ts){
    if ((nxp_fw_request_len & 1) == 0){
        // update sttate
        nxp_fw_offset += nxp_fw_request_len;
        nxp_fw_resend_count = 0;
        // read next firmware chunk
        uint16_t bytes_read = nxp_read_firmware(nxp_fw_request_len, nxp_output_buffer);
        if (bytes_read < nxp_fw_request_len){
            printf("only %u of %u bytes available, abort.\n", bytes_read, nxp_fw_request_len);
            nxp_done_with_status(ERROR_CODE_HARDWARE_FAILURE);
            return;
        }
    } else {
        // resend last chunk if request len is odd
        if (nxp_fw_resend_count >= NXP_MAX_RESEND_COUNT){
            printf("Resent last block %u times, abort.", nxp_fw_resend_count);
            nxp_done_with_status(ERROR_CODE_HARDWARE_FAILURE);
            return;
        }
        nxp_fw_resend_count++;
    }

    printf("SEND: firmware %08x - %u bytes (%u. try)\n", nxp_fw_offset, nxp_fw_request_len, nxp_fw_resend_count + 1);
    nxp_uart_driver->set_block_received(&nxp_w4_fw_req);
    nxp_uart_driver->receive_block(nxp_response_buffer, 5);
    nxp_uart_driver->set_block_sent(nxp_dummy);
    nxp_uart_driver->send_block(nxp_output_buffer, nxp_fw_request_len);
}

static void nxp_done_with_status(uint8_t status){
    printf("DONE!\n");
    (*nxp_download_complete)(status);
}

static void nxp_done(void){
    nxp_done_with_status(ERROR_CODE_SUCCESS);
}

void btstack_chipset_nxp_set_v1_firmware_path(const char * firmware_path){
    btstack_strcpy(nxp_firmware_path, sizeof(nxp_firmware_path), firmware_path);
}

void btstack_chipset_nxp_set_firmware(const uint8_t * fw_data, uint32_t fw_size){
    nxp_fw_data = fw_data;
    nxp_fw_size = fw_size;
}

void btstack_chipset_nxp_download_firmware_with_uart(const btstack_uart_t *uart_driver, void (*done)(uint8_t status)) {
    nxp_uart_driver   = uart_driver;
    nxp_download_complete = done;

    int res = nxp_uart_driver->open();

    if (res) {
        log_error("uart_block init failed %u", res);
        nxp_download_complete(res);
    }

    nxp_start();
}


static void chipset_init(const void *transport_config){
    UNUSED(transport_config);
}

static btstack_chipset_t btstack_chipset_nxp = {
        .name = "NXP",
        .init = chipset_init,
        .next_command = NULL,
        .set_baudrate_command = NULL,
        .set_bd_addr_command = NULL
};

const btstack_chipset_t *btstack_chipset_nxp_instance(void){
    return &btstack_chipset_nxp;
}
