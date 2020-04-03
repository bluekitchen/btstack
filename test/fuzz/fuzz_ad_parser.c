#include <stdint.h>
#include <stddef.h>

#include "ad_parser.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // ad parser uses uint88_t length
    if (size > 255) return 0;
    // test ad iterator by calling simple function that uses it
    ad_data_contains_uuid16(size, data, 0xffff);
    return 0;
}
