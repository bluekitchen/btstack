/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_chipset_intel_firmware.c"

#include <fcntl.h>
#include <stdio.h>
#include <inttypes.h>

#include "btstack_chipset_intel_firmware.h"

#include "bluetooth.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"

#ifdef _MSC_VER
 // ignore deprecated warning for fopen
#pragma warning(disable : 4996)
#endif

// assert outgoing and incoming hci packet buffers can hold max hci command resp. event packet
#if HCI_OUTGOING_PACKET_BUFFER_SIZE < (HCI_CMD_HEADER_SIZE + 255)
#error "HCI_OUTGOING_PACKET_BUFFER_SIZE to small. Outgoing HCI packet buffer to small for largest HCI Command packet. Please set HCI_ACL_PAYLOAD_SIZE to 258 or higher."
#endif
#if HCI_INCOMING_PACKET_BUFFER_SIZE < (HCI_EVENT_HEADER_SIZE_HEADER_SIZE + 255)
#error "HCI_INCOMING_PACKET_BUFFER_SIZE to small. Incoming HCI packet buffer to small for largest HCI Event packet. Please set HCI_ACL_PAYLOAD_SIZE to 257 or higher."
#endif

// Vendor specific structs

typedef struct {
    uint8_t status;
    uint8_t hw_platform;
    uint8_t hw_variant;
    uint8_t hw_revision;
    uint8_t fw_variant;
    uint8_t fw_revision;
    uint8_t fw_build_num;
    uint8_t fw_build_ww;
    uint8_t fw_build_yy;
    uint8_t fw_patch_num;
} intel_version_t;

typedef struct {
    uint8_t     status;
    uint8_t     otp_format;
    uint8_t     otp_content;
    uint8_t     otp_patch;
    uint16_t    dev_revid;
    uint8_t     secure_boot;
    uint8_t     key_from_hdr;
    uint8_t     key_type;
    uint8_t     otp_lock;
    uint8_t     api_lock;
    uint8_t     debug_lock;
    bd_addr_t   otp_bdaddr;
    uint8_t     min_fw_build_nn;
    uint8_t     min_fw_build_cw;
    uint8_t     min_fw_build_yy;
    uint8_t     limited_cce;
    uint8_t     unlocked_state;
} intel_boot_params_t;

typedef enum {
    INTEL_CONTROLLER_LEGACY,
    INTEL_CONTROLLER_TLV,
} intel_controller_mode_t;

typedef enum {
    STATE_INITIAL = 0,
    STATE_HANDLE_HCI_RESET = 1,
    STATE_HANDLE_READ_VERSION_1 = 2,
    STATE_HANDLE_READ_SECURE_BOOT_PARAMS = 3,
    STATE_SEND_PUBLIC_KEY_1 = 4,
    STATE_SEND_PUBLIC_KEY_2 = 5,
    STATE_SEND_SIGNATURE_PART_1 = 6,
    STATE_SEND_SIGNATURE_PART_2 = 7,
    STATE_SEND_FIRMWARE_CHUNK = 8,
    STATE_HANDLE_FIRMWARE_CHUNKS_SENT = 9,
    STATE_HANDLE_VENDOR_SPECIFIC_EVENT_02 = 10,
    STATE_HANDLE_READ_VERSION_2 = 11,
    STATE_SEND_DDC = 12,
    STATE_DONE = 15
} state_t;

// Vendor specific commands

static const hci_cmd_t hci_intel_read_version = {
    0xfc05, "1"
};
static const hci_cmd_t hci_intel_read_secure_boot_params = {
    0xfc0d, ""
};

static const hci_cmd_t hci_intel_reset_param = {
    0xfc01, "11111111"
};

static const hci_cmd_t hci_intel_set_event_mask = {
    0xfc52, "11111111"    
};

// state

const char * firmware_folder_path = ".";

static intel_version_t     intel_version;
static intel_boot_params_t intel_boot_params;

static intel_controller_mode_t controller_mode;

const hci_transport_t * transport;

static state_t state;

static int vendor_firmware_complete_received;
static int waiting_for_command_complete;

static uint8_t hci_outgoing[300];
static uint8_t fw_buffer[300];

static FILE *   fw_file;
static size_t   fw_offset;

static void (*done)(int result);

// protogtypes

static void state_machine(uint8_t *packet, uint16_t size);

// functions

static uint16_t intel_get_dev_revid(intel_boot_params_t * boot_params){
    return little_endian_read_16((uint8_t*)&intel_boot_params.dev_revid, 0);
}

