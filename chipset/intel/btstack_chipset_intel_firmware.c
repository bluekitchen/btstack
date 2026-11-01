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

#define INTEL_TLV_CNVI_TOP       0x10
#define INTEL_TLV_CNVR_TOP       0x11
#define INTEL_TLV_CNVI_BT        0x12
#define INTEL_TLV_CNVR_BT        0x13
#define INTEL_TLV_DEV_REV_ID     0x16
#define INTEL_TLV_IMAGE_TYPE     0x1c
#define INTEL_TLV_TIME_STAMP     0x1d
#define INTEL_TLV_BUILD_TYPE     0x1e
#define INTEL_TLV_BUILD_NUM      0x1f
#define INTEL_TLV_SECURE_BOOT    0x28
#define INTEL_TLV_OTP_LOCK       0x2a
#define INTEL_TLV_API_LOCK       0x2b
#define INTEL_TLV_DEBUG_LOCK     0x2c
#define INTEL_TLV_MIN_FW         0x2d
#define INTEL_TLV_LIMITED_CCE    0x2e
#define INTEL_TLV_SBE_TYPE       0x2f
#define INTEL_TLV_OTP_BDADDR     0x30
#define INTEL_TLV_GIT_SHA1       0x32
#define INTEL_TLV_FW_ID          0x50

#define INTEL_IMG_BOOTLOADER     0x01
#define INTEL_IMG_IML            0x02
#define INTEL_IMG_OPERATIONAL    0x03
#define INTEL_RSA_HEADER_LEN     644u
#define INTEL_ECDSA_HEADER_LEN   320u
#define INTEL_ECDSA_OFFSET       644u
#define INTEL_CSS_HEADER_OFFSET  8u
#define INTEL_CSS_RSA_VERSION    0x00010000u
#define INTEL_CSS_ECDSA_VERSION  0x00020000u
#define INTEL_FW_ID_MAXLEN       64u

typedef struct {
    uint32_t cnvi_top;
    uint32_t cnvr_top;
    uint32_t cnvi_bt;
    uint32_t cnvr_bt;
    uint16_t dev_rev_id;
    uint8_t img_type;
    uint16_t timestamp;
    uint8_t build_type;
    uint32_t build_num;
    uint8_t secure_boot;
    uint8_t otp_lock;
    uint8_t api_lock;
    uint8_t debug_lock;
    uint8_t min_fw_build_nn;
    uint8_t min_fw_build_cw;
    uint8_t min_fw_build_yy;
    uint8_t limited_cce;
    uint8_t sbe_type;
    uint32_t git_sha1;
    char fw_id[INTEL_FW_ID_MAXLEN];
    bd_addr_t otp_bd_addr;
} intel_version_tlv_t;

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
    STATE_TLV_SEND_ECDSA_PUBLIC_KEY = 13,
    STATE_TLV_SEND_ECDSA_SIGNATURE = 14,
    STATE_DONE = 15
} state_t;

// Vendor specific commands

