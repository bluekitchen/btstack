/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "btstack_chipset_bcm.c"

/*
 *  bt_control_bcm.c
 *
 *  Adapter to use Broadcom-based chipsets with BTstack
 */


#include "btstack_config.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

#include "btstack_control.h"
#include "btstack_debug.h"
#include "btstack_chipset_bcm.h"
#include "hci.h"

#ifdef HAVE_POSIX_FILE_IO
#include "tinydir.h"
#include <ctype.h>
#endif

#ifdef _MSC_VER
// ignore deprecated warning for fopen
#pragma warning(disable : 4996)

// map strncasecmp
#define strncasecmp _strnicmp
#endif

// assert outgoing and incoming hci packet buffers can hold max hci command resp. event packet
#if HCI_OUTGOING_PACKET_BUFFER_SIZE < (HCI_CMD_HEADER_SIZE + 255)
#error "HCI_OUTGOING_PACKET_BUFFER_SIZE to small. Outgoing HCI packet buffer to small for largest HCI Command packet. Please set HCI_ACL_PAYLOAD_SIZE to 258 or higher."
#endif
#if HCI_INCOMING_PACKET_BUFFER_SIZE < (HCI_EVENT_HEADER_SIZE + 255)
#error "HCI_INCOMING_PACKET_BUFFER_SIZE to small. Incoming HCI packet buffer to small for largest HCI Event packet. Please set HCI_ACL_PAYLOAD_SIZE to 257 or higher."
#endif

static int send_download_command;
static uint32_t init_script_offset;

// Embedded == non posix systems

// actual init script provided by separate bt_firmware_image.c from WICED SDK
extern const uint8_t brcm_patchram_buf[];
extern const int     brcm_patch_ram_length;
extern const char    brcm_patch_version[];

//
// @note: Broadcom chips require higher UART clock for baud rate > 3000000 -> limit baud rate in hci.c
static void chipset_set_baudrate_command(uint32_t baudrate, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x18;
    hci_cmd_buffer[1] = 0xfc;
    hci_cmd_buffer[2] = 0x06;
    hci_cmd_buffer[3] = 0x00;
    hci_cmd_buffer[4] = 0x00;
    little_endian_store_32(hci_cmd_buffer, 5, baudrate);
}

// @note: bd addr has to be set after sending init script (it might just get re-set)
static void chipset_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x01;
    hci_cmd_buffer[1] = 0xfc;
    hci_cmd_buffer[2] = 0x06;
    reverse_bd_addr(addr, &hci_cmd_buffer[3]);
}

#ifdef HAVE_POSIX_FILE_IO

static const char * hcd_file_path;
static const char * hcd_folder_path = ".";
static FILE * hcd_file;
static char matched_file[1000];


static void chipset_init(const void * config){
    UNUSED(config);
    if (hcd_file_path){
        log_info("chipset-bcm: init file %s", hcd_file_path);
    } else {
        log_info("chipset-bcm: init folder %s", hcd_folder_path);
    }
    send_download_command = 1;
    init_script_offset = 0;
    hcd_file = NULL;
}

static const uint8_t download_command[] = {0x2e, 0xfc, 0x00};

static btstack_chipset_result_t chipset_next_command(uint8_t * hci_cmd_buffer){
    if (hcd_file == NULL){
        log_info("chipset-bcm: open file %s", hcd_file_path);
        hcd_file = fopen(hcd_file_path, "rb");
        if (hcd_file == NULL){
            log_error("chipset-bcm: can't open file %s", hcd_file_path);
            return BTSTACK_CHIPSET_NO_INIT_SCRIPT;
        }
    }

    // send download firmware command
    if (send_download_command){
        send_download_command = 0;
        memcpy(hci_cmd_buffer, download_command, sizeof(download_command));
        return BTSTACK_CHIPSET_VALID_COMMAND;
    }

    // read next command, but skip download command
    do {
        // read command
        size_t bytes_read = fread(hci_cmd_buffer, 1, 3, hcd_file);
        if (bytes_read < 3){
            if (feof(hcd_file)) {
                log_info("chipset-bcm: end of file, size %u", init_script_offset);
            } else {
                log_error("chipset-bcm: read error at %u", init_script_offset);
            }
            fclose(hcd_file);
            hcd_file = NULL;
            return BTSTACK_CHIPSET_DONE;
        }
        init_script_offset += 3;

        // read parameters
        size_t param_len = hci_cmd_buffer[2];
        if (param_len){
            bytes_read = fread(&hci_cmd_buffer[3], 1, param_len, hcd_file);
        }
        if (bytes_read < param_len){
            log_error("chipset-bcm: read error at %u", init_script_offset);
            fclose(hcd_file);
            hcd_file = NULL;
            return BTSTACK_CHIPSET_DONE;
        }
        init_script_offset += param_len;

    } while (memcmp(hci_cmd_buffer, download_command, sizeof(download_command)) == 0);
    return BTSTACK_CHIPSET_VALID_COMMAND;
}

