#ifndef __FIFO_H__
#define __FIFO_H__
#include "type.h"

typedef struct {
    uint8_t *start;
    uint8_t *end;
    uint8_t *read_ptr;
    uint8_t *write_ptr;
    uint32_t depth;
    uint32_t valid_data_length;
} fifo_t;

uint8_t fifo_init(uint32_t fifo_depth);
void fifo_deinit();
uint8_t fifo_push(uint8_t data);
uint8_t fifo_pop(uint8_t *pop_data);
uint8_t is_fifo_empty();
uint8_t is_fifo_full();
uint32_t fifo_get_valid_data_length();

#endif