static void dump_intel_version(intel_version_t     * version){
    log_info("status       0x%02x", version->status);
    log_info("hw_platform  0x%02x", version->hw_platform);
    log_info("hw_variant   0x%02x", version->hw_variant);
    log_info("hw_revision  0x%02x", version->hw_revision);
    log_info("fw_variant   0x%02x", version->fw_variant);
    log_info("fw_revision  0x%02x", version->fw_revision);
    log_info("fw_build_num 0x%02x", version->fw_build_num);
    log_info("fw_build_ww  0x%02x", version->fw_build_ww);
    log_info("fw_build_yy  0x%02x", version->fw_build_yy);
    log_info("fw_patch_num 0x%02x", version->fw_patch_num);
}

static void dump_intel_boot_params(intel_boot_params_t * boot_params){
    bd_addr_t addr;
    reverse_bd_addr(boot_params->otp_bdaddr, addr);
    log_info("Device revision: %u", intel_get_dev_revid(boot_params));
    log_info("Secure Boot:  %s", boot_params->secure_boot ? "enabled" : "disabled");
    log_info("OTP lock:     %s", boot_params->otp_lock    ? "enabled" : "disabled");
    log_info("API lock:     %s", boot_params->api_lock    ? "enabled" : "disabled");
    log_info("Debug lock:   %s", boot_params->debug_lock  ? "enabled" : "disabled");
    log_info("Minimum firmware build %u week %u %u", boot_params->min_fw_build_nn, boot_params->min_fw_build_cw, 2000 + boot_params->min_fw_build_yy);
    log_info("OTC BD_ADDR:  %s", bd_addr_to_str(addr));
}

static int intel_get_firmware_name(intel_version_t *version, intel_boot_params_t *boot_params, const char *folder_path,
                                   const char *suffix, char *firmware_path, size_t firmware_path_len) {
    switch (version->hw_variant)
    {
        case 0x0b: /* SfP */
        case 0x0c: /* WsP */
            snprintf(firmware_path, firmware_path_len, "%s/ibt-%u-%u.%s",
                     folder_path,
                     version->hw_variant,
                     intel_get_dev_revid(boot_params),
                     suffix);
            break;
        case 0x11: /* JfP */
        case 0x12: /* ThP */
        case 0x13: /* HrP */
        case 0x14: /* CcP */
            snprintf(firmware_path, firmware_path_len, "%s/ibt-%u-%u-%u.%s",
                     folder_path,
                     version->hw_variant,
                     version->hw_revision,
                     version->fw_revision,
                     suffix);
            break;
        default:
            printf("Unsupported Intel hardware variant (%u)\n", version->hw_variant);
            break;
    }

    return 0;
}

static int transport_send_packet(uint8_t packet_type, const uint8_t * packet, uint16_t size){
    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, (uint8_t*) packet, size);
    return transport->send_packet(packet_type, (uint8_t *) packet, size);
}

static int transport_send_cmd_va_arg(const hci_cmd_t *cmd, va_list argptr){
    uint8_t * packet = hci_outgoing;
    uint16_t size = hci_cmd_create_from_template(packet, cmd, argptr);
    return transport_send_packet(HCI_COMMAND_DATA_PACKET, packet, size);
}

static int transport_send_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    int res = transport_send_cmd_va_arg(cmd, argptr);
    va_end(argptr);
    return res;
}

static int transport_send_intel_secure(uint8_t fragment_type, const uint8_t * data, uint8_t len){
    little_endian_store_16(hci_outgoing, 0, 0xfc09);
    hci_outgoing[2] = 1 + len;
    hci_outgoing[3] = fragment_type;
    memcpy(&hci_outgoing[4], data, len);
    uint16_t size = 3 +  1 + len;
    return transport_send_packet(HCI_ACL_DATA_PACKET, hci_outgoing, size);
}

static int transport_send_intel_ddc(const uint8_t * data, uint8_t len){
    little_endian_store_16(hci_outgoing, 0, 0xfc8b);
    hci_outgoing[2] = len;
    memcpy(&hci_outgoing[3], data, len);
    uint16_t size = 3 +  len;
    return transport_send_packet(HCI_COMMAND_DATA_PACKET, hci_outgoing, size);
}

// read data from fw file and send it via intel_secure + update state
static int intel_send_fragment(uint8_t fragment_type, uint8_t len){
    size_t res = fread(fw_buffer, 1, len, fw_file);
    log_info("offset %6" PRId32 ", read %3u -> res %" PRId32 "", (int32_t)fw_offset, len, (int32_t)res);
    fw_offset += res;
    return transport_send_intel_secure(fragment_type, fw_buffer, len);
}

