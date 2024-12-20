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
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

/**
 * @title HID Parser
 *
 * Single-pass HID Report Parser: HID Report is directly parsed without preprocessing HID Descriptor to minimize memory.
 *
 */

#ifndef BTSTACK_HID_PARSER_H
#define BTSTACK_HID_PARSER_H

#include <stdint.h>
#include "btstack_bool.h"
#include "btstack_hid.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    Main=0,
    Global,
    Local,
    Reserved
} TagType;

typedef enum {
    Input=8,
    Output,
    Coll,
    Feature,
    EndColl
} MainItemTag;

typedef enum {
    UsagePage,
    LogicalMinimum,
    LogicalMaximum,
    PhysicalMinimum,
    PhysicalMaximum,
    UnitExponent,
    Unit,
    ReportSize,
    ReportID,
    ReportCount,
    Push,
    Pop
} GlobalItemTag;

typedef enum {
    Usage,
    UsageMinimum,
    UsageMaximum,
    DesignatorIndex,
    DesignatorMinimum,
    DesignatorMaximum,
    StringIndex,
    StringMinimum,
    StringMaximum,
    Delimiter 
} LocalItemTag;

typedef struct  {
    int32_t  item_value;    
    uint16_t item_size; 
    uint8_t  item_type;
    uint8_t  item_tag;
    uint8_t  data_size;
} hid_descriptor_item_t;

typedef struct {
    // Descriptor
    const uint8_t * descriptor;
    uint16_t        descriptor_pos;
    uint16_t        descriptor_len;
    // parsed item
    bool            item_ready;
    bool            valid;
    hid_descriptor_item_t descriptor_item;
} btstack_hid_descriptor_iterator_t;


typedef enum {
    BTSTACK_HID_USAGE_ITERATOR_STATE_SCAN_FOR_REPORT_ITEM,
    BTSTACK_HID_USAGE_ITERATOR_USAGES_AVAILABLE,
    BTSTACK_HID_USAGE_ITERATOR_PARSER_COMPLETE,
} btstack_hid_usage_iterator_state_t;

typedef struct {
    // Descriptor
    const uint8_t * descriptor;
    uint16_t        descriptor_len;
    btstack_hid_descriptor_iterator_t descriptor_iterator;

    // State
    btstack_hid_usage_iterator_state_t state;

    hid_descriptor_item_t descriptor_item;

    hid_report_type_t report_type;

    // report bit pos does not include the optional Report ID
    uint16_t        report_pos_in_bit;

    // usage pos and usage_page after last main item, used to find next usage
    uint16_t        usage_pos;
    uint16_t        usage_page;

    // usage generator
    uint32_t        usage_minimum;
    uint32_t        usage_maximum;
    uint16_t        available_usages;
    uint16_t        required_usages;
    bool            usage_range;
    uint8_t         active_record;

    // global
    int32_t         global_logical_minimum;
    int32_t         global_logical_maximum;
    uint16_t        global_usage_page;
    uint8_t         global_report_size;
    uint8_t         global_report_count;
    uint16_t        global_report_id;

} btstack_hid_usage_iterator_t;

typedef struct {
    uint16_t report_id; // 8-bit report ID or 0xffff if not set
    uint16_t bit_pos;   // position in bit
    uint16_t usage_page;
    uint16_t usage;
    uint8_t  size;      // in bit

    // cached values of relevant descriptor item (input,output,feature)
    hid_descriptor_item_t descriptor_item;
    int32_t               global_logical_minimum;
} btstack_hid_usage_item_t;


typedef struct {
    btstack_hid_usage_iterator_t usage_iterator;

    const uint8_t *   report;
    uint16_t          report_len;

    bool have_report_usage_ready;
    btstack_hid_usage_item_t descriptor_usage_item;

} btstack_hid_parser_t;


/* API_START */

// HID Descriptor Iterator

/**
 * @brief Init HID Descriptor iterator
 * @param iterator
 * @param hid_descriptor
 * @param hid_descriptor_len
 */
void btstack_hid_descriptor_iterator_init(btstack_hid_descriptor_iterator_t * iterator, const uint8_t * hid_descriptor, uint16_t hid_descriptor_len);

/**
 * @brief Check if HID Descriptor Items are available
 * @param iterator
 * @return
 */
bool btstack_hid_descriptor_iterator_has_more(btstack_hid_descriptor_iterator_t * iterator);

/**
 * @brief Get next HID Descriptor Item
 * @param iterator
 * @return
 */
const hid_descriptor_item_t * btstack_hid_descriptor_iterator_get_item(btstack_hid_descriptor_iterator_t * iterator);

/**
 * @brief Returns if HID Descriptor was well formed
 * @param iterator
 * @return
 */
bool btstack_hid_descriptor_iterator_valid(btstack_hid_descriptor_iterator_t * iterator);


// HID Descriptor Usage Iterator

/**
 * @brief Initialize usages iterator for HID Descriptor and report type
 * @param parser
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @param hid_report_type
 */
void btstack_hid_usage_iterator_init(btstack_hid_usage_iterator_t * iterator, const uint8_t * hid_descriptor, uint16_t hid_descriptor_len, hid_report_type_t hid_report_type);

/**
 * @brief Checks if more usages are available
 * @param parser
 */
bool btstack_hid_usage_iterator_has_more(btstack_hid_usage_iterator_t * iterator);

/**
 * @brief Get current usage item
 * @param parser
 * @param item
 */
void btstack_hid_usage_iterator_get_item(btstack_hid_usage_iterator_t * iterator, btstack_hid_usage_item_t * item);


// HID Report Iterator

/**
 * @brief Initialize HID Parser.
 * @param parser state 
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @param hid_report_type
 * @param hid_report
 * @param hid_report_len
 */
void btstack_hid_parser_init(btstack_hid_parser_t * parser, const uint8_t * hid_descriptor, uint16_t hid_descriptor_len, hid_report_type_t hid_report_type, const uint8_t * hid_report, uint16_t hid_report_len);

/**
 * @brief Checks if more fields are available
 * @param parser
 */
bool btstack_hid_parser_has_more(btstack_hid_parser_t * parser);

/**
 * @brief Get next field
 * @param parser
 * @param usage_page
 * @param usage
 * @param value provided in HID report
 */
void btstack_hid_parser_get_field(btstack_hid_parser_t * parser, uint16_t * usage_page, uint16_t * usage, int32_t * value);


/**
 * @brief Parses descriptor and returns report size for given report ID and report type
 * @param report_id or HID_REPORT_ID_UNDEFINED
 * @param report_type
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @return report size in bytes or 0 on parsing error
 */
int btstack_hid_get_report_size_for_id(uint16_t report_id, hid_report_type_t report_type, const uint8_t *hid_descriptor,
                                       uint16_t hid_descriptor_len);

/**
 * @brief Parses descriptor and returns status for given report ID
 * @param report_id
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @return status for report id
 */
hid_report_id_status_t btstack_hid_report_id_valid(uint16_t report_id, const uint8_t * hid_descriptor, uint16_t hid_descriptor_len);

/**
 * @brief Parses descriptor and returns true if report ID found
 * @param hid_descriptor
 * @param hid_descriptor_len
 * @return true if report ID declared in descriptor
 */
bool btstack_hid_report_id_declared(const uint8_t *hid_descriptor, uint16_t hid_descriptor_len);


/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_HID_PARSER_H