void btstack_chipset_bcm_set_hcd_file_path(const char * path){
    hcd_file_path = path;
}

void btstack_chipset_bcm_set_hcd_folder_path(const char * path){
    hcd_folder_path = path;
}

void btstack_chipset_bcm_set_device_name(const char * device_name){
    // ignore if file path already set
    if (hcd_file_path) {
        log_error("chipset-bcm: set device name called %s although path %s already set", device_name, hcd_file_path);
        return;
    }

    // find in folder
    tinydir_dir dir = {};
    int res = tinydir_open(&dir, hcd_folder_path);
    if (res < 0){
        log_error("chipset-bcm: could not get directory for %s", hcd_folder_path);
        return;
    }
    uint16_t device_name_len = (uint16_t) strlen(device_name);
    while (dir.has_next) {
        tinydir_file file;
        tinydir_readfile(&dir, &file);
        tinydir_next(&dir);
        // starts with $device_name?
        if (strncasecmp(device_name, file.name, device_name_len) != 0) {
            continue;
        }
        // long enough to have ".hcd" suffix
        uint16_t filename_len = (uint16_t) strlen(file.name);
        if (filename_len < 5) {
            continue;
        }
        // check suffix
        if (strncasecmp(".hcd", &file.name[filename_len - 4], device_name_len) == 0) {
            btstack_strcpy(matched_file, sizeof(matched_file), hcd_folder_path);
            btstack_strcat(matched_file, sizeof(matched_file), "/");
            btstack_strcat(matched_file, sizeof(matched_file), file.name);
            hcd_file_path = matched_file;
            break;
        }
    }
    tinydir_close(&dir);
    if (hcd_file_path == NULL) {
        log_error("chipset-bcm: could not find .hcd that starts with %s at path %s", device_name, hcd_folder_path);
    }
}

#else

static void chipset_init(const void * config){
    UNUSED(config);
    log_info("chipset-bcm: init script %s, len %u", brcm_patch_version, brcm_patch_ram_length);
    init_script_offset = 0;
    send_download_command = 1;
}

static btstack_chipset_result_t chipset_next_command(uint8_t * hci_cmd_buffer){
    // no initscript
    if (brcm_patch_ram_length == 0) return BTSTACK_CHIPSET_NO_INIT_SCRIPT;

    // send download firmware command
    if (send_download_command){
        send_download_command = 0;
        hci_cmd_buffer[0] = 0x2e;
        hci_cmd_buffer[1] = 0xfc;
        hci_cmd_buffer[2] = 0x00;
        return BTSTACK_CHIPSET_VALID_COMMAND;
    }

    if (init_script_offset >= brcm_patch_ram_length) {
        return BTSTACK_CHIPSET_DONE;
    }

    int cmd_len = 3 + brcm_patchram_buf[init_script_offset+2];
    memcpy(&hci_cmd_buffer[0], &brcm_patchram_buf[init_script_offset], cmd_len); 
    init_script_offset += cmd_len;
    return BTSTACK_CHIPSET_VALID_COMMAND;     
}
#endif


static btstack_chipset_t btstack_chipset_bcm = {
    "BCM",
    chipset_init,
    chipset_next_command,
    chipset_set_baudrate_command,
    chipset_set_bd_addr_command,
};

// MARK: public API
const btstack_chipset_t * btstack_chipset_bcm_instance(void){
    return &btstack_chipset_bcm;
}

/**
 * @brief Enable init file - needed by btstack_chipset_bcm_download_firmware when using h5
 * @param enabled
 */
void btstack_chipset_bcm_enable_init_script(int enabled){
    if (enabled){
        btstack_chipset_bcm.next_command = &chipset_next_command;
    } else {
        btstack_chipset_bcm.next_command = NULL;
    }
}
