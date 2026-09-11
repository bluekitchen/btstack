#ifndef TEST_READ_BOUNDS_H
#define TEST_READ_BOUNDS_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ble/att_db.h"

// Cover length queries, partial reads, empty buffers, and offsets past the value.
static void check_read_bounds(att_read_callback_t callback, uint16_t handle,
                              const uint8_t * expected, uint16_t length){
    for (uint16_t offset = 0; offset <= length + 1; offset++){
        assert(callback(1, handle, offset, NULL, 0) == length);
        for (uint16_t capacity = 0; capacity <= length + 1; capacity++){
            uint8_t * buffer = malloc(capacity + 1u);
            assert(buffer != NULL);
            memset(buffer, 0xa5, capacity + 1u);
            uint16_t copied = callback(1, handle, offset, buffer, capacity);
            uint16_t remaining = offset < length ? length - offset : 0;
            uint16_t wanted = remaining < capacity ? remaining : capacity;
            assert(buffer[capacity] == 0xa5);
            assert(copied == wanted);
            if (wanted != 0){
                assert(memcmp(buffer, expected + offset, wanted) == 0);
            }
            free(buffer);
        }
    }
}
#endif
