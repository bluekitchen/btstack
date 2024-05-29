/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btp_ascs.c"

#include "btp_ascs.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"

#include <stdint.h>
#include "btpclient.h"
#include "btp.h"
#include "btp_server.h"

void btp_ascs_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) {
    // provide op info for response
    response_len = 0;
    response_service_id = BTP_SERVICE_ID_ASCS;
    response_op = opcode;
    switch (opcode) {
        case BTP_ASCS_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_ASCS_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER) {
                uint8_t commands = 0;
                btp_send(response_service_id, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_ASCS_DISABLE:
            if (controller_index == 0) {
                /**
                    Address_Type (1 octet)
                    Address (6 octets)
                    ASE_ID (1 octet)
                 */

                uint16_t offset = 0;
                bd_addr_type_t addr_type = data[offset++];
                bd_addr_t address;
                reverse_bd_addr(&data[offset], address);
                offset += 6;

                server_t * server = btp_server_for_address(addr_type, address);
                btstack_assert(server != NULL);

                // get ASE info
                uint8_t ase_id = data[offset++];
                MESSAGE("BTP_ASCS_DISABLE %u, ASE ID %u", server->server_id, ase_id);
                server->ascs_operation_active = true;
                audio_stream_control_service_client_streamendpoint_disable(server->ascs_cid, ase_id);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_ASCS_RECEIVER_STOP_READY:
            if (controller_index == 0) {
                /**
                    Address_Type (1 octet)
                    Address (6 octets)
                    ASE_ID (1 octet)
                 */

                uint16_t offset = 0;
                bd_addr_type_t addr_type = data[offset++];
                bd_addr_t address;
                reverse_bd_addr(&data[offset], address);
                offset += 6;

                server_t *server = btp_server_for_address(addr_type, address);
                btstack_assert(server != NULL);

                // get ASE info
                uint8_t ase_id = data[offset++];
                MESSAGE("BTP_ASCS_RECEIVER_STOP_READY %u, ASE ID %u", server->server_id, ase_id);

                server->ascs_operation_active = true;
                audio_stream_control_service_client_streamendpoint_receiver_stop_ready(server->ascs_cid, ase_id);
                btp_send(response_service_id, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            MESSAGE("BTP ASCS Operation 0x%02x not implemented", opcode);
            btstack_assert(false);
            break;
    }
}

void btp_ascs_init(void) {
}