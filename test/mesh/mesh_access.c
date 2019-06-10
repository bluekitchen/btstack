/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define __BTSTACK_FILE__ "mesh_access.c"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "mesh_access.h"
#include "btstack_memory.h"
#include "btstack_debug.h"
#include "bluetooth_company_id.h"

static mesh_element_t primary_element;

static btstack_linked_list_t mesh_elements;

static uint16_t mid_counter;

void mesh_access_init(void){
}

mesh_element_t * mesh_primary_element(void){
    return &primary_element;
}

void mesh_access_set_primary_element_address(uint16_t unicast_address){
    primary_element.unicast_address = unicast_address;
}

void mesh_element_add(mesh_element_t * element){
    btstack_linked_list_add_tail(&mesh_elements, (void*) element);
}

mesh_element_t * mesh_element_for_unicast_address(uint16_t unicast_address){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &mesh_elements);
    while (btstack_linked_list_iterator_has_next(&it)){
        mesh_element_t * element = (mesh_element_t *) btstack_linked_list_iterator_next(&it);
        if (element->unicast_address != unicast_address) continue;
        return element;
    }
    return NULL;
}

// Model Identifier utilities

uint32_t mesh_model_get_model_identifier(uint16_t vendor_id, uint16_t model_id){
    return (vendor_id << 16) | model_id;
}

uint32_t mesh_model_get_model_identifier_bluetooth_sig(uint16_t model_id){
    return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | model_id;
}

uint16_t mesh_model_get_model_id(uint32_t model_identifier){
    return model_identifier & 0xFFFFu;
}

uint16_t mesh_model_get_vendor_id(uint32_t model_identifier){
    return model_identifier >> 16;
}

int mesh_model_is_bluetooth_sig(uint32_t model_identifier){
    return mesh_model_get_vendor_id(model_identifier) == BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC;
}


void mesh_element_add_model(mesh_element_t * element, mesh_model_t * mesh_model){
    if (mesh_model_is_bluetooth_sig(mesh_model->model_identifier)){
        element->models_count_sig++;
    } else {
        element->models_count_vendor++;
    }
    mesh_model->mid = mid_counter++;
    btstack_linked_list_add_tail(&element->models, (btstack_linked_item_t *) mesh_model);
}

void mesh_model_iterator_init(mesh_model_iterator_t * iterator, mesh_element_t * element){
    btstack_linked_list_iterator_init(&iterator->it, &element->models);
}

int mesh_model_iterator_has_next(mesh_model_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_model_t * mesh_model_iterator_next(mesh_model_iterator_t * iterator){
    return (mesh_model_t *) btstack_linked_list_iterator_next(&iterator->it);
}

void mesh_element_iterator_init(mesh_element_iterator_t * iterator){
    btstack_linked_list_iterator_init(&iterator->it, &mesh_elements);
}

int mesh_element_iterator_has_next(mesh_element_iterator_t * iterator){
    return btstack_linked_list_iterator_has_next(&iterator->it);
}

mesh_element_t * mesh_element_iterator_next(mesh_element_iterator_t * iterator){
    return (mesh_element_t *) btstack_linked_list_iterator_next(&iterator->it);
}

mesh_model_t * mesh_model_get_by_identifier(mesh_element_t * element, uint32_t model_identifier){
    mesh_model_iterator_t it;
    mesh_model_iterator_init(&it, element);
    while (mesh_model_iterator_has_next(&it)){
        mesh_model_t * model = mesh_model_iterator_next(&it);
        if (model->model_identifier != model_identifier) continue;
        return model;
    }
    return NULL;
}

uint16_t mesh_pdu_src(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_src((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_src((mesh_network_pdu_t *) pdu);
        default:
            return MESH_ADDRESS_UNSASSIGNED;
    }
}

uint16_t mesh_pdu_dst(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_transport_dst((mesh_transport_pdu_t*) pdu);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_network_dst((mesh_network_pdu_t *) pdu);
        default:
            return MESH_ADDRESS_UNSASSIGNED;
    }
}

uint16_t mesh_pdu_netkey_index(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->netkey_index;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->netkey_index;
        default:
            return 0;
    }
}

uint16_t mesh_pdu_len(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->len;
        case MESH_PDU_TYPE_NETWORK:
            return ((mesh_network_pdu_t *) pdu)->len - 10;
        default:
            return 0;
    }
}

