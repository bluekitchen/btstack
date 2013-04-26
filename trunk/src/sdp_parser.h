
//*****************************************************************************
//
// minimal setup for SDP client over USB or UART
//
//*****************************************************************************
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/sdp_util.h>

typedef struct de_state {
    uint8_t  in_state_GET_DE_HEADER_LENGTH ;
    uint32_t addon_header_bytes;
    uint32_t de_size;
    uint32_t de_offset;
} de_state_t;

typedef enum sdp_parser_event_type {
    SDP_PARSER_ATTRIBUTE_VALUE = 1,
    SDP_PARSER_COMPLETE,
} sdp_parser_event_type_t;

typedef struct sdp_parser_event {
    uint8_t type;
} sdp_parser_event_t;

typedef struct sdp_parser_attribute_value_event {
    uint8_t type;
    int record_id;
    uint16_t attribute_id;
    uint32_t attribute_length;
    int data_offset;
    uint8_t data;
} sdp_parser_attribute_value_event_t;

typedef struct sdp_parser_complete_event {
    uint8_t type;
    uint8_t status; // 0 == OK
} sdp_parser_complete_event_t;

void de_state_init(de_state_t * state);
int  de_state_size(uint8_t eventByte, de_state_t *de_state);

void sdp_parser_init();
void sdp_parser_handle_chunk(uint8_t * data, uint16_t size);
void sdp_parser_handle_done(uint8_t status);
void sdp_parser_register_callback(void (*sdp_callback)(sdp_parser_event_t* event));
