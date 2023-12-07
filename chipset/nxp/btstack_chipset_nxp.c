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
#include "hci_cmd.h"

#include <stdio.h>

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

// Firmware download protocol constants
#define NXP_V1_FIRMWARE_REQUEST_PACKET	0xa5
#define NXP_V1_CHIP_VERION_PACKET	0xaa
#define NXP_V3_FIRMWARE_REQUEST_PACKET	0xa7
#define NXP_V3_CHIP_VERSION_PACKET	0xab

#define NXP_ACK_V1		0x5a
#define NXP_NAK_V1		0xbf
#define NXP_ACK_V3		0x7a
#define NXP_NAK_V3		0x7b
#define NXP_CRC_ERROR_V3	0x7c

// chip ids
#define NXP_CHIP_ID_W9098		0x5c03
#define NXP_CHIP_ID_IW416		0x7201
#define NXP_CHIP_ID_IW612		0x7601

// firmwares
#define NXP_FIRMWARE_W9098	"uartuart9098_bt_v1.bin"
#define NXP_FIRMWARE_IW416	"uartiw416_bt_v0.bin"
#define NXP_FIRMWARE_IW612	"uartspi_n61x_v1.bin.se"

#define NXP_MAX_RESEND_COUNT 5

// prototypes
static void nxp_done_with_status(uint8_t status);
static void nxp_read_uart_handler(void);
static void nxp_send_chunk_v1(void);
static void nxp_send_chunk_v3(void);
static void nxp_start(void);
static void nxp_run(void);

// globals
static void (*nxp_download_complete_callback)(uint8_t status);
static const btstack_uart_t * nxp_uart_driver;
static btstack_timer_source_t nxp_timer;

static uint16_t nxp_chip_id;

static const uint8_t * nxp_fw_data;
static uint32_t        nxp_fw_size;
static uint32_t        nxp_fw_offset;

static uint8_t       nxp_input_buffer[10];
static uint16_t      nxp_input_pos;
static uint16_t      nxp_input_bytes_requested;

static uint8_t       nxp_output_buffer[2048 + 1];

static const uint8_t nxp_ack_buffer_v1[] = {NXP_ACK_V1 };
static const uint8_t nxp_ack_buffer_v3[] = {NXP_ACK_V3, 0x92 };

static uint16_t      nxp_fw_request_len;
static uint8_t       nxp_fw_resend_count;

// state / tasks
static bool         nxp_have_firmware;
static bool         nxp_tx_send_ack_v1;
static bool         nxp_tx_send_ack_v3;
static bool         nxp_tx_send_chunk_v1;
static bool         nxp_tx_send_chunk_v3;
static bool         nxp_tx_active;
static bool         nxp_download_done;
static uint8_t      nxp_download_statue;

#ifdef HAVE_POSIX_FILE_IO
static char   nxp_firmware_path[1000];
static FILE * nxp_firmware_file;

static char *nxp_fw_name_from_chipid(uint16_t chip_id)
{
    switch (chip_id) {
        case NXP_CHIP_ID_W9098:
            return NXP_FIRMWARE_W9098;
        case NXP_CHIP_ID_IW416:
            return NXP_FIRMWARE_IW416;
        case NXP_CHIP_ID_IW612:
            return NXP_FIRMWARE_IW612;
        default:
            log_error("Unknown chip id 0x%04x", chip_id);
            return NULL;
    }
}

