/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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

#define BTSTACK_FILE__ "btstack_chipset_cc256x.c"

/*
 *  btstack_chipset_cc256x.c
 *
 *  Adapter to use cc256x-based chipsets with BTstack
 *  
 *  Handles init script (a.k.a. Service Patch)
 *  Allows for non-standard UART baud rate
 *  Allows to configure transmit power
 *  Allows to activate eHCILL deep sleep mode
 *
 *  Issues with mspgcc LTS:
 *  - 20 bit support is not there yet -> .text cannot get bigger than 48 kb
 *  - arrays cannot have more than 32k entries
 * 
 *  workarounds:
 *  - store init script in .fartext and use assembly code to read from there 
 *  - split into two arrays
 *  
 * Issues with AVR
 *  - Harvard architecture doesn't allow to store init script directly -> use avr-libc helpers
 *
 * Documentation for TI VS CC256x commands: http://processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands
 *
 */

#include "btstack_config.h"
#include "btstack_chipset_cc256x.h"
#include "btstack_debug.h"
#include "hci.h"

#include <stddef.h>   /* NULL */
#include <stdio.h> 
#include <string.h>   /* memcpy */

// assert outgoing and incoming hci packet buffers can hold max hci command resp. event packet
#if HCI_OUTGOING_PACKET_BUFFER_SIZE < (HCI_CMD_HEADER_SIZE + 255)
#error "HCI_OUTGOING_PACKET_BUFFER_SIZE to small. Outgoing HCI packet buffer to small for largest HCI Command packet. Please set HCI_ACL_PAYLOAD_SIZE to 258 or higher."
#endif
#if HCI_INCOMING_PACKET_BUFFER_SIZE < (HCI_EVENT_HEADER_SIZE + 255)
#error "HCI_INCOMING_PACKET_BUFFER_SIZE to small. Incoming HCI packet buffer to small for largest HCI Event packet. Please set HCI_ACL_PAYLOAD_SIZE to 257 or higher."
#endif

#if defined(ENABLE_SCO_OVER_HCI) && defined(ENABLE_SCO_OVER_PCM)
#error "SCO can either be routed over HCI or PCM, please define only one of: ENABLE_SCO_OVER_HCI or ENABLE_SCO_OVER_PCM"
#endif

#if defined(__GNUC__) && defined(__MSP430X__) && (__MSP430X__ > 0)
#include "hal_compat.h"
#endif

#ifdef __AVR__
#include <avr/pgmspace.h>
#endif

#include "btstack_control.h"


// default init script provided by separate .c file
extern const uint8_t  cc256x_init_script[];
extern const uint32_t cc256x_init_script_size;

// custom init script set by btstack_chipset_cc256x_set_init_script
// used to select init scripts before each power up
static const uint8_t  * custom_init_script;
static uint32_t         custom_init_script_size;

// init script to use: either cc256x_init_script or custom_init_script
static const uint8_t  * init_script;
static uint32_t         init_script_size;

// power in db - set by btstack_chipset_cc256x_set_power
static int16_t    init_power_in_dB    = 13; // 13 dBm

// explicit power vectors of 16 uint8_t bytes
static const uint8_t * init_power_vectors[3];

// upload position
static uint32_t   init_script_offset  = 0;

// support for SCO over HCI
#ifdef ENABLE_SCO_OVER_HCI
static int   init_send_route_sco_over_hci = 0;
static const uint8_t hci_route_sco_over_hci[] = {
        // Follow recommendation from https://e2e.ti.com/support/wireless_connectivity/bluetooth_cc256x/f/660/t/397004
        // route SCO over HCI (connection type=1, tx buffer size = 120, tx buffer max latency= 720, accept packets with CRC Error
        0x10, 0xfe, 0x05, 0x01, 0x78, 0xd0, 0x02, 0x01,
};
#endif
#ifdef ENABLE_SCO_OVER_PCM
static int init_send_sco_i2s_config_cvsd = 0;
static const uint8_t hci_write_codec_config_cvsd[] = {
        0x06, 0xFD,              // HCI opcode = HCI_VS_Write_CODEC_Config
        0x22,                    // HCI param length
        0x00, 0x01,              // PCM clock rate 256, - clock rate 256000 Hz
        0x00,                    // PCM clock direction = master
        0x40, 0x1F, 0x00, 0x00,  // PCM frame sync = 8kHz
        0x10, 0x00,              // PCM frame sync duty cycle = 16 clk
        0x01,                    // PCM frame edge = rising edge
        0x00,                    // PCM frame polarity = active high
        0x00,                    // Reserved
        0x10, 0x00,              // PCM channel 1 out size = 16
        0x01, 0x00,              // PCM channel 1 out offset = 1
        0x01,                    // PCM channel 1 out edge = rising
        0x10, 0x00,              // PCM channel 1 in size = 16
        0x01, 0x00,              // PCM channel 1 in offset = 1
        0x00,                    // PCM channel 1 in edge = falling
        0x00,                    // Reserved
        0x10, 0x00,              // PCM channel 2 out size = 16
        0x11, 0x00,              // PCM channel 2 out offset = 17
        0x01,                    // PCM channel 2 out edge = rising
        0x10, 0x00,              // PCM channel 2 in size = 16
        0x11, 0x00,              // PCM channel 2 in offset = 17
        0x00,                    // PCM channel 2 in edge = falling
        0x00,                    // Reserved
};
#endif

