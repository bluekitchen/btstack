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
#include "btstack_tlv.h"
#include "btstack_tlv_flash_sector.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include <string.h>

// Header:
// - Magic: 'BTstack'
// - Status:
//   - bits 765432: reserved
//	 - bits 10:     epoch

// Entries
// - Tag: 32 bit
// - Len: 32 bit
// - Value: Len in bytes

#define BTSTACK_TLV_HEADER_LEN 8
static const char * btstack_tlv_header_magic = "BTstack";

// TLV Iterator

typedef struct {
	int 	 bank;
	uint32_t offset;
	uint32_t tag;
	uint32_t len;
} tlv_iterator_t;

static void btstack_tlv_flash_sector_iterator_fetch_tag_len(btstack_tlv_flash_sector_t * self, tlv_iterator_t * it){
	uint8_t entry[8];
	self->hal_flash_sector_impl->read(self->hal_flash_sector_context, it->bank, it->offset, entry, 8);
	it->tag = big_endian_read_32(entry, 0);
	it->len = big_endian_read_32(entry, 4);
}

static void btstack_tlv_flash_sector_iterator_init(btstack_tlv_flash_sector_t * self, tlv_iterator_t * it, int bank){
	memset(it, 0, sizeof(tlv_iterator_t));
	it->bank = bank;
	it->offset = BTSTACK_TLV_HEADER_LEN;
	btstack_tlv_flash_sector_iterator_fetch_tag_len(self, it);
}

static int btstack_tlv_flash_sector_iterator_has_next(btstack_tlv_flash_sector_t * self, tlv_iterator_t * it){
	if (it->tag == 0xffffffff) return 0;
	return 1;
}

static void tlv_iterator_fetch_next(btstack_tlv_flash_sector_t * self, tlv_iterator_t * it){
	it->offset += 8 + it->len;
	if (it->offset >= self->hal_flash_sector_impl->get_size(self->hal_flash_sector_context)) {
		it->tag = 0xffffffff;
		it->len = 0;
		return;
	}
	btstack_tlv_flash_sector_iterator_fetch_tag_len(self, it);
}

//

// check both banks for headers and pick the one with the higher epoch % 4
// @returns bank or -1 if something is invalid
static int btstack_tlv_flash_sector_get_latest_bank(btstack_tlv_flash_sector_t * self){
 	uint8_t header0[BTSTACK_TLV_HEADER_LEN];
 	uint8_t header1[BTSTACK_TLV_HEADER_LEN];
 	self->hal_flash_sector_impl->read(self->hal_flash_sector_context, 0, 0, &header0[0], BTSTACK_TLV_HEADER_LEN);
 	self->hal_flash_sector_impl->read(self->hal_flash_sector_context, 1, 0, &header1[0], BTSTACK_TLV_HEADER_LEN);
 	int valid0 = memcmp(header0, btstack_tlv_header_magic, BTSTACK_TLV_HEADER_LEN-1) == 0;
 	int valid1 = memcmp(header1, btstack_tlv_header_magic, BTSTACK_TLV_HEADER_LEN-1) == 0;
	if (!valid0 && !valid1) return -1;
	if ( valid0 && !valid1) return 0;
	if (!valid0 &&  valid1) return 1;
	int epoch0 = header0[BTSTACK_TLV_HEADER_LEN-1] & 0x03;
	int epoch1 = header1[BTSTACK_TLV_HEADER_LEN-1] & 0x03;
	if (epoch0 == ((epoch1 + 1) & 0x03)) return 0;
	if (epoch1 == ((epoch0 + 1) & 0x03)) return 1;
	return -1;	// invalid, must not happen
}

static void btstack_tlv_flash_sector_write_header(btstack_tlv_flash_sector_t * self, int bank, int epoch){
	uint8_t header[BTSTACK_TLV_HEADER_LEN];
	memcpy(&header[0], btstack_tlv_header_magic, BTSTACK_TLV_HEADER_LEN-1);
	header[BTSTACK_TLV_HEADER_LEN-1] = epoch;
	self->hal_flash_sector_impl->write(self->hal_flash_sector_context, bank, 0, header, BTSTACK_TLV_HEADER_LEN);
}

