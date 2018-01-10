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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/*
 *  btstack_hid_parser.h
 *
 *  Single-pass HID Report Parser: HID Report is directly parsed without preprocessing HID Descriptor to minimize memory
 */

#ifndef __BTSTACK_HID_PARSER_H
#define __BTSTACK_HID_PARSER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct  {
    int32_t  item_value;    
    uint16_t item_size; 
    uint8_t  item_type;
    uint8_t  item_tag;
    uint8_t  data_size;
} hid_descriptor_item_t;

typedef enum {
    BTSTACK_HID_REPORT_TYPE_OTHER  = 0x0,
    BTSTACK_HID_REPORT_TYPE_INPUT,
    BTSTACK_HID_REPORT_TYPE_OUTPUT,
    BTSTACK_HID_REPORT_TYPE_FEATURE,
} btstack_hid_report_type_t;

typedef enum {
    BTSTACK_HID_PARSER_SCAN_FOR_REPORT_ITEM,
    BTSTACK_HID_PARSER_USAGES_AVAILABLE,
    BTSTACK_HID_PARSER_COMPLETE,
} btstack_hid_parser_state_t;

typedef struct {

    // Descriptor
    const uint8_t * descriptor;
    uint16_t        descriptor_len;

    // Report
    btstack_hid_report_type_t report_type;
    const uint8_t * report;
    uint16_t        report_len;

    // State
    btstack_hid_parser_state_t state;

    hid_descriptor_item_t descriptor_item;

    uint16_t        descriptor_pos;
    uint16_t        report_pos_in_bit;

    // usage pos and usage_page after last main item, used to find next usage
    uint16_t        usage_pos;
    uint16_t        usage_page;

    // usage generator
    uint32_t        usage_minimum;
    uint32_t        usage_maximum;
    uint16_t        available_usages;    
    uint8_t         required_usages;
    uint8_t         active_record;
    uint8_t         have_usage_min;
    uint8_t         have_usage_max;

    // global
    int32_t         global_logical_minimum;
    int32_t         global_logical_maximum;
    uint16_t        global_usage_page; 
    uint8_t         global_report_size;
    uint8_t         global_report_count;
    uint8_t         global_report_id;
} btstack_hid_parser_t;

/* API_START */

/**
 * @brief Initialize HID Parser.
 * @param parser state 
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @param hid_report_type
 * @param hid_report
 * @param hid_report_len
 */
void btstack_hid_parser_init(btstack_hid_parser_t * parser, const uint8_t * hid_descriptor, uint16_t hid_descriptor_len, btstack_hid_report_type_t hid_report_type, const uint8_t * hid_report, uint16_t hid_report_len);

/**
 * @brief Checks if more fields are available
 * @param parser
 */
int  btstack_hid_parser_has_more(btstack_hid_parser_t * parser);

/**
 * @brief Get next field
 * @param parser
 * @param usage_page
 * @param usage
 * @param value provided in HID report
 */
void btstack_hid_parser_get_field(btstack_hid_parser_t * parser, uint16_t * usage_page, uint16_t * usage, int32_t * value);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_HID_PARSER_H
