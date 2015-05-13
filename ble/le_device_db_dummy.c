/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
#include  "le_device_db.h"

 // Central Device db interface
void le_device_db_init(void){}

// @returns index if successful, -1 otherwise
int le_device_db_add(int addr_type, bd_addr_t addr, sm_key_t irk){
	return -1;
}

// @returns number of device in db
int le_device_db_count(void){
	return 0;
}

/**
 * @brief set remote encryption info
 * @brief index
 * @brief ediv 
 * @brief rand
 * @brief ltk
 */
void le_device_db_encryption_set(int index, uint16_t ediv, uint8_t rand[8], sm_key_t ltk){}

/**
 * @brief get remote encryption info
 * @brief index
 * @brief ediv 
 * @brief rand
 * @brief ltk
 */
void le_device_db_encryption_get(int index, uint16_t * ediv, uint8_t rand[8], sm_key_t ltk){}

// get device information: addr type and address
void le_device_db_info(int index, int * addr_type, bd_addr_t addr, sm_key_t csrk){}

// get signature key
void le_device_db_csrk_get(int index, sm_key_t csrk){}

// query last used/seen signing counter
uint32_t le_device_db_remote_counter_get(int index){ 
	return 0xffffffff;
}

// update signing counter
void le_device_db_local_counter_set(int index, uint32_t counter){}

// query last used/seen signing counter
uint32_t le_device_db_local_counter_get(int index){ 
	return 0xffffffff;
}

// update signing counter
void le_device_db_remote_counter_set(int index, uint32_t counter){}

// free device
void le_device_db_remove(int index){}

