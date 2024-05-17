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
 */

#define BTSTACK_FILE__ "btstack_tlv_flash_bank.c"

#include "btstack_tlv.h"
#include "btstack_tlv_flash_bank.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_debug.h"

#include <string.h>
#include <inttypes.h>

// Header:
// - Magic: 'BTstack'
// - Status:
//   - bits 765432: reserved
//	 - bits 10:     epoch

// Entries
// - Tag: 32 bit
// - Len: 32 bit
// - if ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD is used
//    - padding to align delete field
//    - Delete: 32 bit
//    - padding to align value field
// - Value: Len in bytes

// Alignment
// Tag is aligned to the alignment from hal_flash_bank_t
// If ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD is used, both Delete and Value are aligned, too

// ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD 
// 
// Most Flash implementations allow to:
// - erase sector -> all values are 0xff
// - write value (1s -> 0s)
// - overwrite value with zero (remaining 1s -> 0s)
//
// We use the ability to overwrite a value with zeros to mark deleted entries (by writing zero into the tag field).
// Some targets, E.g. Kinetix K64F, do not allow for that.
//
// With ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD an extra field is reserved to indicate a deleted tag, while keeping main logic
//
// With ENABLE_TLV_FLASH_WRITE_ONCE, tags are never marked as deleted. Instead, an emtpy tag will be written instead.
//     Also, lookup and migrate requires to always search until the end of the valid bank

#if defined (ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD) && defined (ENABLE_TLV_FLASH_WRITE_ONCE)
#error "Please define either ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD or ENABLE_TLV_FLASH_WRITE_ONCE"
#endif

#define BTSTACK_TLV_BANK_HEADER_LEN  8
#define BTSTACK_TLV_ENTRY_HEADER_LEN 8

#ifndef BTSTACK_FLASH_ALIGNMENT_MAX
#define BTSTACK_FLASH_ALIGNMENT_MAX 16
#endif

static const char * btstack_tlv_header_magic = "BTstack";

// TLV Iterator
typedef struct {
	int 	 bank;
	uint32_t offset;
    uint32_t size;
	uint32_t tag;
	uint32_t len;
} tlv_iterator_t;

static uint32_t btstack_tlv_flash_bank_align_size(btstack_tlv_flash_bank_t * self, uint32_t size){
	uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
	return (size + alignment - 1) & ~(alignment - 1);
}

static uint32_t btstack_tlv_flash_bank_aligned_entry_size(btstack_tlv_flash_bank_t * self, uint32_t size) {
#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
    // entry header and delete fields are already padded
    return self->entry_header_len + self->delete_tag_len + btstack_tlv_flash_bank_align_size(self, size);
#else
    // otherwise, data starts right after entry header
    return btstack_tlv_flash_bank_align_size(self, self->entry_header_len + size);
#endif
}

// support unaligned flash read/writes
// strategy: increase size to meet alignment, perform unaligned read/write of last chunk with helper buffer

static void btstack_tlv_flash_bank_read(btstack_tlv_flash_bank_t * self, int bank, uint32_t offset, uint8_t * buffer, uint32_t size){

	// read main data
	uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
	uint32_t lower_bits = size & (alignment - 1);
	uint32_t size_aligned = size - lower_bits;
	if (size_aligned){
		self->hal_flash_bank_impl->read(self->hal_flash_bank_context, bank, offset, buffer, size_aligned);
		buffer += size_aligned;
		offset += size_aligned;
		size   -= size_aligned;
	}

	// read last part
	if (size == 0) return;
	uint8_t alignment_block[BTSTACK_FLASH_ALIGNMENT_MAX];
	self->hal_flash_bank_impl->read(self->hal_flash_bank_context, bank, offset, alignment_block, alignment);
	memcpy(buffer, alignment_block, size);
}

