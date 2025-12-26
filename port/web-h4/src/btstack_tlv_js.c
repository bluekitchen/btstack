/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_tlv_js.c"

#include "btstack_tlv_js.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>

#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "btstack_debug.h"

#include <emscripten/emscripten.h>

#include "btstack_util.h"

// Create "TAG-XXXXXXXX" in JavaScript
EM_JS(const char *, makeTag, (uint32_t tag), {
	// Ensure value is treated as unsigned 32-bit
	const uint32 = tag >>> 0;
	// Convert to uppercase hex and pad to 8 characters
	const hex = uint32.toString(16).toUpperCase().padStart(8, '0');
	return `TAG-${hex}`;
});

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @return size of value
 */
static int btstack_tlv_js_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){
    UNUSED(context);
	int size = EM_ASM_INT({
		tag_string = makeTag($0);
		item_string = localStorage.getItem(tag_string);
		if (item_string) {
			item = JSON.parse(item_string);
			console.log(item);
			size = item.length;
			console.log(size);
			if ($2 >= size) {
				HEAPU8.set(item, size);
				return size;
			}
		}
		return 0;
	}, tag, buffer, buffer_size);
	return size;
}

/**
 * Store Tag
 * @param tag
 * @param data
 * @param data_size
 */
static int btstack_tlv_js_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){
    UNUSED(context);
	EM_ASM({
		tag_string = makeTag($0);
		item = new Uint8Array(HEAPU8.buffer, $1, $2);
		localStorage.setItem(tag_string, JSON.stringify(Array.from(item)));
		btstack_tlv_js_updated();
	}, tag, data, data_size);
    return 0;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_js_delete_tag(void * context, uint32_t tag){
    UNUSED(context);
	EM_ASM({
		tag_string = makeTag($0);
		localStorage.removeItem(tag_string);
		btstack_tlv_js_updated();
	}, tag);
}

static const btstack_tlv_t btstack_tlv_js = {
	/* int  (*get_tag)(..);     */ &btstack_tlv_js_get_tag,
	/* int (*store_tag)(..);    */ &btstack_tlv_js_store_tag,
	/* void (*delete_tag)(v..); */ &btstack_tlv_js_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * btstack_tlv_js_init_instance(void){
	return &btstack_tlv_js;
}