uint8_t * mesh_pdu_data(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return ((mesh_transport_pdu_t*) pdu)->data;
        case MESH_PDU_TYPE_NETWORK:
            return &((mesh_network_pdu_t *) pdu)->data[10];
        default:
            return NULL;
    }
}

// message parser

static int mesh_access_get_opcode(uint8_t * buffer, uint16_t buffer_size, uint32_t * opcode, uint16_t * opcode_size){
    switch (buffer[0] >> 6){
        case 0:
        case 1:
            if (buffer[0] == 0x7f) return 0;
            *opcode = buffer[0];
            *opcode_size = 1;
            return 1;
        case 2:
            if (buffer_size < 2) return 0;
            *opcode = big_endian_read_16(buffer, 0);
            *opcode_size = 2;
            return 1;
        case 3:
            if (buffer_size < 3) return 0;
            *opcode = (buffer[0] << 16) | little_endian_read_16(buffer, 1);
            *opcode_size = 3;
            return 1;
        default:
            return 0;
    }
}

static int mesh_access_transport_get_opcode(mesh_transport_pdu_t * transport_pdu, uint32_t * opcode, uint16_t * opcode_size){
    return mesh_access_get_opcode(transport_pdu->data, transport_pdu->len, opcode, opcode_size);
}

static int mesh_access_network_get_opcode(mesh_network_pdu_t * network_pdu, uint32_t * opcode, uint16_t * opcode_size){
    // TransMIC already removed by mesh_upper_transport_validate_unsegmented_message_ccm
    return mesh_access_get_opcode(&network_pdu->data[10], network_pdu->len - 10, opcode, opcode_size);
}

int mesh_access_pdu_get_opcode(mesh_pdu_t * pdu, uint32_t * opcode, uint16_t * opcode_size){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_TRANSPORT:
            return mesh_access_transport_get_opcode((mesh_transport_pdu_t*) pdu, opcode, opcode_size);
        case MESH_PDU_TYPE_NETWORK:
            return mesh_access_network_get_opcode((mesh_network_pdu_t *) pdu, opcode, opcode_size);
        default:
            return 0;
    }
}

void mesh_access_parser_skip(mesh_access_parser_state_t * state, uint16_t bytes_to_skip){
    state->data += bytes_to_skip;
    state->len  -= bytes_to_skip;
}

int mesh_access_parser_init(mesh_access_parser_state_t * state, mesh_pdu_t * pdu){
    state->data = mesh_pdu_data(pdu);
    state->len  = mesh_pdu_len(pdu);

    uint16_t opcode_size = 0;
    int ok = mesh_access_get_opcode(state->data, state->len, &state->opcode, &opcode_size);
    if (ok){
        mesh_access_parser_skip(state, opcode_size);
    }
    return ok;
}

uint16_t mesh_access_parser_available(mesh_access_parser_state_t * state){
    return state->len;
}

uint8_t mesh_access_parser_get_u8(mesh_access_parser_state_t * state){
    uint8_t value = *state->data;
    mesh_access_parser_skip(state, 1);
    return value;
}

uint16_t mesh_access_parser_get_u16(mesh_access_parser_state_t * state){
    uint16_t value = little_endian_read_16(state->data, 0);
    mesh_access_parser_skip(state, 2);
    return value;
}

uint32_t mesh_access_parser_get_u24(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 3);
    return value;
}

uint32_t mesh_access_parser_get_u32(mesh_access_parser_state_t * state){
    uint32_t value = little_endian_read_24(state->data, 0);
    mesh_access_parser_skip(state, 4);
    return value;
}

void mesh_access_parser_get_u128(mesh_access_parser_state_t * state, uint8_t * dest){
    reverse_128( state->data, dest);
    mesh_access_parser_skip(state, 16);
}