static void chipset_init(const void * config){
    UNUSED(config);
    init_script_offset = 0;
#if defined(__GNUC__) && defined(__MSP430X__) && (__MSP430X__ > 0)
    // On MSP430, custom init script is not supported
    init_script_size = cc256x_init_script_size;
#else
    if (custom_init_script){
        log_info("cc256x: using custom init script");
        init_script      = custom_init_script;
        init_script_size = custom_init_script_size;
    } else {
        log_info("cc256x: using default init script");
        init_script      = cc256x_init_script;
        init_script_size = cc256x_init_script_size;
    }
#endif
#ifdef ENABLE_SCO_OVER_HCI
    init_send_route_sco_over_hci = 1;
#endif
#ifdef ENABLE_SCO_OVER_PCM
    init_send_sco_i2s_config_cvsd = 1;
#endif
}

static void chipset_set_baudrate_command(uint32_t baudrate, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x36;
    hci_cmd_buffer[1] = 0xFF;
    hci_cmd_buffer[2] = 0x04;
    hci_cmd_buffer[3] =  baudrate        & 0xff;
    hci_cmd_buffer[4] = (baudrate >>  8) & 0xff;
    hci_cmd_buffer[5] = (baudrate >> 16) & 0xff;
    hci_cmd_buffer[6] = 0;
}

static void chipset_set_bd_addr_command(bd_addr_t addr, uint8_t *hci_cmd_buffer){
    hci_cmd_buffer[0] = 0x06;
    hci_cmd_buffer[1] = 0xFC;
    hci_cmd_buffer[2] = 0x06;
    reverse_bd_addr(addr, &hci_cmd_buffer[3]);
}

// Output Power control from: http://e2e.ti.com/support/low_power_rf/f/660/p/134853/484767.aspx
#define NUM_POWER_LEVELS 16
#define DB_MIN_LEVEL -35
#define DB_PER_LEVEL 5
#define DB_DYNAMIC_RANGE 30

static int get_max_power_for_modulation_type(int type){
    // a) limit max output power
    int power_db;
    switch (type){
        case 0:     // GFSK
            power_db = 12;
            break;
        default:    // EDRx
            power_db = 10;
            break;
    }
    if (power_db > init_power_in_dB) {
        power_db = init_power_in_dB;
    }
    return power_db;
}

static int get_highest_level_for_given_power(int power_db, int recommended_db){
    int i = NUM_POWER_LEVELS-1;
    while (i) {
        if (power_db <= recommended_db) {
            return i;
        }
        power_db -= DB_PER_LEVEL;
        i--;
    }
    return 0;
}

static void update_set_power_vector(uint8_t *hci_cmd_buffer){
    uint8_t modulation_type = hci_cmd_buffer[3];
    btstack_assert(modulation_type <= 2);

    // explicit power vector provided by user
    if (init_power_vectors[modulation_type] != NULL){
        (void)memcpy(&hci_cmd_buffer[4], init_power_vectors[modulation_type], 16);
        return;
    }

    unsigned int i;
    int power_db = get_max_power_for_modulation_type(modulation_type);
    int dynamic_range = 0;

    // f) don't touch level 0
    for ( i = (NUM_POWER_LEVELS-1) ; i >= 1 ; i--){

#ifdef ENABLE_BLE
        // level 1 is BLE transmit power for GFSK
        if (i == 1 && modulation_type == 0) {
            hci_cmd_buffer[4+1] = 2 * get_max_power_for_modulation_type(modulation_type);
            // as level 0 isn't set, we're done
            continue;
        }
#endif
        hci_cmd_buffer[4+i] = 2 * power_db;

        if (dynamic_range + DB_PER_LEVEL > DB_DYNAMIC_RANGE) continue;  // e)

        power_db      -= DB_PER_LEVEL;   // d)
        dynamic_range += DB_PER_LEVEL;

        if (power_db > DB_MIN_LEVEL) continue;

        power_db = DB_MIN_LEVEL;    // b) 
    } 
}

