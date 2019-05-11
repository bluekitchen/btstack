/*
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
 *  btstack_network.h
 *  Network interface abstraction
 */

#ifndef BTSTACK_NETWORK_H
#define BTSTACK_NETWORK_H

#include <stdint.h>
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize network interface
 * @param send_packet_callback
 */
void btstack_network_init(void (*send_packet_callback)(const uint8_t * packet, uint16_t size));

/**
 * @brief Bring up network interfacd
 * @param network_address
 * @return 0 if ok
 */
int  btstack_network_up(bd_addr_t network_address);

/**
 * @brief Shut down network interfacd
 * @param network_address
 * @return 0 if ok
 */
int  btstack_network_down(void);

/** 
 * @brief Receive packet on network interface, e.g., forward packet to TCP/IP stack 
 * @param packet
 * @param size
 */
void btstack_network_process_packet(const uint8_t * packet, uint16_t size);

/** 
 * @brief Notify network interface that packet from send_packet_callback was sent and the next packet can be delivered.
 */
void btstack_network_packet_sent(void);

/**
 * @brief Get network name after network was activated
 * @note e.g. tapX on Linux, might not be useful on all platforms
 * @returns network name
 */
const char * btstack_network_get_name(void);

#if defined __cplusplus
}
#endif

#endif
