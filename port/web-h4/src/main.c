/*
* Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "main.c"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <emscripten/emscripten.h>

#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_js.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "btstack_uart.h"
#include "hci_dump_embedded_stdout.h"
#include "hci_transport_h4.h"
#include "hci_transport_h4_js.h"
#include "ble/le_device_db_tlv.h"
#include "classic/btstack_link_key_db_tlv.h"

static const hci_transport_config_uart_t config = {
    HCI_TRANSPORT_CONFIG_UART,
    115200,
    0,
    1,
    NULL,
    BTSTACK_UART_PARITY_OFF
};

static int led_state = 0;
void hal_led_toggle(void){
    led_state = 1 - led_state;
    printf("LED State %u\n", led_state);
}

EMSCRIPTEN_KEEPALIVE
int main() {
    printf("Hello from WebAssembly!\n");

    // init memory
    btstack_memory_init();

    // init run loop
    btstack_run_loop_init(btstack_run_loop_js_get_instance());

    // init HCI dump to stdout
    hci_dump_init(hci_dump_embedded_stdout_get_instance());

    // init HCI with H4 HCI Transport over JavaScript
    hci_init(hci_transport_h4_js_instance(), (void*) &config);

    // use in-memory TLV for testing
    const btstack_tlv_t * btstack_tlv_impl = btstack_tlv_none_init_instance();
    void                * btstack_tlv_context = NULL;
    btstack_tlv_set_instance(btstack_tlv_impl, btstack_tlv_context);

    // setup Link Key DB using TLV
    const btstack_link_key_db_t * btstack_link_key_db = btstack_link_key_db_tlv_get_instance(btstack_tlv_impl, &btstack_tlv_context);
    hci_set_link_key_db(btstack_link_key_db);

    // setup LE Device DB using TLV
    le_device_db_tlv_configure(btstack_tlv_impl, btstack_tlv_context);

    printf("Running...\n\r");

    return 0;
}
