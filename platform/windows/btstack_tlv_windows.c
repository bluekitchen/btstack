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

#define BTSTACK_FILE__ "btstack_tlv_windows.c"

#include "btstack_tlv.h"
#include "btstack_tlv_windows.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_debug.h"
#include "string.h"

#include <stdlib.h>
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

#define MAX_TLV_VALUE_SIZE 2048

static const char btstack_tlv_header_magic[] = "BTstack";

#define DUMMY_SIZE 4
typedef struct tlv_entry {
	void   * next;
	uint32_t tag;
	uint32_t len;
	uint8_t  value[DUMMY_SIZE];	// dummy size
} tlv_entry_t;

static int btstack_tlv_windows_append_tag(btstack_tlv_windows_t * self, uint32_t tag, const uint8_t * data, uint32_t data_size){

	if (self->file == INVALID_HANDLE_VALUE) return 1;

	log_info("append tag %04x, len %u", tag, data_size);

	uint8_t header[8];
	big_endian_store_32(header, 0, tag);
	big_endian_store_32(header, 4, data_size);
	DWORD written_header = 0;
	(void)WriteFile(
		self->file,			// file handle
		&header,			// start of data to write
		sizeof(header),		// number of bytes to write
		&written_header,	// number of bytes that were written
		NULL);				// no overlapped structure

	if (written_header != sizeof(header)) return 1;
	if (data_size > 0) {
		DWORD written_value = 0;
		(void)WriteFile(
			self->file,			// file handle
			data,   			// start of data to write
            data_size,		    // number of bytes to write
			&written_value,	    // number of bytes that were written
			NULL);				// no overlapped structure
		if (written_value != data_size) return 1;
	}
	return 1;
}

static tlv_entry_t * btstack_tlv_windows_find_entry(btstack_tlv_windows_t * self, uint32_t tag){
	btstack_linked_list_iterator_t it;
	btstack_linked_list_iterator_init(&it, &self->entry_list);
	while (btstack_linked_list_iterator_has_next(&it)){
		tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
		if (entry->tag != tag) continue;
		return entry;
	}
	return NULL;
}

/**
 * Delete Tag
 * @param tag
 */
static void btstack_tlv_windows_delete_tag(void * context, uint32_t tag){
	btstack_tlv_windows_t * self = (btstack_tlv_windows_t *) context;
	btstack_linked_list_iterator_t it;
	btstack_linked_list_iterator_init(&it, &self->entry_list);
	while (btstack_linked_list_iterator_has_next(&it)){
		tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
		if (entry->tag != tag) continue;
		btstack_linked_list_iterator_remove(&it);
		free(entry);
		btstack_tlv_windows_append_tag(self, tag, NULL, 0);
		return;
	}
}

/**
 * Get Value for Tag
 * @param tag
 * @param buffer
 * @param buffer_size
 * @returns size of value
 */
static int btstack_tlv_windows_get_tag(void * context, uint32_t tag, uint8_t * buffer, uint32_t buffer_size){
	btstack_tlv_windows_t * self = (btstack_tlv_windows_t *) context;
	tlv_entry_t * entry = btstack_tlv_windows_find_entry(self, tag);
	// not found
	if (!entry) return 0;
	// return len if buffer = NULL
	if (!buffer) return entry->len;
	// otherwise copy data into buffer
	uint16_t bytes_to_copy = btstack_min(buffer_size, entry->len);
	memcpy(buffer, &entry->value[0], bytes_to_copy);
	return bytes_to_copy;
}

/**
 * Store Tag 
 * @param tag
 * @param data
 * @param data_size
 */
static int btstack_tlv_windows_store_tag(void * context, uint32_t tag, const uint8_t * data, uint32_t data_size){
	btstack_tlv_windows_t * self = (btstack_tlv_windows_t *) context;

	// enforce arbitrary max value size
	btstack_assert(data_size <= MAX_TLV_VALUE_SIZE);

	// remove old entry
	tlv_entry_t * old_entry = btstack_tlv_windows_find_entry(self, tag);
	if (old_entry){
		btstack_linked_list_remove(&self->entry_list, (btstack_linked_item_t *) old_entry);
		free(old_entry);
	}

	// create new entry
	uint32_t entry_size = sizeof(tlv_entry_t) - DUMMY_SIZE + data_size;
	tlv_entry_t * new_entry = (tlv_entry_t *) malloc(entry_size);
	if (!new_entry) return 0;
	memset(new_entry, 0, entry_size);
	new_entry->tag = tag;
	new_entry->len = data_size;
	memcpy(&new_entry->value[0], data, data_size);

	// append new entry
	btstack_linked_list_add(&self->entry_list, (btstack_linked_item_t *) new_entry);

	// write new tag
	btstack_tlv_windows_append_tag(self, tag, data, data_size);

	return 0;
}

