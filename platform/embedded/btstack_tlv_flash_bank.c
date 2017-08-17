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

#define __BTSTACK_FILE__ "btstack_tlv_flash_bank.c"

#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
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

static void btstack_tlv_flash_bank_iterator_fetch_tag_len(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
	uint8_t entry[8];
	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, it->bank, it->offset, entry, 8);
	it->tag = big_endian_read_32(entry, 0);
	it->len = big_endian_read_32(entry, 4);
}

static void btstack_tlv_flash_bank_iterator_init(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it, int bank){
	memset(it, 0, sizeof(tlv_iterator_t));
	it->bank = bank;
	it->offset = BTSTACK_TLV_HEADER_LEN;
	btstack_tlv_flash_bank_iterator_fetch_tag_len(self, it);
}

static int btstack_tlv_flash_bank_iterator_has_next(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
	if (it->tag == 0xffffffff) return 0;
	return 1;
}

static void tlv_iterator_fetch_next(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
	it->offset += 8 + it->len;
	if (it->offset >= self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)) {
		it->tag = 0xffffffff;
		it->len = 0;
		return;
	}
	btstack_tlv_flash_bank_iterator_fetch_tag_len(self, it);
}

//

// check both banks for headers and pick the one with the higher epoch % 4
// @returns bank or -1 if something is invalid
static int btstack_tlv_flash_bank_get_latest_bank(btstack_tlv_flash_bank_t * self){
 	uint8_t header0[BTSTACK_TLV_HEADER_LEN];
 	uint8_t header1[BTSTACK_TLV_HEADER_LEN];
 	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, 0, 0, &header0[0], BTSTACK_TLV_HEADER_LEN);
 	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, 1, 0, &header1[0], BTSTACK_TLV_HEADER_LEN);
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

static void btstack_tlv_flash_bank_write_header(btstack_tlv_flash_bank_t * self, int bank, int epoch){
	uint8_t header[BTSTACK_TLV_HEADER_LEN];
	memcpy(&header[0], btstack_tlv_header_magic, BTSTACK_TLV_HEADER_LEN-1);
	header[BTSTACK_TLV_HEADER_LEN-1] = epoch;
	self->hal_flash_bank_impl->write(self->hal_flash_bank_context, bank, 0, header, BTSTACK_TLV_HEADER_LEN);
}

/**
 * @brief Check if erased from offset
 */
static int btstack_tlv_flash_bank_test_erased(btstack_tlv_flash_bank_t * self, int bank, uint32_t offset){
	log_info("test erased: bank %u, offset %u", bank, offset);
	uint32_t size = self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context);
	uint8_t buffer[16];
	uint8_t empty16[16];
	memset(empty16, 0xff, sizeof(empty16));
	while (offset < size){
		uint32_t copy_size = (offset + sizeof(empty16) < size) ? sizeof(empty16) : (size - offset); 
		self->hal_flash_bank_impl->read(self->hal_flash_bank_context, bank, offset, buffer, copy_size);
		if (memcmp(buffer, empty16, copy_size)) {
			log_info("not erased %x - %x", offset, offset + copy_size);
			return 0;
		}
		offset += copy_size;
	}
	return 1;
}

/** 
 * @brief erase bank (only if not already erased)
 */
static void btstack_tlv_flash_bank_erase_bank(btstack_tlv_flash_bank_t * self, int bank){
	if (btstack_tlv_flash_bank_test_erased(self, bank, 0)){
		log_info("bank %u already erased", bank);
	} else {
		log_info("bank %u not empty, erase bank", bank);
		self->hal_flash_bank_impl->erase(self->hal_flash_bank_context, bank);
	}
}

static void btstack_tlv_flash_bank_migrate(btstack_tlv_flash_bank_t * self){

	int next_bank = 1 - self->current_bank;
	log_info("migrate bank %u -> bank %u", self->current_bank, next_bank);
	// erase bank (if needed)
	btstack_tlv_flash_bank_erase_bank(self, next_bank);
	int next_write_pos = 8;

	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
		// skip deleted entries
		if (it.tag) {
			uint32_t tag_len = it.len;
			uint32_t tag_index = it.offset;

			// copy
			int bytes_to_copy = 8 + tag_len;
			log_info("migrate pos %u, tag '%x' len %u -> new pos %u", tag_index, it.tag, bytes_to_copy, next_write_pos);
			uint8_t copy_buffer[32];
			while (bytes_to_copy){
				int bytes_this_iteration = btstack_min(bytes_to_copy, sizeof(copy_buffer));
				self->hal_flash_bank_impl->read(self->hal_flash_bank_context, self->current_bank, tag_index, copy_buffer, bytes_this_iteration);
				self->hal_flash_bank_impl->write(self->hal_flash_bank_context, next_bank, next_write_pos, copy_buffer, bytes_this_iteration);
				tag_index      += bytes_this_iteration;
				next_write_pos += bytes_this_iteration;
				bytes_to_copy  -= bytes_this_iteration;
			}
		}
		tlv_iterator_fetch_next(self, &it);
	}

	// prepare new one
	uint8_t epoch_buffer;
	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, self->current_bank, BTSTACK_TLV_HEADER_LEN-1, &epoch_buffer, 1);
	btstack_tlv_flash_bank_write_header(self, next_bank, (epoch_buffer + 1) & 3);
	self->current_bank = next_bank;
	self->write_offset = next_write_pos;
}

// returns 1 == ok
static int btstack_tlv_flash_bank_verify_alignment(btstack_tlv_flash_bank_t * self, uint32_t value_size){
	uint32_t aligment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
	if (value_size % aligment){
		log_error("Value size %u not a multiply of flash alignment %u", value_size, aligment);
		return 0;
	};
	return 1;
}