void mesh_access_parser_get_label_uuid(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

void mesh_access_parser_get_key(mesh_access_parser_state_t * state, uint8_t * dest){
    memcpy( dest, state->data, 16);
    mesh_access_parser_skip(state, 16);
}

uint32_t mesh_access_parser_get_model_identifier(mesh_access_parser_state_t * parser){
    if (mesh_access_parser_available(parser) == 4){
        return mesh_access_parser_get_u32(parser);
    } else {
        return (BLUETOOTH_COMPANY_ID_BLUETOOTH_SIG_INC << 16) | mesh_access_parser_get_u16(parser);
    }
}

// Mesh Access Message Builder

// message builder

static int mesh_access_setup_opcode(uint8_t * buffer, uint32_t opcode){
    if (opcode < 0x100){
        buffer[0] = opcode;
        return 1;
    }
    if (opcode < 0x10000){
        big_endian_store_16(buffer, 0, opcode);
        return 2;
    }
    buffer[0] = opcode >> 16;
    little_endian_store_16(buffer, 1, opcode & 0xffff);
    return 3;
}

mesh_transport_pdu_t * mesh_access_transport_init(uint32_t opcode){
    mesh_transport_pdu_t * pdu = mesh_transport_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(pdu->data, opcode);
    return pdu;
}

void mesh_access_transport_add_uint8(mesh_transport_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

void mesh_access_transport_add_uint16(mesh_transport_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

void mesh_access_transport_add_uint24(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

void mesh_access_transport_add_uint32(mesh_transport_pdu_t * pdu, uint32_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}
void mesh_access_transport_add_model_identifier(mesh_transport_pdu_t * pdu, uint32_t model_identifier){
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        mesh_access_transport_add_uint16( pdu, mesh_model_get_model_id(model_identifier) );
    } else {
        mesh_access_transport_add_uint32( pdu, model_identifier );
    }
}

mesh_network_pdu_t * mesh_access_network_init(uint32_t opcode){
    mesh_network_pdu_t * pdu = mesh_network_pdu_get();
    if (!pdu) return NULL;

    pdu->len  = mesh_access_setup_opcode(&pdu->data[10], opcode) + 10;
    return pdu;
}

void mesh_access_network_add_uint8(mesh_network_pdu_t * pdu, uint8_t value){
    pdu->data[pdu->len++] = value;
}

void mesh_access_network_add_uint16(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_16(pdu->data, pdu->len, value);
    pdu->len += 2;
}

void mesh_access_network_add_uint24(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_24(pdu->data, pdu->len, value);
    pdu->len += 3;
}

void mesh_access_network_add_uint32(mesh_network_pdu_t * pdu, uint16_t value){
    little_endian_store_32(pdu->data, pdu->len, value);
    pdu->len += 4;
}

void mesh_access_network_add_model_identifier(mesh_network_pdu_t * pdu, uint32_t model_identifier){
    if (mesh_model_is_bluetooth_sig(model_identifier)){
        mesh_access_network_add_uint16( pdu, mesh_model_get_model_id(model_identifier) );
    } else {
        mesh_access_network_add_uint32( pdu, model_identifier );
    }
}

// access message template

mesh_network_pdu_t * mesh_access_setup_unsegmented_message(const mesh_access_message_t *template, ...){
    mesh_network_pdu_t * network_pdu = mesh_access_network_init(template->opcode);
    if (!network_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint8( network_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_network_add_uint16( network_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint24( network_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_uint32( network_pdu, longword);
                break;
            case 'm':
                longword = va_arg(argptr, uint32_t);
                mesh_access_network_add_model_identifier( network_pdu, longword);
                break;
            default:
                log_error("Unsupported mesh message format specifier '%c", *format);
                break;
        }
        format++;
    }

    va_end(argptr);

    return network_pdu;
}

mesh_transport_pdu_t * mesh_access_setup_segmented_message(const mesh_access_message_t *template, ...){
    mesh_transport_pdu_t * transport_pdu = mesh_access_transport_init(template->opcode);
    if (!transport_pdu) return NULL;

    va_list argptr;
    va_start(argptr, template);

    // add params
    const char * format = template->format;
    uint16_t word;
    uint32_t longword;
    while (*format){
        switch (*format++){
            case '1':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint8( transport_pdu, word);
                break;
            case '2':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                mesh_access_transport_add_uint16( transport_pdu, word);
                break;
            case '3':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint24( transport_pdu, longword);
                break;
            case '4':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_uint32( transport_pdu, longword);
                break;
            case 'm':
                longword = va_arg(argptr, uint32_t);
                mesh_access_transport_add_model_identifier( transport_pdu, longword);
                break;
            default:
                break;
        }
    }

    va_end(argptr);

    return transport_pdu;
}
