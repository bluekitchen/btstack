#include <stdint.h>
#include <stddef.h>

#include "classic/obex_iterator.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    obex_iterator_t it;
    for (obex_iterator_init_with_request_packet(&it, data, size); obex_iterator_has_more(&it) ; obex_iterator_next(&it)){
        uint32_t len = obex_iterator_get_data_len(&it);
        const uint8_t * item = obex_iterator_get_data(&it);
        // access first and last byte
        (void) data[0];
        (void) data[len-1];
    }
    return 0;
}