static void btstack_tlv_flash_bank_write(btstack_tlv_flash_bank_t * self, int bank, uint32_t offset, const uint8_t * buffer, uint32_t size){

	// write main data
	uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
	uint32_t lower_bits = size & (alignment - 1);
	uint32_t size_aligned = size - lower_bits;
	if (size_aligned){
		self->hal_flash_bank_impl->write(self->hal_flash_bank_context, bank, offset, buffer, size_aligned);
		buffer += size_aligned;
		offset += size_aligned;
		size   -= size_aligned;
	}

	// write last part
	if (size == 0) return;
	uint8_t alignment_block[BTSTACK_FLASH_ALIGNMENT_MAX];
	memset(alignment_block, 0xff, alignment);
	memcpy(alignment_block, buffer, lower_bits);
	self->hal_flash_bank_impl->write(self->hal_flash_bank_context, bank, offset, alignment_block, alignment);
}


// iterator

static void btstack_tlv_flash_bank_iterator_fetch_tag_len(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
    // abort if header doesn't fit into remaining space
    if (it->offset + self->entry_header_len + self->delete_tag_len >= self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){
        it->tag = 0xffffffff;
        return;
    }

	uint8_t entry[BTSTACK_TLV_ENTRY_HEADER_LEN];
	btstack_tlv_flash_bank_read(self, it->bank, it->offset, entry, BTSTACK_TLV_ENTRY_HEADER_LEN);
	it->tag = big_endian_read_32(entry, 0);
	it->len = big_endian_read_32(entry, 4);

#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
	// clear tag, if delete field is set
	uint32_t delete_tag;
	btstack_tlv_flash_bank_read(self, it->bank, it->offset + self->entry_header_len, (uint8_t *) &delete_tag, 4);
	if (delete_tag == 0){
		it->tag = 0;
	}
#endif
}

static void btstack_tlv_flash_bank_iterator_init(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it, int bank){
	memset(it, 0, sizeof(tlv_iterator_t));
	it->bank = bank;
	it->offset = btstack_tlv_flash_bank_align_size(self, BTSTACK_TLV_BANK_HEADER_LEN);
    it->size = self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context);
	btstack_tlv_flash_bank_iterator_fetch_tag_len(self, it);
}

static bool btstack_tlv_flash_bank_iterator_has_next(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
	UNUSED(self);
	return it->tag != 0xffffffff;
}

