/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */
#pragma once

#include <btstack/btstack.h>
#include <stdint.h>
#include "att.h"

 /*
  * @brief setup ATT server
  * @param db attribute database created by compile-gatt.ph
  * @param read_callback, see att.h, can be NULL
  * @param write_callback, see attl.h, can be NULL
  */
void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback);

/*
 * @brief register packet handler for general HCI Events like connect, diconnect, etc.
 * @param handler
 */
void att_server_register_packet_handler(btstack_packet_handler_t handler);

/*
 * @brief tests if a notification or indication can be send right now
 * @return 1, if packet can be sent
 */
int  att_server_can_send();

/*
 * @brief notify client about attribute value change
 */
void att_server_notify(uint16_t handle, uint8_t *value, uint16_t value_len);

/*
 * @brief indicate value change to client. client is supposed to reply with an indication_response
 */
void att_server_indicate(uint16_t handle, uint8_t *value, uint16_t value_len);
