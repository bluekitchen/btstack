#include <stdint.h>
#include <stddef.h>

#include "classic/hfp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // test ad iterator by calling simple function that uses it
    if (size < 1) return 0;

    int is_handsfree = data[0] & 1;
    hfp_connection_t hfp_connection;
    memset(&hfp_connection, 0, sizeof(hfp_connection_t));

    uint32_t i;
    for (i = 1; i < size; i++){
        hfp_parse(&hfp_connection, data[i], is_handsfree);
    }
 
    return 0;
}