static void tlv_iterator_fetch_next(btstack_tlv_flash_bank_t * self, tlv_iterator_t * it){
    it->offset += btstack_tlv_flash_bank_aligned_entry_size(self, it->len);

	if (it->offset >= it->size) {
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
 	uint8_t header0[BTSTACK_TLV_BANK_HEADER_LEN];
 	uint8_t header1[BTSTACK_TLV_BANK_HEADER_LEN];
 	btstack_tlv_flash_bank_read(self, 0, 0, &header0[0], BTSTACK_TLV_BANK_HEADER_LEN);
 	btstack_tlv_flash_bank_read(self, 1, 0, &header1[0], BTSTACK_TLV_BANK_HEADER_LEN);
 	int valid0 = memcmp(header0, btstack_tlv_header_magic, BTSTACK_TLV_BANK_HEADER_LEN-1) == 0;
 	int valid1 = memcmp(header1, btstack_tlv_header_magic, BTSTACK_TLV_BANK_HEADER_LEN-1) == 0;
	if (!valid0 && !valid1) return -1;
	if ( valid0 && !valid1) return 0;
	if (!valid0 &&  valid1) return 1;
	int epoch0 = header0[BTSTACK_TLV_BANK_HEADER_LEN-1] & 0x03;
	int epoch1 = header1[BTSTACK_TLV_BANK_HEADER_LEN-1] & 0x03;
	if (epoch0 == ((epoch1 + 1) & 0x03)) return 0;
	if (epoch1 == ((epoch0 + 1) & 0x03)) return 1;
	return -1;	// invalid, must not happen
}

static void btstack_tlv_flash_bank_write_header(btstack_tlv_flash_bank_t * self, int bank, int epoch){
	uint8_t header[BTSTACK_TLV_BANK_HEADER_LEN];
	memcpy(&header[0], btstack_tlv_header_magic, BTSTACK_TLV_BANK_HEADER_LEN-1);
	header[BTSTACK_TLV_BANK_HEADER_LEN-1] = epoch;
	btstack_tlv_flash_bank_write(self, bank, 0, header, BTSTACK_TLV_BANK_HEADER_LEN);
}

/**
 * @brief Check if erased from offset
 */
static int btstack_tlv_flash_bank_test_erased(btstack_tlv_flash_bank_t * self, int bank, uint32_t offset){
	log_info("test erased: bank %u, offset %u", bank, (unsigned int) offset);
	uint32_t size = self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context);
	uint8_t buffer[16];
	uint8_t empty16[16];
	memset(empty16, 0xff, sizeof(empty16));
	while (offset < size){
		uint32_t copy_size = (offset + sizeof(empty16) < size) ? sizeof(empty16) : (size - offset); 
		btstack_tlv_flash_bank_read(self, bank, offset, buffer, copy_size);
		if (memcmp(buffer, empty16, copy_size) != 0) {
			log_info("not erased %x - %x", (unsigned int) offset, (unsigned int) (offset + copy_size));
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
	uint32_t next_write_pos = btstack_tlv_flash_bank_align_size (self, BTSTACK_TLV_BANK_HEADER_LEN);;

	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
		// skip deleted entries
		if (it.tag) {
			uint32_t tag_len = it.len;
			uint32_t tag_index = it.offset;

            bool tag_valid = true;

#ifdef ENABLE_TLV_FLASH_WRITE_ONCE
            // search until end for newer entry of same tag
            tlv_iterator_t it2;
            memcpy(&it2, &it, sizeof(tlv_iterator_t));
            while (btstack_tlv_flash_bank_iterator_has_next(self, &it2)){
                if ((it2.offset != it.offset) && (it2.tag == it.tag)){
                    tag_valid = false;
                    break;
                }
                tlv_iterator_fetch_next(self, &it2);
            }
            if (tag_valid == false){
			    log_info("skip pos %u, tag '%x' as newer entry found at %u", (unsigned int) tag_index, (unsigned int) it.tag,
                    (unsigned int) it2.offset);
            }
#endif

            if (tag_valid) {

                log_info("migrate pos %u, tag '%x' len %u -> new pos %u",
                         (unsigned int) tag_index, (unsigned int) it.tag, (unsigned int) tag_len,
                         (unsigned int) next_write_pos);

                uint32_t write_offset = next_write_pos;
                uint32_t bytes_to_copy;
                uint32_t entry_size = btstack_tlv_flash_bank_aligned_entry_size(self, tag_len);

#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
                // copy in two steps to skip delete field

                // copy header
                uint8_t header_buffer[BTSTACK_TLV_ENTRY_HEADER_LEN];
                btstack_tlv_flash_bank_read(self, self->current_bank, tag_index, header_buffer, BTSTACK_TLV_ENTRY_HEADER_LEN);
                btstack_tlv_flash_bank_write(self, next_bank, next_write_pos,    header_buffer, BTSTACK_TLV_ENTRY_HEADER_LEN);
                tag_index    += self->entry_header_len + self->delete_tag_len;
                write_offset += self->entry_header_len + self->delete_tag_len;

                // preparee copy value
                bytes_to_copy = tag_len;
#else
                // copy everything as one block
                bytes_to_copy = entry_size;
#endif

                // copy value
                uint8_t copy_buffer[32];
                while (bytes_to_copy > 0) {
                    uint32_t bytes_this_iteration = btstack_min(bytes_to_copy, sizeof(copy_buffer));
                    btstack_tlv_flash_bank_read(self, self->current_bank, tag_index, copy_buffer, bytes_this_iteration);
                    btstack_tlv_flash_bank_write(self, next_bank, write_offset, copy_buffer, bytes_this_iteration);
                    tag_index     += bytes_this_iteration;
                    write_offset  += bytes_this_iteration;
                    bytes_to_copy -= bytes_this_iteration;
                }
                next_write_pos += entry_size;
            }
		}
		tlv_iterator_fetch_next(self, &it);
	}

	// prepare new one
	uint8_t epoch_buffer;
	btstack_tlv_flash_bank_read(self, self->current_bank, BTSTACK_TLV_BANK_HEADER_LEN-1, &epoch_buffer, 1);
	btstack_tlv_flash_bank_write_header(self, next_bank, (epoch_buffer + 1) & 3);
	self->current_bank = next_bank;
	self->write_offset = next_write_pos;
}

#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
static void btstack_tlv_flash_bank_delete_tag_until_offset(btstack_tlv_flash_bank_t * self, uint32_t tag, uint32_t offset){
	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it) && it.offset < offset){
		if (it.tag == tag){
			log_info("Erase tag '%x' at position %u", (unsigned int) tag, (unsigned int) it.offset);

			// mark entry as invalid
			uint32_t zero_value = 0;
#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
			// write delete field after entry header
			btstack_tlv_flash_bank_write(self, self->current_bank, it.offset+self->entry_header_len, (uint8_t*) &zero_value, sizeof(zero_value));
#else
            uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
            if (alignment <= 4){
                // if alignment < 4, overwrite only tag with zero value
                btstack_tlv_flash_bank_write(self, self->current_bank, it.offset, (uint8_t*) &zero_value, sizeof(zero_value));
            } else {
                // otherwise, overwrite complete entry. This results in a sequence of { tag: 0, len: 0 } entries
                uint8_t zero_buffer[32];
                memset(zero_buffer, 0, sizeof(zero_buffer));
                uint32_t entry_offset = 0;
                uint32_t entry_size = btstack_tlv_flash_bank_aligned_entry_size(self, it.len);
                while (entry_offset < entry_size) {
                    uint32_t bytes_to_write = btstack_min(entry_size - entry_offset, sizeof(zero_buffer));
                    btstack_tlv_flash_bank_write(self, self->current_bank, it.offset + entry_offset, zero_buffer, bytes_to_write);
                    entry_offset += bytes_to_write;
                }
            }
#endif
		}
		tlv_iterator_fetch_next(self, &it);
	}
}
#endif

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
static int btstack_tlv_flash_bank_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){

	btstack_tlv_flash_bank_t * self = (btstack_tlv_flash_bank_t *) context;

	uint32_t tag_index = 0;
	uint32_t tag_len   = 0;
	tlv_iterator_t it;
	btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
	while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
		if (it.tag == tag){
			log_info("Found tag '%x' at position %u", (unsigned int) tag, (unsigned int) it.offset);
			tag_index = it.offset;
			tag_len   = it.len;
#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
			break;
#endif
		}
		tlv_iterator_fetch_next(self, &it);
	}
	if (tag_index == 0) return 0;
	if (!buffer) return tag_len;
	int copy_size = btstack_min(buffer_size, tag_len);
	uint32_t value_offset = tag_index + self->entry_header_len;