// max permitted power for class 2 devices: 4 dBm
static void update_set_class2_single_power(uint8_t * hci_cmd_buffer){
    const int max_power_class_2 = 4;
    int i = 0;
    for (i=0;i<3;i++){
        hci_cmd_buffer[3+i] = get_highest_level_for_given_power(get_max_power_for_modulation_type(i), max_power_class_2);
    }
}

// eHCILL activate from http://e2e.ti.com/support/low_power_rf/f/660/p/134855/484776.aspx
static void update_sleep_mode_configurations(uint8_t * hci_cmd_buffer){
#ifdef ENABLE_EHCILL  
    hci_cmd_buffer[4] = 1;
#else
    hci_cmd_buffer[4] = 0;
#endif
}

static void update_init_script_command(uint8_t *hci_cmd_buffer){

    uint16_t opcode = hci_cmd_buffer[0] | (hci_cmd_buffer[1] << 8);

    switch (opcode){
        case 0xFD87:
            update_set_class2_single_power(hci_cmd_buffer);
            break;
        case 0xFD82:
            update_set_power_vector(hci_cmd_buffer);
            break;
        case 0xFD0C:
            update_sleep_mode_configurations(hci_cmd_buffer);
            break;
        default:
            break;
    }
}

static btstack_chipset_result_t chipset_next_command(uint8_t * hci_cmd_buffer){
    if (init_script_offset >= init_script_size) {

#ifdef ENABLE_SCO_OVER_HCI
        // append send route SCO over HCI if requested
        if (init_send_route_sco_over_hci){
            init_send_route_sco_over_hci = 0;
            memcpy(hci_cmd_buffer, hci_route_sco_over_hci, sizeof(hci_route_sco_over_hci));
            return BTSTACK_CHIPSET_VALID_COMMAND;
        }
#endif
#ifdef ENABLE_SCO_OVER_PCM
        // append sco i2s cvsd config
        if (init_send_sco_i2s_config_cvsd){
            init_send_sco_i2s_config_cvsd = 0;
            memcpy(hci_cmd_buffer, hci_write_codec_config_cvsd, sizeof(hci_write_codec_config_cvsd));
            return BTSTACK_CHIPSET_VALID_COMMAND;
        }
#endif
       return BTSTACK_CHIPSET_DONE;
    }
    
    // extracted init script has 0x01 cmd packet type, but BTstack expects them without
    init_script_offset++;

#if defined(__GNUC__) && defined(__MSP430X__) && (__MSP430X__ > 0)
    
    // workaround: use FlashReadBlock with 32-bit integer and assume init script starts at 0x10000
    uint32_t init_script_addr = 0x10000;
    FlashReadBlock(&hci_cmd_buffer[0], init_script_addr + init_script_offset, 3);  // cmd header
    init_script_offset += 3;
    int payload_len = hci_cmd_buffer[2];
    FlashReadBlock(&hci_cmd_buffer[3], init_script_addr + init_script_offset, payload_len);  // cmd payload

#elif defined (__AVR__)

    // workaround: use memcpy_P to access init script in lower 64 kB of flash
    memcpy_P(&hci_cmd_buffer[0], &init_script[init_script_offset], 3);
    init_script_offset += 3;
    int payload_len = hci_cmd_buffer[2];
    memcpy_P(&hci_cmd_buffer[3], &init_script[init_script_offset], payload_len);

#else    

    // use memcpy with pointer
    uint8_t * init_script_ptr = (uint8_t*) &init_script[0];
    memcpy(&hci_cmd_buffer[0], init_script_ptr + init_script_offset, 3);  // cmd header
    init_script_offset += 3;
    int payload_len = hci_cmd_buffer[2];
    memcpy(&hci_cmd_buffer[3], init_script_ptr + init_script_offset, payload_len);  // cmd payload

#endif

    init_script_offset += payload_len;

    // control power commands and ehcill 
    update_init_script_command(hci_cmd_buffer);

    return BTSTACK_CHIPSET_VALID_COMMAND; 
}


// MARK: public API
void btstack_chipset_cc256x_set_power(int16_t power_in_dB){
    init_power_in_dB = power_in_dB;
}

void btstack_chipset_cc256x_set_power_vector(uint8_t modulation_type, const uint8_t * power_vector){
    btstack_assert(modulation_type <= 2);
    init_power_vectors[modulation_type] = power_vector;
}

void btstack_chipset_cc256x_set_init_script(uint8_t * data, uint32_t size){
    custom_init_script      = data;
    custom_init_script_size = size;
}

static const btstack_chipset_t btstack_chipset_cc256x = {
    "CC256x",
    chipset_init,
    chipset_next_command,
    chipset_set_baudrate_command,
    chipset_set_bd_addr_command,
};

const btstack_chipset_t * btstack_chipset_cc256x_instance(void){
    return &btstack_chipset_cc256x;
}