static void btstack_tlv_flash_sector_migrate(btstack_tlv_flash_sector_t * self){

	int next_bank = 1 - self->current_bank;

	// @TODO erase might not be needed - resp. already been performed
	self->hal_flash_sector_impl->erase(self->hal_flash_sector_context, next_bank);
	int next_write_pos = 8;

	tlv_iterator_t it;
	btstack_tlv_flash_sector_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_sector_iterator_has_next(self, &it)){
		// check if already exist in next bank
		int found = 0;
		tlv_iterator_t next_it;
		btstack_tlv_flash_sector_iterator_init(self, &next_it, next_bank);
		while (btstack_tlv_flash_sector_iterator_has_next(self, &next_it)){
			if (next_it.tag == it.tag){
				found = 1;
				break;
			}
			tlv_iterator_fetch_next(self, &next_it);
		}
		log_info("tag %x in next bank %u", it.tag, found);
		if (found) {
			tlv_iterator_fetch_next(self, &it);
			continue;
		}
	
		// finde latest value for this one
		uint32_t tag_index = 0;
		uint32_t tag_len   = 0;
		tlv_iterator_t scan_it;
		btstack_tlv_flash_sector_iterator_init(self, &scan_it, self->current_bank);
		while (btstack_tlv_flash_sector_iterator_has_next(self, &scan_it)){
			if (scan_it.tag == it.tag){
				tag_index = scan_it.offset;
				tag_len   = scan_it.len;
			}
			tlv_iterator_fetch_next(self, &scan_it);
		}
		// copy
		int bytes_to_copy = 8 + tag_len;
		log_info("migrate %u len %u -> %u", tag_index, bytes_to_copy, next_write_pos);
		uint8_t copy_buffer[32];
		while (bytes_to_copy){
			int bytes_this_iteration = btstack_min(bytes_to_copy, sizeof(copy_buffer));
			self->hal_flash_sector_impl->read(self->hal_flash_sector_context, self->current_bank, tag_index, copy_buffer, bytes_this_iteration);
			self->hal_flash_sector_impl->write(self->hal_flash_sector_context, next_bank, next_write_pos, copy_buffer, bytes_this_iteration);
			tag_index      += bytes_this_iteration;
			next_write_pos += bytes_this_iteration;
			bytes_to_copy  -= bytes_this_iteration;
		}

		tlv_iterator_fetch_next(self, &it);
	}

	// prepare new one
	uint8_t epoch_buffer;
	self->hal_flash_sector_impl->read(self->hal_flash_sector_context, self->current_bank, BTSTACK_TLV_HEADER_LEN-1, &epoch_buffer, 1);
	btstack_tlv_flash_sector_write_header(self, next_bank, (epoch_buffer + 1) & 3);
	self->current_bank = next_bank;
	self->write_offset = next_write_pos;
}

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
static int btstack_tlv_flash_sector_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){

	btstack_tlv_flash_sector_t * self = (btstack_tlv_flash_sector_t *) context;

	uint32_t tag_index = 0;
	uint32_t tag_len   = 0;
	tlv_iterator_t it;
	btstack_tlv_flash_sector_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_sector_iterator_has_next(self, &it)){
		log_info("Offset %u, tag %x", it.offset, it.tag);
		if (it.tag == tag){
			log_info("Found tag '%x' at position %u", tag, it.offset);
			tag_index = it.offset;
			tag_len   = it.len;
		}
		tlv_iterator_fetch_next(self, &it);
	}
	if (tag_index == 0) return 0;
	if (!buffer) return tag_len;
	int copy_size = btstack_min(buffer_size, tag_len);
	self->hal_flash_sector_impl->read(self->hal_flash_sector_context, self->current_bank, tag_index + 8, buffer, copy_size);
	return copy_size;
}

/**
 * Store Tag 
 * @param tag
 * @param data
 * @param data_size
 */
static void btstack_tlv_flash_sector_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){

	btstack_tlv_flash_sector_t * self = (btstack_tlv_flash_sector_t *) context;

	if (self->write_offset + 8 + data_size > self->hal_flash_sector_impl->get_size(self->hal_flash_sector_context)){
		btstack_tlv_flash_sector_migrate(self);
	}
	// prepare entry
	uint8_t entry[8];
	big_endian_store_32(entry, 0, tag);
	big_endian_store_32(entry, 4, data_size);

	log_info("write '%x', len %u at %u", tag, data_size, self->write_offset);

	// write value first
	self->hal_flash_sector_impl->write(self->hal_flash_sector_context, self->current_bank, self->write_offset + 8, data, data_size);

	// then entry
	self->hal_flash_sector_impl->write(self->hal_flash_sector_context, self->current_bank, self->write_offset, entry, sizeof(entry));

	self->write_offset += sizeof(entry) + data_size;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_flash_sector_delete_tag(void * context, uint32_t tag){
	btstack_tlv_flash_sector_store_tag(context, tag, NULL, 0);
}

static const btstack_tlv_t btstack_tlv_flash_sector = {
	/* int  (*get_tag)(..);     */ &btstack_tlv_flash_sector_get_tag,
	/* void (*store_tag)(..);   */ &btstack_tlv_flash_sector_store_tag,
	/* void (*delete_tag)(v..); */ &btstack_tlv_flash_sector_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * btstack_tlv_flash_sector_init_instance(btstack_tlv_flash_sector_t * self, const hal_flash_sector_t * hal_flash_sector_impl, void * hal_flash_sector_context){

	self->hal_flash_sector_impl    = hal_flash_sector_impl;
	self->hal_flash_sector_context = hal_flash_sector_context;

	// find current bank and erase both if none found
	int current_bank = btstack_tlv_flash_sector_get_latest_bank(self);
	log_info("found bank %d", current_bank);
	if (current_bank < 0){
		log_info("erase both banks");
		// erase both to get into stable state
		hal_flash_sector_impl->erase(hal_flash_sector_context, 0);
		hal_flash_sector_impl->erase(hal_flash_sector_context, 1);
		current_bank = 0;
		btstack_tlv_flash_sector_write_header(self, current_bank, 0);	// epoch = 0;
	}
	self->current_bank = current_bank;

	// find write offset
	tlv_iterator_t it;
	btstack_tlv_flash_sector_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_sector_iterator_has_next(self, &it)){
		tlv_iterator_fetch_next(self, &it);
	}
	self->write_offset = it.offset;
	log_info("write offset %u", self->write_offset);

	return &btstack_tlv_flash_sector;
}

