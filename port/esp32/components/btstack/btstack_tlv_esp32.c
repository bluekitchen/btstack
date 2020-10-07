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
 */

#define BTSTACK_FILE__ "btstack_tlv_esp32.c"

#include "btstack_tlv.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include <string.h>

static nvs_handle the_nvs_handle;
static int nvs_active;

// @param buffer char array of size 9
static void key_for_tag(uint32_t tag, char * key_buffer){
	int i;
	for (i=0;i<8;i++){
		key_buffer[i] = char_for_nibble( (tag >> (4*(7-i))) & 0x0f);
	}
	key_buffer[i] = 0;
}

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
static int btstack_tlv_esp32_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){
	if (!nvs_active) return 0;
	char key_buffer[9];
	key_for_tag(tag, key_buffer);
	log_debug("read tag %s", key_buffer);
	size_t size = 0;
    esp_err_t err = nvs_get_blob(the_nvs_handle, key_buffer, NULL, &size);
    switch (err) {
        case ESP_OK:
        	if (size > buffer_size){
        		log_error("buffer_size %u < value size %u", buffer_size, size);
        		return 0;
        	}
        	nvs_get_blob(the_nvs_handle, key_buffer, buffer, &size);
            return size;
        case ESP_ERR_NVS_NOT_FOUND:
        	break;
        default :
            log_error("Error (0x%04x) reading %s!\n", err, key_buffer);
            break;
    }
	return 0;
}

/**
 * Store Tag 
 * @param tag
 * @param data
 * @param data_size
 */
static int btstack_tlv_esp32_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){
	if (!nvs_active) return 0;
	char key_buffer[9];
	key_for_tag(tag, key_buffer);
	log_info("store tag %s", key_buffer);
	esp_err_t err = nvs_set_blob(the_nvs_handle, key_buffer, data, data_size);
	if (err != ESP_OK){
        log_error("Error (0x%04x) nvs_set_blob %s!", err, key_buffer);
        return 1;
    }
	err = nvs_commit(the_nvs_handle);
	if (err != ESP_OK){
        log_error("Error (0x%04x) nvs_commit %s!", err, key_buffer);
        return 1;
    }
    return 0;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_esp32_delete_tag(void * context, uint32_t tag){
	if (!nvs_active) return;
	char key_buffer[9];
	key_for_tag(tag, key_buffer);
	log_info("delete tag %s", key_buffer);
	esp_err_t err = nvs_erase_key(the_nvs_handle, key_buffer);
    switch (err) {
        case ESP_OK:
        case ESP_ERR_NVS_NOT_FOUND:
        	break;
        	break;
        default :
            log_error("Error (0x%04x) deleting reading %s!\n", err, key_buffer);
            break;
    }
}

static const btstack_tlv_t btstack_tlv_esp32 = {
	/* int  (*get_tag)(..);     */ &btstack_tlv_esp32_get_tag,
	/* int (*store_tag)(..);    */ &btstack_tlv_esp32_store_tag,
	/* void (*delete_tag)(v..); */ &btstack_tlv_esp32_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * btstack_tlv_esp32_get_instance(void){
	// Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	    log_info("Error (0x%04x) init flash", err);
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    err = nvs_open("BTstack", NVS_READWRITE, &the_nvs_handle);
    if (err == ESP_OK) {
		nvs_active = 1;
    } else {
	    log_info("Error (0x%04x) open flash 'BTstack' section", err);
        nvs_active = 0;
	}
	return &btstack_tlv_esp32;
}