static void nxp_load_firmware(void) {
    if (nxp_firmware_file == NULL){
        log_info("open file %s", nxp_firmware_path);
        nxp_firmware_file = fopen(nxp_firmware_path, "rb");
        if (nxp_firmware_file != NULL){
            printf("Open file '%s' failed\n", nxp_firmware_path);
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

static void nxp_send_ack_v1() {
    printf("SEND: ack v1\n");
    btstack_assert(nxp_tx_active == false);
    nxp_tx_active = true;
    nxp_uart_driver->send_block(nxp_ack_buffer_v1, sizeof(nxp_ack_buffer_v1));
}

static void nxp_send_ack_v3() {
    printf("SEND: ack v3\n");
    btstack_assert(nxp_tx_active == false);
    nxp_tx_active = true;
    nxp_uart_driver->send_block(nxp_ack_buffer_v3, sizeof(nxp_ack_buffer_v3));
}

static bool nxp_valid_packet(void){
    switch (nxp_input_buffer[0]){
        case NXP_V1_FIRMWARE_REQUEST_PACKET:
        case NXP_V1_CHIP_VERION_PACKET:
            // first two uint16_t should xor to 0xffff
            return ((nxp_input_buffer[1] ^ nxp_input_buffer[3]) == 0xff) && ((nxp_input_buffer[2] ^ nxp_input_buffer [4]) == 0xff);
        case NXP_V3_CHIP_VERSION_PACKET:
        case NXP_V3_FIRMWARE_REQUEST_PACKET:
            // TODO: check crc-8
            return true;
        default:
            return false;
    }
}

static void nxp_handle_chip_version_v1(void){
    printf("RECV: NXP_V1_CHIP_VER_PKT, id = 0x%x02, revision = 0x%02x\n", nxp_input_buffer[0], nxp_input_buffer[1]);
    nxp_tx_send_ack_v1 = true;
    nxp_run();
}

static void nxp_handle_chip_version_v3(void){
    nxp_chip_id = little_endian_read_16(nxp_input_buffer, 1);
    btstack_strcpy(nxp_firmware_path, sizeof(nxp_firmware_path), nxp_fw_name_from_chipid(nxp_chip_id));
    printf("RECV: NXP_V3_CHIP_VER_PKT, id = 0x%04x, loader 0x%02x -> firmware '%s'\n", nxp_chip_id, nxp_input_buffer[3], nxp_firmware_path);
    nxp_tx_send_ack_v3 = true;
    nxp_run();
}

static void nxp_prepare_firmware(void){
    // get firmware
    if (nxp_have_firmware == false){
        nxp_load_firmware();
    }
    if (nxp_have_firmware == false){
        printf("No firmware found, abort\n");
    }
}

static void nxp_send_chunk_v1(void){
    if (nxp_fw_request_len == 0){
        printf("last chunk sent!\n");
        nxp_unload_firmware();
        nxp_done_with_status(ERROR_CODE_SUCCESS);
        return;
    } else if ((nxp_fw_request_len & 1) == 0){
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
    btstack_assert(nxp_tx_active == false);
    nxp_tx_active = true;
    nxp_uart_driver->send_block(nxp_output_buffer, nxp_fw_request_len);
}

static void nxp_send_chunk_v3(void){
    // update state
    nxp_fw_offset += nxp_fw_request_len;
    nxp_fw_resend_count = 0;
    if (nxp_fw_request_len == 0){
        printf("last chunk sent!\n");
        nxp_unload_firmware();
        nxp_done_with_status(ERROR_CODE_SUCCESS);
        return;
    }
    // read next firmware chunk
    uint16_t bytes_read = nxp_read_firmware(nxp_fw_request_len, nxp_output_buffer);
    if (bytes_read < nxp_fw_request_len){
        printf("only %u of %u bytes available, abort.\n", bytes_read, nxp_fw_request_len);
        nxp_done_with_status(ERROR_CODE_HARDWARE_FAILURE);
        return;
    }
    printf("SEND: firmware %08x - %u bytes (%u. try)\n", nxp_fw_offset, nxp_fw_request_len, nxp_fw_resend_count + 1);
    btstack_assert(nxp_tx_active == false);
    nxp_tx_active = true;
    nxp_uart_driver->send_block(nxp_output_buffer, nxp_fw_request_len);
}

static void nxp_handle_firmware_request_v1(void){
    nxp_fw_request_len = little_endian_read_16(nxp_input_buffer, 1);
    printf("RECV: NXP_V1_FW_REQ_PKT, len %u\n", nxp_fw_request_len);

    nxp_prepare_firmware();
    if (nxp_have_firmware == false){
        return;
    }
    nxp_tx_send_ack_v1 = true;
    nxp_tx_send_chunk_v1 = true;
    nxp_run();
}

static void nxp_handle_firmware_request_v3(void){
    nxp_fw_request_len = little_endian_read_16(nxp_input_buffer, 1);
    printf("RECV: NXP_V3_FW_REQ_PKT, len %u\n", nxp_fw_request_len);

    nxp_prepare_firmware();
    if (nxp_have_firmware == false){
        return;
    }
    nxp_tx_send_ack_v3 = true;
    nxp_tx_send_chunk_v3 = true;
    nxp_run();
}

static void nxp_start_read(uint16_t bytes_to_read){
    nxp_input_bytes_requested = bytes_to_read;
    nxp_uart_driver->receive_block(&nxp_input_buffer[nxp_input_pos], bytes_to_read);
}

static void nxp_read_uart_handler(void){
    uint16_t bytes_to_read;
    if (nxp_input_pos == 0){
        switch (nxp_input_buffer[0]){
            case NXP_V1_CHIP_VERION_PACKET:
            case NXP_V3_CHIP_VERSION_PACKET:
            case NXP_V1_FIRMWARE_REQUEST_PACKET:
                nxp_input_pos++;
                bytes_to_read = 4;
                break;
            case NXP_V3_FIRMWARE_REQUEST_PACKET:
                nxp_input_pos++;
                bytes_to_read = 9;
                break;
            default:
                // invalid packet type, skip and get next byte
                bytes_to_read = 1;
                break;
        }
    } else {
        nxp_input_pos += nxp_input_bytes_requested;
        printf("RECV: ");
        printf_hexdump(nxp_input_buffer, nxp_input_pos);
        switch (nxp_input_buffer[0]){
            case NXP_V1_CHIP_VERION_PACKET:
                nxp_handle_chip_version_v1();
                break;
            case NXP_V3_CHIP_VERSION_PACKET:
                nxp_handle_chip_version_v3();
                break;
            case NXP_V1_FIRMWARE_REQUEST_PACKET:
                nxp_handle_firmware_request_v1();
                break;
            case NXP_V3_FIRMWARE_REQUEST_PACKET:
                nxp_handle_firmware_request_v3();
                break;
            default:
                btstack_assert(false);
                break;
        }
        nxp_input_pos = 0;
        bytes_to_read = 1;

        // stop timer
        btstack_run_loop_remove_timer(&nxp_timer);
    }
    nxp_start_read(bytes_to_read);
}

static void nxp_run(void){
    if (nxp_tx_active) {
        return;
    }
    if (nxp_tx_send_ack_v1){
        nxp_tx_send_ack_v1 = false;
        nxp_send_ack_v1();
        return;
    }
    if (nxp_tx_send_ack_v3){
        nxp_tx_send_ack_v3 = false;
        nxp_send_ack_v3();
        return;
    }
    if (nxp_tx_send_chunk_v1){
        nxp_tx_send_chunk_v1 = false;
        nxp_send_chunk_v1();
        return;
    }
    if (nxp_tx_send_chunk_v3){
        nxp_tx_send_chunk_v3 = false;
        nxp_send_chunk_v3();
        return;
    }
    if (nxp_download_done){
        nxp_download_done = false;
        (*nxp_download_complete_callback)(nxp_download_statue);
    }
}

static void nxp_write_uart_handler(void){
    btstack_assert(nxp_tx_active == true);
    printf("SEND: complete\n");
    nxp_tx_active = false;
    nxp_run();
}

static void nxp_start(void){
    nxp_fw_resend_count = 0;
    nxp_fw_offset = 0;
    nxp_have_firmware = false;
    nxp_uart_driver->set_block_received(&nxp_read_uart_handler);
    nxp_uart_driver->set_block_sent(&nxp_write_uart_handler);
    nxp_uart_driver->receive_block(nxp_input_buffer, 1);
}

static void nxp_done_with_status(uint8_t status){
    nxp_download_statue = status;
    nxp_download_done = true;
    printf("DONE! status 0x%02x\n", status);
    nxp_run();
}

void btstack_chipset_nxp_set_v1_firmware_path(const char * firmware_path){
    btstack_strcpy(nxp_firmware_path, sizeof(nxp_firmware_path), firmware_path);
}

void btstack_chipset_nxp_set_firmware(const uint8_t * fw_data, uint32_t fw_size){
    nxp_fw_data = fw_data;
    nxp_fw_size = fw_size;
}

void nxp_timer_handler(btstack_timer_source_t *ts) {
    printf("No firmware request received, assuming firmware already loaded\n");
    nxp_done_with_status(ERROR_CODE_SUCCESS);
    nxp_run();
}

void btstack_chipset_nxp_download_firmware_with_uart(const btstack_uart_t *uart_driver, void (*callback)(uint8_t status)) {
    nxp_uart_driver = uart_driver;
    nxp_download_complete_callback = callback;

    int res = nxp_uart_driver->open();

    if (res) {
        log_error("uart_block init failed %u", res);
        nxp_done_with_status(ERROR_CODE_HARDWARE_FAILURE);
        nxp_run();
        return;
    }

    // if firmware is loaded, there will be no firmware request
    btstack_run_loop_set_timer(&nxp_timer, 250);
    btstack_run_loop_set_timer_handler(&nxp_timer, &nxp_timer_handler);
    btstack_run_loop_add_timer(&nxp_timer);

    nxp_start();
}

// init script support
static enum {
    NXP_INIT_SEND_SCO_CONFIG,
    NXP_INIT_SEND_HOST_CONTROL_ENABLE,
    NXP_INIT_SEND_WRITE_PCM_SETTINGS,
    NXP_INIT_SEND_WRITE_PCM_SYNC_SETTINGS,
    NXP_INIT_SEND_WRITE_PCM_LINK_SETTINGS,
    NXP_INIT_DONE,
} nxp_init_state;

#ifdef ENABLE_SCO_OVER_HCI
// Voice Path: Host
static const uint8_t nxp_chipset_sco_routing_path = 0;
#endif

#ifdef ENABLE_SCO_OVER_PCM
// Voice Path: PCM/I2S
static const uint8_t nxp_chipset_sco_routing_path = 1;
#endif

static void nxp_init(const void *transport_config){
    UNUSED(transport_config);
    nxp_init_state = NXP_INIT_SEND_SCO_CONFIG;
}

static btstack_chipset_result_t nxp_next_command(uint8_t * hci_cmd_buffer) {
    switch (nxp_init_state){
        case NXP_INIT_SEND_SCO_CONFIG:
#if defined(ENABLE_SCO_OVER_HCI) || defined(ENABLE_SCO_OVER_PCM)
#ifdef ENABLE_NXP_PCM_WBS
            nxp_init_state = NXP_INIT_SEND_HOST_CONTROL_ENABLE;
#else
            nxp_init_state = NXP_INIT_SEND_WRITE_PCM_SETTINGS;
#endif
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_set_sco_data_path, nxp_chipset_sco_routing_path);
            return BTSTACK_CHIPSET_VALID_COMMAND;
#endif
#ifdef ENABLE_SCO_OVER_PCM
        case NXP_INIT_SEND_HOST_CONTROL_ENABLE:
            nxp_init_state = NXP_INIT_SEND_WRITE_PCM_SETTINGS;
            // Host Control enabled
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_host_pcm_i2s_control_enable, 1);
            return BTSTACK_CHIPSET_VALID_COMMAND;
        case NXP_INIT_SEND_WRITE_PCM_SETTINGS:
            nxp_init_state = NXP_INIT_SEND_WRITE_PCM_SYNC_SETTINGS;
            // PCM/I2S master mode
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_write_pcm_i2s_settings, 0x02);
            return BTSTACK_CHIPSET_VALID_COMMAND;
        case NXP_INIT_SEND_WRITE_PCM_SYNC_SETTINGS:
            nxp_init_state = NXP_INIT_SEND_WRITE_PCM_LINK_SETTINGS;
#ifdef ENABLE_NXP_PCM_WBS
            // 16 kHz sync, 2048 kHz, data in left channel, DIN sampled on rising edge, DOUT driven on falling edge, I2Sa
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_write_pcm_i2s_sync_settings, 0x03, 0x071e);
#else
            //  8 kHz sync, 2048 kHz, data in left channel, DIN sampled on rising edge, DOUT driven on falling edge, I2S
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_write_pcm_i2s_sync_settings, 0x03, 0x031e);
#endif
            return BTSTACK_CHIPSET_VALID_COMMAND;
        case NXP_INIT_SEND_WRITE_PCM_LINK_SETTINGS:
            nxp_init_state = NXP_INIT_DONE;
            // 1st SCO Link PCM Logical Slot 0, PCM start slot 1
            hci_cmd_create_from_template_with_vargs(hci_cmd_buffer, &hci_nxp_write_pcm_link_settings, 0x0004);
            return BTSTACK_CHIPSET_VALID_COMMAND;
#endif
        default:
            break;
    }
    return BTSTACK_CHIPSET_DONE;
}

static btstack_chipset_t btstack_chipset_nxp = {
        .name = "NXP",
        .init = nxp_init,
        .next_command = nxp_next_command,
        .set_baudrate_command = NULL,
        .set_bd_addr_command = NULL
};

const btstack_chipset_t *btstack_chipset_nxp_instance(void){
    return &btstack_chipset_nxp;
}

uint32_t btstack_chipset_nxp_get_initial_baudrate(void){
    switch (nxp_chip_id){
        case NXP_CHIP_ID_IW612:
            return 3000000;
        default:
            return 115200;
    }
}