// returns 0 on success
static int btstack_tlv_windows_read_db(btstack_tlv_windows_t * self){
	// open file
	log_info("open db %s", self->db_path);
	self->file = CreateFile(
		self->db_path,
		GENERIC_READ | GENERIC_WRITE,
		0,	// do not share
		NULL, // default security
		OPEN_EXISTING, // only open if it exists
		FILE_ATTRIBUTE_NORMAL,
		NULL);

    uint8_t header[BTSTACK_TLV_HEADER_LEN];
	if (self->file != INVALID_HANDLE_VALUE){
    	// check header
		bool ok = ReadFile(self->file, header, BTSTACK_TLV_HEADER_LEN, NULL, NULL);
		int file_valid = 0;
	    if (ok){
	    	if (memcmp(header, btstack_tlv_header_magic, strlen(btstack_tlv_header_magic)) == 0){
		    	log_info("BTstack Magic Header found");
		    	// read entries
		    	while (true){
					uint8_t entry[8];
					DWORD bytes_read;
					ok = ReadFile(self->file, entry, sizeof(entry), &bytes_read, NULL);
					if ( ok && (bytes_read == 0)) {
						// EOF, we're good
						file_valid = 1;
						break;
					}

					if ( (ok == false) || (bytes_read != sizeof(entry))) {
						break;
					}

					uint32_t tag = big_endian_read_32(entry, 0);
                    uint32_t len = big_endian_read_32(entry, 4);

                    // arbitrary safety check: values <= MAX_TLV_VALUE_SIZE
					if (len > MAX_TLV_VALUE_SIZE) {
						break;
					}

                    // create new entry for regular tag
                    tlv_entry_t * new_entry = NULL;
                    if (len > 0) {
                        new_entry = (tlv_entry_t *) malloc(sizeof(tlv_entry_t) - DUMMY_SIZE + len);
                        if (!new_entry) return 0;
                        new_entry->tag = tag;
                        new_entry->len = len;

                        // read
						DWORD value_read;
						ok = ReadFile(self->file, &new_entry->value[0],len, &value_read, NULL);
						if ((ok == false) || (value_read != len)) {
							break;
						}
                    }

                    // remove old entry
                    tlv_entry_t * old_entry = btstack_tlv_windows_find_entry(self, tag);
                    if (old_entry){
                        btstack_linked_list_remove(&self->entry_list, (btstack_linked_item_t *) old_entry);
                        free(old_entry);
                    }

                    // append new entry
                    if (new_entry){
	                    btstack_linked_list_add(&self->entry_list, (btstack_linked_item_t *) new_entry);
                    }
		    	}
	    	}
	    }
	    if (!file_valid) {
	    	log_info("file invalid, re-create");
			CloseHandle(self->file);
    		self->file = INVALID_HANDLE_VALUE;
	    }
    }
    if (self->file == INVALID_HANDLE_VALUE){
    	// create new file
		self->file = CreateFile(
			self->db_path,
		    GENERIC_WRITE,
			0,	// do not share
			NULL, // default security
			CREATE_ALWAYS, // create new file / replace old one
			FILE_ATTRIBUTE_NORMAL,
			NULL);

        if (self->file == INVALID_HANDLE_VALUE) {
            log_error("failed to create file");
            return -1;
        }
	    memset(header, 0, sizeof(header));
	    memcpy(header, btstack_tlv_header_magic, sizeof(btstack_tlv_header_magic));

		DWORD dwBytesWritten = 0;
		(void)WriteFile(
			self->file,			// file handle
			&header,			// start of data to write
			sizeof(header),		// number of bytes to write
			&dwBytesWritten,	// number of bytes that were written
			NULL);				// no overlapped structure
		UNUSED(dwBytesWritten);

		// write out all valid entries (if any)
		btstack_linked_list_iterator_t it;
		btstack_linked_list_iterator_init(&it, &self->entry_list);
		while (btstack_linked_list_iterator_has_next(&it)){
			tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
			btstack_tlv_windows_append_tag(self, entry->tag, &entry->value[0], entry->len);
		}
    }
	return 0;
}

static const btstack_tlv_t btstack_tlv_windows = {
	/* int  (*get_tag)(..);     */ &btstack_tlv_windows_get_tag,
	/* int (*store_tag)(..);    */ &btstack_tlv_windows_store_tag,
	/* void (*delete_tag)(v..); */ &btstack_tlv_windows_delete_tag,
};

/**
 * Init Tag Length Value Store
 */
const btstack_tlv_t * btstack_tlv_windows_init_instance(btstack_tlv_windows_t * self, const char * db_path){
	memset(self, 0, sizeof(btstack_tlv_windows_t));
	self->db_path = db_path;
	self->file = INVALID_HANDLE_VALUE;

	// read DB
	btstack_tlv_windows_read_db(self);
	return &btstack_tlv_windows;
}

/**
 * Free TLV entries
 * @param self
 */
void btstack_tlv_windows_deinit(btstack_tlv_windows_t * self){
    // free all entries
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &self->entry_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        tlv_entry_t * entry = (tlv_entry_t*) btstack_linked_list_iterator_next(&it);
		btstack_linked_list_iterator_remove(&it);
		free(entry);
    }
}