// read data from  ddc file and send iva intel ddc command
// @returns -1 on eof
static int intel_send_ddc(void){
    size_t res;
    // read len
    res = fread(fw_buffer, 1, 1, fw_file);
    log_info("offset %6" PRId32 ", read 1 -> res %" PRId32 "", (int32_t)fw_offset, (int32_t)res);
    if (res == 0) return -1;
    uint8_t len = fw_buffer[0];
    fw_offset += 1;
    res = fread(&fw_buffer[1], 1, len, fw_file);
    log_info("offset %6" PRId32 ", read %u -> res %" PRId32 "", (int32_t)fw_offset, 1, (int32_t)res);
    return transport_send_intel_ddc(fw_buffer, 1 + len);
}

static void state_machine(uint8_t *packet, uint16_t size) {
    size_t res;
    size_t buffer_offset;
    bd_addr_t addr;
    char    fw_path[300];

    if (packet){
        // firmware upload complete event?
        if (packet[0] == 0xff && packet[2] == 0x06) {
            vendor_firmware_complete_received = 1;
        }

        // command complete
        if (packet[0] == 0x0e){
            waiting_for_command_complete = 0;
        }
    }

    switch (state){
        case STATE_INITIAL:
            controller_mode = INTEL_CONTROLLER_LEGACY;
            state = STATE_HANDLE_HCI_RESET;
            transport_send_cmd(&hci_reset);
            break;
        case STATE_HANDLE_HCI_RESET:
            // check if HCI Reset was supported
            if (packet[0] == 0x0e && packet[1] == 0x04 && packet[3] == 0x03 && packet[4] == 0x0c && packet[5] == 0x00){
                log_info("HCI Reset was successful, no need for firmware upload / or not an Intel chipset");
                (*done)(0);
                break;
            }

            // Read Intel Version
            state = STATE_HANDLE_READ_VERSION_1;
            transport_send_cmd(&hci_intel_read_version, 0xff);
            break;
        case STATE_HANDLE_READ_VERSION_1:
            // detect legacy vs. new TLV mode based on Read Version response
            if ((size == sizeof(intel_version_t)) || (packet[1] != 0x037)){
                controller_mode = INTEL_CONTROLLER_TLV;
                printf("\nERROR: Intel Controller uses new TLV mode. TLV mode is not supported yet\n");
                printf("Details: https://github.com/torvalds/linux/blob/master/drivers/bluetooth/btintel.c\n\n");
                log_error("TLV mode not supported");
                (*done)(1);
                break;
            }

            // legacy mode
            intel_version =  *(intel_version_t*) hci_event_command_complete_get_return_parameters(packet);
            dump_intel_version(&intel_version);

            // fw_variant = 0x06 bootloader mode / 0x23 operational mode
            if (intel_version.fw_variant == 0x23) {
                (*done)(0);
                break;
            }

            if (intel_version.fw_variant != 0x06){
                log_error("unknown fw_variant 0x%02x", intel_version.fw_variant);
                break;
            }

            // Read Intel Secure Boot Params
            state = STATE_HANDLE_READ_SECURE_BOOT_PARAMS;
            transport_send_cmd(&hci_intel_read_secure_boot_params);
            break;
        case STATE_HANDLE_READ_SECURE_BOOT_PARAMS:
            intel_boot_params = *(intel_boot_params_t *) hci_event_command_complete_get_return_parameters(packet);
            dump_intel_boot_params(&intel_boot_params);

            reverse_bd_addr(intel_boot_params.otp_bdaddr, addr);

            // assert command complete is required
            if (intel_boot_params.limited_cce != 0) break;

            // firmware file
            intel_get_firmware_name(&intel_version, &intel_boot_params, firmware_folder_path,
                                    "sfi", fw_path, sizeof(fw_path));
            log_info("Open firmware %s", fw_path);
            printf("Firmware %s\n", fw_path);

            // open firmware file
            fw_offset = 0;
            fw_file = fopen(fw_path, "rb");
            if (!fw_file){
                log_error("can't open file %s", fw_path);
                (*done)(1);
                return;
            }

            vendor_firmware_complete_received = 0;

            // send CCS segment - offset 0
            state = STATE_SEND_PUBLIC_KEY_1;
            intel_send_fragment(0x00, 128);
            break;
        case STATE_SEND_PUBLIC_KEY_1:
            // send public key / part 1 - offset 128
            state = STATE_SEND_PUBLIC_KEY_2;
            intel_send_fragment(0x03, 128);
            break;
        case STATE_SEND_PUBLIC_KEY_2:
            // send public key / part 2 - offset 384
            state = STATE_SEND_SIGNATURE_PART_1;
            intel_send_fragment(0x03, 128);
            break;
        case STATE_SEND_SIGNATURE_PART_1:
            // skip 4 bytes
            res = fread(fw_buffer, 1, 4, fw_file);
            log_info("read res %d", (int)res);
            fw_offset += res;

            // send signature / part 1 - offset 388
            state = STATE_SEND_SIGNATURE_PART_2;
            intel_send_fragment(0x02, 128);
            break;
        case STATE_SEND_SIGNATURE_PART_2:
            // send signature / part 2 - offset 516
            state = STATE_SEND_FIRMWARE_CHUNK;
            intel_send_fragment(0x02, 128);
            break;
        case STATE_SEND_FIRMWARE_CHUNK:
            // send firmware chunks - offset 644
            // chunk len must be 4 byte aligned
            // multiple commands can be combined
            buffer_offset = 0;
            do {
                res = fread(&fw_buffer[buffer_offset], 1, 3, fw_file);
                log_info("fw_offset %6" PRId32 ", buffer_offset %" PRId32 ", read %3u -> res %" PRId32 "", (int32_t)fw_offset, (int32_t)buffer_offset, 3, (int32_t)res);
                fw_offset += res;
                if (res == 0 ){
                    // EOF
                    log_info("End of file");
                    fclose(fw_file);
                    fw_file = NULL;
                    state = STATE_HANDLE_FIRMWARE_CHUNKS_SENT;
                    break;
                }
                int param_len = fw_buffer[buffer_offset + 2];
                buffer_offset += 3;
                if (param_len){
                    res = fread(&fw_buffer[buffer_offset], 1, param_len, fw_file);
                    fw_offset     += res;
                    buffer_offset += res;
                }
            } while ((buffer_offset & 3) != 0);

            if (buffer_offset == 0) break;

            waiting_for_command_complete = 1;
            transport_send_intel_secure(0x01, fw_buffer, (uint8_t) buffer_offset);
            break;

        case STATE_HANDLE_FIRMWARE_CHUNKS_SENT:
            // expect Vendor Specific Event 0x06
            if (!vendor_firmware_complete_received) break;

            printf("Firmware upload complete\n");
            log_info("Vendor Event 0x06 - firmware complete");

            // Reset Params - constants from Windows Intel driver
            state = STATE_HANDLE_VENDOR_SPECIFIC_EVENT_02;
            transport_send_cmd(&hci_intel_reset_param, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x04, 0x00);
            break;

        case STATE_HANDLE_VENDOR_SPECIFIC_EVENT_02:
            // expect Vendor Specific Event 0x02
            if (packet[0] != 0xff) break;
            if (packet[2] != 0x02) break;

            printf("Firmware operational\n");
            log_info("Vendor Event 0x02 - firmware operational");

            // Read Intel Version
            state = STATE_HANDLE_READ_VERSION_2;
            transport_send_cmd(&hci_intel_read_version);
            break;

        case STATE_HANDLE_READ_VERSION_2:
            intel_version = *(intel_version_t*) hci_event_command_complete_get_return_parameters(packet);
            dump_intel_version(&intel_version);

            // ddc config
            intel_get_firmware_name(&intel_version, &intel_boot_params, firmware_folder_path,
                                    "ddc", fw_path, sizeof(fw_path));
            log_info("Open DDC %s", fw_path);

            // open ddc file
            fw_offset = 0;
            fw_file = fopen(fw_path, "rb");
            if (!fw_file){
                log_error("can't open file %s", fw_path);

                (*done)(1);
                return;
            }

            // load ddc
            state = STATE_SEND_DDC;

            /* fall through */

        case STATE_SEND_DDC:
            res = intel_send_ddc();
            if (res == 0) break;

            // DDC download complete
            log_info("Load DDC Complete");

            // TODO: check if we need to wait for HCI Command Complete, resp. add another state here

            // Set Intel event mask 0xfc52
            state = STATE_DONE;
            transport_send_cmd(&hci_intel_set_event_mask, 0x87, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
            break;

        case STATE_DONE:
            (*done)(0);
            break;

        default:
            break;
    }
}

static void transport_packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);
    // we also get events with packet_type ACL from the controller
    hci_dump_packet(HCI_EVENT_PACKET, 1, packet, size);
    switch (hci_event_packet_get_type(packet)){
        case HCI_EVENT_COMMAND_COMPLETE:
        case HCI_EVENT_VENDOR_SPECIFIC:
            state_machine(packet, size);
            break;
        default:
            break;
    }
}

void btstack_chipset_intel_set_firmware_path(const char * path){
    firmware_folder_path = path;
}

void btstack_chipset_intel_download_firmware(const hci_transport_t * hci_transport, void (*callback)(int result)){

    done = callback;

	transport = hci_transport;;
    transport->register_packet_handler(&transport_packet_handler);
    transport->open();

    // get started
    state = STATE_INITIAL;
    state_machine(NULL, 0);
}
