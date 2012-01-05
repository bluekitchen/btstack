/*
 * Copyright (C) 2011 by Matthias Ringwald
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
 */

/*
 *  btstsack_memory.h
 *
 *  @brief BTstack memory management via configurable memory pools
 *
 */

#pragma once

#if defined __cplusplus
extern "C" {
#endif
    
void btstack_memory_init(void);

void * btstack_memory_hci_connection_get(void);
void   btstack_memory_hci_connection_free(void *hci_connection);
void * btstack_memory_l2cap_service_get(void);
void   btstack_memory_l2cap_service_free(void *l2cap_service);
void * btstack_memory_l2cap_channel_get(void);
void   btstack_memory_l2cap_channel_free(void *l2cap_channel);
void * btstack_memory_rfcomm_multiplexer_get(void);
void   btstack_memory_rfcomm_multiplexer_free(void *rfcomm_multiplexer);
void * btstack_memory_rfcomm_service_get(void);
void   btstack_memory_rfcomm_service_free(void *rfcomm_service);
void * btstack_memory_rfcomm_channel_get(void);
void   btstack_memory_rfcomm_channel_free(void *rfcomm_channel);
void * btstack_memory_db_mem_device_name_get(void);
void   btstack_memory_db_mem_device_name_free(void *db_mem_device_name);
void * btstack_memory_db_mem_device_link_key_get(void);
void   btstack_memory_db_mem_device_link_key_free(void *db_mem_device_link_key);
void * btstack_memory_db_mem_service_get(void);
void   btstack_memory_db_mem_service_free(void *db_mem_service);

#if defined __cplusplus
}
#endif