static const hci_cmd_t hci_intel_read_version_tlv = {
    0xfc05, "1"
};
static const hci_cmd_t hci_intel_read_version_legacy = {
    0xfc05, ""
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
static intel_version_tlv_t intel_version_tlv;
static uint32_t             intel_boot_param;
static size_t               firmware_payload_offset;
static int                  firmware_payload_started;
static size_t               tlv_header_base;
static uint8_t              tlv_use_ecdsa;

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
static int intel_send_fragment(uint8_t fragment_type, uint8_t len);

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

static uint8_t intel_tlv_hw_platform(const intel_version_tlv_t *version){
    return (uint8_t)((version->cnvi_bt >> 8) & 0xffu);
}

static uint8_t intel_tlv_hw_variant(const intel_version_tlv_t *version){
    return (uint8_t)((version->cnvi_bt >> 16) & 0x3fu);
}

static uint16_t intel_tlv_pack_top(uint32_t top){
    uint16_t value = (uint16_t)(((top & 0x0fffu) << 4) | ((top >> 24) & 0x0fu));
    return (uint16_t)((value >> 8) | (value << 8));
}

static int intel_get_firmware_name_tlv(const intel_version_tlv_t *version, const char *folder_path,
                                       const char *suffix, char *firmware_path, size_t firmware_path_len){
    uint16_t cnvi = intel_tlv_pack_top(version->cnvi_top);
    uint16_t cnvr = intel_tlv_pack_top(version->cnvr_top);
    int len = snprintf(firmware_path, firmware_path_len, "%s/ibt-%04x-%04x.%s",
                       folder_path, cnvi, cnvr, suffix);
    return (len > 0 && (size_t)len < firmware_path_len) ? 0 : -1;
}

static int intel_parse_version_tlv(const uint8_t *data, uint16_t len, intel_version_tlv_t *version){
    uint16_t pos = 1;
    if (data == NULL || version == NULL || len < 1 || data[0] != 0) return -1;
    memset(version, 0, sizeof(*version));

    while ((uint16_t)(pos + 2) <= len){
        uint8_t type = data[pos++];
        uint8_t value_len = data[pos++];
        if ((uint16_t)(pos + value_len) > len) return -1;
        const uint8_t *value = &data[pos];

        switch (type){
            case INTEL_TLV_CNVI_TOP:    if (value_len >= 4) version->cnvi_top = little_endian_read_32(value, 0); break;
            case INTEL_TLV_CNVR_TOP:    if (value_len >= 4) version->cnvr_top = little_endian_read_32(value, 0); break;
            case INTEL_TLV_CNVI_BT:     if (value_len >= 4) version->cnvi_bt = little_endian_read_32(value, 0); break;
            case INTEL_TLV_CNVR_BT:     if (value_len >= 4) version->cnvr_bt = little_endian_read_32(value, 0); break;
            case INTEL_TLV_DEV_REV_ID:  if (value_len >= 2) version->dev_rev_id = little_endian_read_16(value, 0); break;
            case INTEL_TLV_IMAGE_TYPE:  if (value_len >= 1) version->img_type = value[0]; break;
            case INTEL_TLV_TIME_STAMP:  if (value_len >= 2) version->timestamp = little_endian_read_16(value, 0); break;
            case INTEL_TLV_BUILD_TYPE:  if (value_len >= 1) version->build_type = value[0]; break;
            case INTEL_TLV_BUILD_NUM:   if (value_len >= 4) version->build_num = little_endian_read_32(value, 0); break;
            case INTEL_TLV_SECURE_BOOT: if (value_len >= 1) version->secure_boot = value[0]; break;
            case INTEL_TLV_OTP_LOCK:    if (value_len >= 1) version->otp_lock = value[0]; break;
            case INTEL_TLV_API_LOCK:    if (value_len >= 1) version->api_lock = value[0]; break;
            case INTEL_TLV_DEBUG_LOCK:  if (value_len >= 1) version->debug_lock = value[0]; break;
            case INTEL_TLV_MIN_FW:
                if (value_len >= 3){
                    version->min_fw_build_nn = value[0];
                    version->min_fw_build_cw = value[1];
                    version->min_fw_build_yy = value[2];
                }
                break;
            case INTEL_TLV_LIMITED_CCE: if (value_len >= 1) version->limited_cce = value[0]; break;
            case INTEL_TLV_SBE_TYPE:    if (value_len >= 1) version->sbe_type = value[0]; break;
            case INTEL_TLV_GIT_SHA1:    if (value_len >= 4) version->git_sha1 = little_endian_read_32(value, 0); break;
            case INTEL_TLV_FW_ID: {
                size_t copy_len = value_len;
                if (copy_len >= sizeof(version->fw_id)) copy_len = sizeof(version->fw_id) - 1;
                memcpy(version->fw_id, value, copy_len);
                version->fw_id[copy_len] = 0;
                break;
            }
            case INTEL_TLV_OTP_BDADDR:
                if (value_len >= sizeof(bd_addr_t)) memcpy(version->otp_bd_addr, value, sizeof(bd_addr_t));
                break;
            default:
                break;
        }
        pos = (uint16_t)(pos + value_len);
    }
    return pos == len ? 0 : -1;
}

static void dump_intel_version_tlv(const intel_version_tlv_t *version){
    log_info("Intel TLV: CNVi top 0x%08" PRIx32 ", CNVr top 0x%08" PRIx32, version->cnvi_top, version->cnvr_top);
    log_info("Intel TLV: CNVi BT 0x%08" PRIx32 ", CNVr BT 0x%08" PRIx32, version->cnvi_bt, version->cnvr_bt);
    log_info("Intel TLV: platform 0x%02x variant 0x%02x rev 0x%04x image 0x%02x",
             intel_tlv_hw_platform(version), intel_tlv_hw_variant(version), version->dev_rev_id, version->img_type);
    log_info("Intel TLV: secure boot %u SBE %u limited CCE %u build 0x%08" PRIx32,
             version->secure_boot, version->sbe_type, version->limited_cce, version->build_num);
    if (version->fw_id[0]) log_info("Intel TLV: FW ID %s", version->fw_id);
}

static int intel_read_css_version(FILE *file, size_t base, uint32_t *version){
    uint8_t value[4];
    if (file == NULL || version == NULL) return -1;
    if (fseek(file, (long)(base + INTEL_CSS_HEADER_OFFSET), SEEK_SET) != 0) return -1;
    if (fread(value, 1, sizeof(value), file) != sizeof(value)) return -1;
    *version = little_endian_read_32(value, 0);
    return 0;
}

static int intel_seek_and_send_fragment(uint8_t fragment_type, size_t offset, uint8_t len){
    if (fw_file == NULL || fseek(fw_file, (long)offset, SEEK_SET) != 0) return -1;
    fw_offset = offset;
    return intel_send_fragment(fragment_type, len);
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
    res = fread(fw_buffer, 1, 1, fw_file);
    if (res == 0) return -1;
    if (res != 1) return -2;
    uint8_t len = fw_buffer[0];
    fw_offset += 1;

    res = fread(&fw_buffer[1], 1, len, fw_file);
    if (res != len) return -2;
    fw_offset += res;
    return transport_send_intel_ddc(fw_buffer, (uint8_t)(1 + len));
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
            // This loader is used only by the Intel-specific port. Always query Intel
            // version after reset so a bootloader that accepts HCI Reset is not mistaken
            // for an already operational controller.
            // Parameter 0xff requests the TLV format on modern controllers.
            state = STATE_HANDLE_READ_VERSION_1;
            transport_send_cmd(&hci_intel_read_version_tlv, 0xff);
            break;

        case STATE_HANDLE_READ_VERSION_1: {
            const uint8_t *return_params = hci_event_command_complete_get_return_parameters(packet);
            uint16_t return_len = packet[1] >= 3 ? (uint16_t)(packet[1] - 3) : 0;

            if (return_len != sizeof(intel_version_t)){
                uint32_t rsa_version = 0;
                uint32_t ecdsa_version = 0;
                uint8_t hw_platform;
                uint8_t hw_variant;

                controller_mode = INTEL_CONTROLLER_TLV;
                if (intel_parse_version_tlv(return_params, return_len, &intel_version_tlv) != 0){
                    log_error("Invalid Intel TLV Read Version response");
                    (*done)(1);
                    break;
                }
                dump_intel_version_tlv(&intel_version_tlv);
                hw_platform = intel_tlv_hw_platform(&intel_version_tlv);
                hw_variant = intel_tlv_hw_variant(&intel_version_tlv);

                if (hw_platform != 0x37){
                    log_error("Unsupported Intel TLV platform 0x%02x", hw_platform);
                    (*done)(1);
                    break;
                }
                if (intel_version_tlv.img_type == INTEL_IMG_OPERATIONAL){
                    printf("Intel TLV firmware is already operational\n");
                    (*done)(0);
                    break;
                }
                if (intel_version_tlv.img_type != INTEL_IMG_BOOTLOADER){
                    log_error("Unsupported Intel image type 0x%02x", intel_version_tlv.img_type);
                    (*done)(1);
                    break;
                }
                if (intel_version_tlv.limited_cce != 0){
                    log_error("Intel controller reports limited CCE mode %u", intel_version_tlv.limited_cce);
                    (*done)(1);
                    break;
                }
                if (intel_version_tlv.sbe_type > 1){
                    log_error("Unsupported Intel secure boot engine type %u", intel_version_tlv.sbe_type);
                    (*done)(1);
                    break;
                }

                if (intel_get_firmware_name_tlv(&intel_version_tlv, firmware_folder_path,
                                                "sfi", fw_path, sizeof(fw_path)) != 0){
                    log_error("Could not construct Intel TLV firmware filename");
                    (*done)(1);
                    break;
                }
                printf("Intel TLV platform 0x%02x variant 0x%02x\n", hw_platform, hw_variant);
                printf("Firmware %s\n", fw_path);
                fw_file = fopen(fw_path, "rb");
                if (fw_file == NULL){
                    log_error("can't open file %s", fw_path);
                    (*done)(1);
                    break;
                }

                if (intel_read_css_version(fw_file, 0, &rsa_version) != 0 || rsa_version != INTEL_CSS_RSA_VERSION){
                    log_error("Intel SFI does not contain the expected RSA CSS header");
                    fclose(fw_file);
                    fw_file = NULL;
                    (*done)(1);
                    break;
                }

                // AX211-generation firmware carries an ECDSA CSS header after the legacy RSA header.
                if (hw_variant >= 0x17){
                    if (intel_read_css_version(fw_file, INTEL_ECDSA_OFFSET, &ecdsa_version) != 0 ||
                        ecdsa_version != INTEL_CSS_ECDSA_VERSION){
                        log_error("Intel SFI does not contain the expected ECDSA CSS header");
                        fclose(fw_file);
                        fw_file = NULL;
                        (*done)(1);
                        break;
                    }
                    firmware_payload_offset = INTEL_RSA_HEADER_LEN + INTEL_ECDSA_HEADER_LEN;
                } else {
                    firmware_payload_offset = INTEL_RSA_HEADER_LEN;
                }

                intel_boot_param = 0;
                firmware_payload_started = 0;
                vendor_firmware_complete_received = 0;
                tlv_use_ecdsa = (uint8_t)(intel_version_tlv.sbe_type == 1 && hw_variant >= 0x17);
                tlv_header_base = tlv_use_ecdsa ? INTEL_ECDSA_OFFSET : 0;

                if (tlv_use_ecdsa){
                    state = STATE_TLV_SEND_ECDSA_PUBLIC_KEY;
                    if (intel_seek_and_send_fragment(0x00, tlv_header_base, 128) != 0){
                        log_error("Failed to send Intel ECDSA init fragment");
                        (*done)(1);
                    }
                } else {
                    state = STATE_SEND_PUBLIC_KEY_1;
                    if (intel_seek_and_send_fragment(0x00, 0, 128) != 0){
                        log_error("Failed to send Intel RSA init fragment");
                        (*done)(1);
                    }
                }
                break;
            }

            // legacy mode
            controller_mode = INTEL_CONTROLLER_LEGACY;
            intel_version = *(intel_version_t *)return_params;
            dump_intel_version(&intel_version);

            // fw_variant = 0x06 bootloader mode / 0x23 operational mode
            if (intel_version.fw_variant == 0x23) {
                (*done)(0);
                break;
            }

            if (intel_version.fw_variant != 0x06){
                log_error("unknown fw_variant 0x%02x", intel_version.fw_variant);
                (*done)(1);
                break;
            }

            // Read Intel Secure Boot Params
            state = STATE_HANDLE_READ_SECURE_BOOT_PARAMS;
            transport_send_cmd(&hci_intel_read_secure_boot_params);
            break;
        }

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
            state = STATE_SEND_PUBLIC_KEY_2;
            intel_send_fragment(0x03, 128);
            break;

        case STATE_SEND_PUBLIC_KEY_2:
            state = STATE_SEND_SIGNATURE_PART_1;
            intel_send_fragment(0x03, 128);
            break;

        case STATE_SEND_SIGNATURE_PART_1:
            // RSA CSS places a four-byte gap between the public key and signature.
            res = fread(fw_buffer, 1, 4, fw_file);
            if (res != 4){
                log_error("Short read before Intel RSA signature");
                (*done)(1);
                break;
            }
            fw_offset += res;
            state = STATE_SEND_SIGNATURE_PART_2;
            intel_send_fragment(0x02, 128);
            break;

        case STATE_SEND_SIGNATURE_PART_2:
            state = STATE_SEND_FIRMWARE_CHUNK;
            intel_send_fragment(0x02, 128);
            break;

        case STATE_TLV_SEND_ECDSA_PUBLIC_KEY:
            state = STATE_TLV_SEND_ECDSA_SIGNATURE;
            intel_send_fragment(0x03, 96);
            break;

        case STATE_TLV_SEND_ECDSA_SIGNATURE:
            state = STATE_SEND_FIRMWARE_CHUNK;
            intel_send_fragment(0x02, 96);
            break;

        case STATE_SEND_FIRMWARE_CHUNK:
            if (controller_mode == INTEL_CONTROLLER_TLV && !firmware_payload_started){
                if (fseek(fw_file, (long)firmware_payload_offset, SEEK_SET) != 0){
                    log_error("Failed to seek to Intel TLV firmware payload offset %u", (unsigned)firmware_payload_offset);
                    (*done)(1);
                    break;
                }
                fw_offset = firmware_payload_offset;
                firmware_payload_started = 1;
                printf("Intel firmware payload offset: %u\n", (unsigned)firmware_payload_offset);
            }

            // Secure Send accepts at most 252 firmware bytes per transfer. Batch complete
            // HCI commands and stop only on a four-byte aligned boundary.
            buffer_offset = 0;
            do {
                size_t command_start = buffer_offset;
                long file_command_start = ftell(fw_file);
                res = fread(&fw_buffer[buffer_offset], 1, 3, fw_file);
                if (res == 0){
                    log_info("End of file");
                    fclose(fw_file);
                    fw_file = NULL;
                    state = STATE_HANDLE_FIRMWARE_CHUNKS_SENT;
                    break;
                }
                if (res != 3){
                    log_error("Truncated Intel firmware HCI command header");
                    (*done)(1);
                    break;
                }
                fw_offset += 3;

                uint16_t opcode = little_endian_read_16(&fw_buffer[command_start], 0);
                uint8_t param_len = fw_buffer[command_start + 2];
                size_t command_len = (size_t)3 + param_len;

                if (command_start + command_len > 252){
                    if (command_start != 0 && (command_start & 3u) == 0){
                        if (fseek(fw_file, file_command_start, SEEK_SET) != 0){
                            log_error("Failed to rewind Intel firmware command");
                            (*done)(1);
                            break;
                        }
                        fw_offset -= 3;
                        break;
                    }
                    log_error("Intel firmware command cannot fit secure fragment (%u bytes)", (unsigned)command_len);
                    (*done)(1);
                    break;
                }

                buffer_offset += 3;
                if (param_len){
                    res = fread(&fw_buffer[buffer_offset], 1, param_len, fw_file);
                    if (res != param_len){
                        log_error("Truncated Intel firmware HCI command payload");
                        (*done)(1);
                        break;
                    }
                    fw_offset += res;
                    buffer_offset += res;
                }

                if (opcode == 0xfc0e && param_len >= 4){
                    intel_boot_param = little_endian_read_32(&fw_buffer[command_start + 3], 0);
                    printf("Intel boot address: 0x%08" PRIx32 "\n", intel_boot_param);
                }
            } while ((buffer_offset & 3u) != 0);

            if (buffer_offset == 0) break;
            if ((buffer_offset & 3u) != 0){
                log_error("Intel secure firmware fragment is not four-byte aligned (%u)", (unsigned)buffer_offset);
                (*done)(1);
                break;
            }

            waiting_for_command_complete = 1;
            transport_send_intel_secure(0x01, fw_buffer, (uint8_t)buffer_offset);
            break;

        case STATE_HANDLE_FIRMWARE_CHUNKS_SENT:
            // expect Vendor Specific Event 0x06
            if (!vendor_firmware_complete_received) break;

            printf("Firmware upload complete\n");
            log_info("Vendor Event 0x06 - firmware complete");

            state = STATE_HANDLE_VENDOR_SPECIFIC_EVENT_02;
            if (controller_mode == INTEL_CONTROLLER_TLV){
                if (intel_boot_param == 0){
                    log_error("Intel TLV firmware did not provide a boot address");
                    (*done)(1);
                    break;
                }
                // Modern Intel reset: reset_type=0, patch_enable=1, ddc_reload=0,
                // boot_option=1, followed by the firmware boot address in little endian.
                transport_send_cmd(&hci_intel_reset_param,
                                   0x00, 0x01, 0x00, 0x01,
                                   (uint8_t)(intel_boot_param & 0xffu),
                                   (uint8_t)((intel_boot_param >> 8) & 0xffu),
                                   (uint8_t)((intel_boot_param >> 16) & 0xffu),
                                   (uint8_t)((intel_boot_param >> 24) & 0xffu));
            } else {
                // Legacy constants retained for 8260/8265-era controllers.
                transport_send_cmd(&hci_intel_reset_param, 0x00, 0x00, 0x00, 0x01, 0x00, 0x08, 0x04, 0x00);
            }
            break;

        case STATE_HANDLE_VENDOR_SPECIFIC_EVENT_02:
            // expect Vendor Specific Event 0x02
            if (packet[0] != 0xff) break;
            if (packet[2] != 0x02) break;

            printf("Firmware operational\n");
            log_info("Vendor Event 0x02 - firmware operational");

            state = STATE_HANDLE_READ_VERSION_2;
            if (controller_mode == INTEL_CONTROLLER_TLV){
                transport_send_cmd(&hci_intel_read_version_tlv, 0xff);
            } else {
                transport_send_cmd(&hci_intel_read_version_legacy);
            }
            break;

        case STATE_HANDLE_READ_VERSION_2: {
            if (controller_mode == INTEL_CONTROLLER_TLV){
                const uint8_t *return_params = hci_event_command_complete_get_return_parameters(packet);
                uint16_t return_len = packet[1] >= 3 ? (uint16_t)(packet[1] - 3) : 0;
                intel_version_tlv_t operational_version;

                if (intel_parse_version_tlv(return_params, return_len, &operational_version) == 0){
                    intel_version_tlv = operational_version;
                    dump_intel_version_tlv(&intel_version_tlv);
                } else {
                    log_info("Could not parse post-boot Intel TLV version; retaining bootloader identity");
                }

                if (intel_get_firmware_name_tlv(&intel_version_tlv, firmware_folder_path,
                                                "ddc", fw_path, sizeof(fw_path)) != 0){
                    log_error("Could not construct Intel TLV DDC filename");
                    (*done)(1);
                    break;
                }
            } else {
                intel_version = *(intel_version_t *)hci_event_command_complete_get_return_parameters(packet);
                dump_intel_version(&intel_version);
                intel_get_firmware_name(&intel_version, &intel_boot_params, firmware_folder_path,
                                        "ddc", fw_path, sizeof(fw_path));
            }

            log_info("Open DDC %s", fw_path);
            fw_offset = 0;
            fw_file = fopen(fw_path, "rb");
            if (!fw_file){
                if (controller_mode == INTEL_CONTROLLER_TLV){
                    log_info("Intel TLV DDC file not found; continuing without DDC: %s", fw_path);
                    state = STATE_DONE;
                    transport_send_cmd(&hci_intel_set_event_mask, 0x87, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
                    break;
                }
                log_error("can't open file %s", fw_path);
                (*done)(1);
                break;
            }

            state = STATE_SEND_DDC;
            res = intel_send_ddc();
            if (res == 0) break;
            if (res == -2){
                log_error("Malformed Intel DDC file");
                (*done)(1);
                break;
            }
            fclose(fw_file);
            fw_file = NULL;
            log_info("Load DDC Complete");
            state = STATE_DONE;
            transport_send_cmd(&hci_intel_set_event_mask, 0x87, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
            break;
        }

        case STATE_SEND_DDC:
            res = intel_send_ddc();
            if (res == 0) break;
            if (res == -2){
                log_error("Malformed Intel DDC file");
                (*done)(1);
                break;
            }
            if (fw_file){
                fclose(fw_file);
                fw_file = NULL;
            }
            log_info("Load DDC Complete");
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

    transport = hci_transport;
    transport->register_packet_handler(&transport_packet_handler);
    if (transport->open() != 0){
        log_error("Could not open Intel HCI transport");
        (*done)(1);
        return;
    }

    // get started
    state = STATE_INITIAL;
    state_machine(NULL, 0);
}