#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
	// skip delete field
	value_offset += self->delete_tag_len;
#endif
	btstack_tlv_flash_bank_read(self, self->current_bank, value_offset, buffer, copy_size);
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

	// trigger migration if not enough space
	uint32_t required_space = self->entry_header_len + self->delete_tag_len + data_size;
	if (self->write_offset + required_space > self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){
		btstack_tlv_flash_bank_migrate(self);
	}

	if (self->write_offset + required_space > self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){
		log_error("couldn't write entry, not enough space left");
		return 2;
	}

    // prepare entry
    log_info("write '%" PRIx32 "', len %" PRIu32 " at %" PRIx32, tag, data_size, self->write_offset);

    uint8_t alignment_buffer[BTSTACK_FLASH_ALIGNMENT_MAX];
    memset(alignment_buffer, 0, sizeof(alignment_buffer));
    big_endian_store_32(alignment_buffer, 0, tag);
    big_endian_store_32(alignment_buffer, 4, data_size);

    uint32_t header_len = BTSTACK_TLV_ENTRY_HEADER_LEN;
    uint32_t value_size = data_size;
    uint32_t value_offset = self->write_offset + self->entry_header_len;
#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
    // skip delete field
    value_offset += self->delete_tag_len;
#else
    uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);

    // if alignment is larger than the entry header, store parts from value in alignment buffer
    if (alignment > BTSTACK_TLV_ENTRY_HEADER_LEN){
        // calculated number of value bytes to store in alignment buffer
        uint32_t bytes_from_value = btstack_min(alignment - BTSTACK_TLV_ENTRY_HEADER_LEN, value_size);

        // store parts from value in alignment buffer and update remaining value
        memcpy(&alignment_buffer[header_len], data, bytes_from_value);
        header_len += bytes_from_value;
        data       += bytes_from_value;
        value_size -= bytes_from_value;
        value_offset = self->write_offset + alignment;
    }
