/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
 *  btstack_uart_posix.h
 *
 *  Common code to access serial port via POSIX interface
 *  Used by hci_transport_h4_posix.c and hci_transport_h5.posix
 *
 */

#ifndef __BTSTACK_UART_POSIX_H
#define __BTSTACK_UART_POSIX_H

#include <stdint.h>

/**
 * @brief Open serial port 
 * @param device_name
 * @param flowcontrol enabled
 * @param baudrate
 * @returns fd if successful
 */
int btstack_uart_posix_open(const char * device_name, int flowcontrol, uint32_t baudrate);

/**
 * @brief Set Baudrate 
 * @param fd
 * @param baudrate
 * @returns 0 if successful
 */
int btstack_uart_posix_set_baudrate(int fd, uint32_t baudrate);

/**
 * @brief Set Parity
 * @param fd
 * @param parity
 * @returns 0 if successful
 */
int btstack_uart_posix_set_parity(int fd, int parity);

/**
 * @brief Blocking write
 * @param fd
 * @param data
 * @param size
 */
 void btstack_uart_posix_write(int fd, const uint8_t * data, int size);

#endif