static void btstack_tlv_flash_bank_delete_tag_until_offset(btstack_tlv_flash_bank_t * self, uint32_t tag, uint32_t offset){
	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it) && it.offset < offset){
		if (it.tag == tag){
			log_info("Erase tag '%x' at position %u", tag, it.offset);
			// overwrite tag with invalid tag
			uint32_t zero_tag = 0;
			self->hal_flash_bank_impl->write(self->hal_flash_bank_context, self->current_bank, it.offset, (uint8_t*) &zero_tag, sizeof(zero_tag));
		}
		tlv_iterator_fetch_next(self, &it);
	}
}

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
static int btstack_tlv_flash_bank_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){

	btstack_tlv_flash_bank_t * self = (btstack_tlv_flash_bank_t *) context;

	// abort if data size not aligned with flash requirements
	if (!btstack_tlv_flash_bank_verify_alignment(self, buffer_size)) return 0;

	uint32_t tag_index = 0;
	uint32_t tag_len   = 0;
	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
		if (it.tag == tag){
			log_info("Found tag '%x' at position %u", tag, it.offset);
			tag_index = it.offset;
			tag_len   = it.len;
			break;
		}
		tlv_iterator_fetch_next(self, &it);
	}
	if (tag_index == 0) return 0;
	if (!buffer) return tag_len;
	int copy_size = btstack_min(buffer_size, tag_len);
	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, self->current_bank, tag_index + 8, buffer, copy_size);
	return copy_size;
}

/**
 * Store Tag 
 * @param tag
 * @param data
 * @param data_size
 */
static int btstack_tlv_flash_bank_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){

	btstack_tlv_flash_bank_t * self = (btstack_tlv_flash_bank_t *) context;

	// abort if data size not aligned with flash requirements
	if (!btstack_tlv_flash_bank_verify_alignment(self, data_size)) return 1;

	// trigger migration if not enough space
	if (self->write_offset + 8 + data_size > self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){
		btstack_tlv_flash_bank_migrate(self);
	}

	if (self->write_offset + 8 + data_size > self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){
		log_error("couldn't write entry, not enough space left");
		return 2;
	}

	// prepare entry
	uint8_t entry[8];
	big_endian_store_32(entry, 0, tag);
	big_endian_store_32(entry, 4, data_size);

	log_info("write '%x', len %u at %u", tag, data_size, self->write_offset);

	// write value first
	self->hal_flash_bank_impl->write(self->hal_flash_bank_context, self->current_bank, self->write_offset + 8, data, data_size);

	// then entry
	self->hal_flash_bank_impl->write(self->hal_flash_bank_context, self->current_bank, self->write_offset, entry, sizeof(entry));

	// overwrite old entries (if exists)
	btstack_tlv_flash_bank_delete_tag_until_offset(self, tag, self->write_offset);

	// done
	self->write_offset += sizeof(entry) + data_size;

	return 0;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_flash_bank_delete_tag(void * context, uint32_t tag){
	btstack_tlv_flash_bank_t * self = (btstack_tlv_flash_bank_t *) context;
	btstack_tlv_flash_bank_delete_tag_until_offset(self, tag, self->write_offset);
}

static const btstack_tlv_t btstack_tlv_flash_bank = {
	/* int  (*get_tag)(..);     */ &btstack_tlv_flash_bank_get_tag,
	/* int (*store_tag)(..);    */ &btstack_tlv_flash_bank_store_tag,
	/* void (*delete_tag)(v..); */ &btstack_tlv_flash_bank_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * btstack_tlv_flash_bank_init_instance(btstack_tlv_flash_bank_t * self, const hal_flash_bank_t * hal_flash_bank_impl, void * hal_flash_bank_context){

	self->hal_flash_bank_impl    = hal_flash_bank_impl;
	self->hal_flash_bank_context = hal_flash_bank_context;

	// try to find current bank
	self->current_bank = btstack_tlv_flash_bank_get_latest_bank(self);
	log_info("found bank %d", self->current_bank);
	if (self->current_bank >= 0){

		// find last entry and write offset
		tlv_iterator_t it;
		uint32_t last_tag = 0;
		uint32_t last_offset = 0;
		btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
		while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
			last_tag = it.tag;
			last_offset = it.offset;
			tlv_iterator_fetch_next(self, &it);
		}
		self->write_offset = it.offset;

		if (self->write_offset < self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){

			// delete older instances of last_tag
			// this handles the unlikely case where MCU did reset after new value + header was written but before delete did complete
			if (last_tag){
				btstack_tlv_flash_bank_delete_tag_until_offset(self, last_tag, last_offset);
			}

			// verify that rest of bank is empty
			// this handles the unlikely case where MCU did reset after new value was written, but not the tag
			if (!btstack_tlv_flash_bank_test_erased(self, self->current_bank, self->write_offset)){
				log_info("Flash not empty after last found tag -> migrate");
				btstack_tlv_flash_bank_migrate(self);
			} else {
				log_info("Flash clean after last found tag");
			}
		} else {
			// failure!
			self->current_bank = -1;
		}
	} 

	if (self->current_bank < 0) {
		btstack_tlv_flash_bank_erase_bank(self, 0);
		self->current_bank = 0;
		btstack_tlv_flash_bank_write_header(self, self->current_bank, 0);	// epoch = 0;
		self->write_offset = 8;
	}

	log_info("write offset %u", self->write_offset);
	return &btstack_tlv_flash_bank;
}