#endif

    // write value first
    if (value_size > 0){
        btstack_tlv_flash_bank_write(self, self->current_bank, value_offset, data, value_size);
    }

    // then entry
    btstack_tlv_flash_bank_write(self, self->current_bank, self->write_offset, alignment_buffer, header_len);

#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
	// overwrite old entries (if exists)
	btstack_tlv_flash_bank_delete_tag_until_offset(self, tag, self->write_offset);
#endif

	// done
	self->write_offset += btstack_tlv_flash_bank_aligned_entry_size(self, data_size);

	return 0;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_flash_bank_delete_tag(void * context, uint32_t tag){
#ifdef ENABLE_TLV_FLASH_WRITE_ONCE
    btstack_tlv_flash_bank_store_tag(context, tag, NULL, 0);
#else
    btstack_tlv_flash_bank_t * self = (btstack_tlv_flash_bank_t *) context;
	btstack_tlv_flash_bank_delete_tag_until_offset(self, tag, self->write_offset);
#endif
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
    self->delete_tag_len = 0;

    // BTSTACK_FLASH_ALIGNMENT_MAX must be larger than alignment
    uint32_t alignment = self->hal_flash_bank_impl->get_alignment(self->hal_flash_bank_context);
    btstack_assert(BTSTACK_FLASH_ALIGNMENT_MAX >= alignment);

#ifdef ENABLE_TLV_FLASH_EXPLICIT_DELETE_FIELD
	// set delete tag len
	self->delete_tag_len = (uint8_t) btstack_max(4, alignment);
	log_info("delete tag len %u", self->delete_tag_len);

    // set aligned entry header len
    self->entry_header_len = btstack_tlv_flash_bank_align_size(self, BTSTACK_TLV_ENTRY_HEADER_LEN);
	log_info("entry header len %u", self->entry_header_len);
#else
    UNUSED(alignment);
    // data starts right after entry header
    self->entry_header_len = BTSTACK_TLV_ENTRY_HEADER_LEN;
#endif

	// try to find current bank
	self->current_bank = btstack_tlv_flash_bank_get_latest_bank(self);
	log_info("found bank %d", self->current_bank);
	if (self->current_bank >= 0){

		// find last entry and write offset
		tlv_iterator_t it;
#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
		uint32_t last_tag = 0;
		uint32_t last_offset = 0;
#endif
        btstack_tlv_flash_bank_iterator_init(self, &it, self->current_bank);
		while (btstack_tlv_flash_bank_iterator_has_next(self, &it)){
#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
			last_tag = it.tag;
			last_offset = it.offset;
#endif
			tlv_iterator_fetch_next(self, &it);
		}
		self->write_offset = it.offset;

		if (self->write_offset <= self->hal_flash_bank_impl->get_size(self->hal_flash_bank_context)){

#ifndef ENABLE_TLV_FLASH_WRITE_ONCE
			// delete older instances of last_tag
			// this handles the unlikely case where MCU did reset after new value + header was written but before delete did complete
			if (last_tag){
				btstack_tlv_flash_bank_delete_tag_until_offset(self, last_tag, last_offset);
			}
#endif

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
        self->write_offset = btstack_tlv_flash_bank_align_size (self, BTSTACK_TLV_BANK_HEADER_LEN);
	}

	log_info("write offset %" PRIx32, self->write_offset);
	return &btstack_tlv_flash_bank;
}

