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
 *  btstack_memory_ucos.c
 *
 *  @brief BTstack memory management via configurable memory pools
 *
 *  Created by Albis Technologies.
 */

#include <btstack/port_ucos.h>

#include "../config.h"
#include "btstack_memory.h"

#include "hci.h"
#include "l2cap.h"
#include "rfcomm.h"

DEFINE_THIS_FILE

/*=============================================================================
=============================================================================*/
static MEM_POOL hci_connection_pool;
void * btstack_memory_hci_connection_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&hci_connection_pool,
                          sizeof(hci_connection_t),
                          &err);
}
void btstack_memory_hci_connection_free(void *hci_connection)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&hci_connection_pool, hci_connection, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL l2cap_service_pool;
void * btstack_memory_l2cap_service_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&l2cap_service_pool,
                          sizeof(l2cap_service_t),
                          &err);
}
void btstack_memory_l2cap_service_free(void *l2cap_service)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&l2cap_service_pool, l2cap_service, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL l2cap_channel_pool;
void * btstack_memory_l2cap_channel_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&l2cap_channel_pool,
                          sizeof(l2cap_channel_t),
                          &err);
}
void btstack_memory_l2cap_channel_free(void *l2cap_channel)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&l2cap_channel_pool, l2cap_channel, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL rfcomm_multiplexer_pool;
void * btstack_memory_rfcomm_multiplexer_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&rfcomm_multiplexer_pool,
                          sizeof(rfcomm_multiplexer_t),
                          &err);
}
void btstack_memory_rfcomm_multiplexer_free(void *rfcomm_multiplexer)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&rfcomm_multiplexer_pool, rfcomm_multiplexer, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL rfcomm_service_pool;
void * btstack_memory_rfcomm_service_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&rfcomm_service_pool,
                          sizeof(rfcomm_service_t),
                          &err);
}
void btstack_memory_rfcomm_service_free(void *rfcomm_service)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&rfcomm_service_pool, rfcomm_service, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL rfcomm_channel_pool;
void * btstack_memory_rfcomm_channel_get(void)
{
    LIB_ERR err;
    return Mem_PoolBlkGet(&rfcomm_channel_pool,
                          sizeof(rfcomm_channel_t),
                          &err);
}
void btstack_memory_rfcomm_channel_free(void *rfcomm_channel)
{
    LIB_ERR err;
    Mem_PoolBlkFree(&rfcomm_channel_pool, rfcomm_channel, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL db_mem_device_pool;
void * btstack_memory_db_mem_device_get(void){
    LIB_ERR err;
    return Mem_PoolBlkGet(&db_mem_device_pool,
                          sizeof(db_mem_device_t),
                          &err);
}
void btstack_memory_db_mem_device_free(void *db_mem_device){
    LIB_ERR err;
    Mem_PoolBlkFree(&db_mem_device_pool, db_mem_device, &err);
}

/*=============================================================================
=============================================================================*/
static MEM_POOL db_mem_service_pool;
void * btstack_memory_db_mem_service_get(void){
    LIB_ERR err;
    return Mem_PoolBlkGet(&db_mem_service_pool,
                          sizeof(db_mem_service_t),
                          &err);
}
void btstack_memory_db_mem_service_free(void *db_mem_service){
    LIB_ERR err;
    Mem_PoolBlkFree(&db_mem_service_pool, db_mem_service, &err);
}

/*=============================================================================
=============================================================================*/
void btstack_memory_init(void)
{
    LIB_ERR err;
    CPU_SIZE_T octetsReqd;

    Mem_PoolCreate(&hci_connection_pool,
                   0,
                   MAX_NO_HCI_CONNECTIONS * sizeof(hci_connection_t),
                   MAX_NO_HCI_CONNECTIONS,
                   sizeof(hci_connection_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&l2cap_service_pool,
                   0,
                   MAX_NO_L2CAP_SERVICES * sizeof(l2cap_service_t),
                   MAX_NO_L2CAP_SERVICES,
                   sizeof(l2cap_service_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&l2cap_channel_pool,
                   0,
                   MAX_NO_L2CAP_CHANNELS * sizeof(l2cap_channel_t),
                   MAX_NO_L2CAP_CHANNELS,
                   sizeof(l2cap_channel_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&rfcomm_multiplexer_pool,
                   0,
                   MAX_NO_RFCOMM_MULTIPLEXERS * sizeof(rfcomm_multiplexer_t),
                   MAX_NO_RFCOMM_MULTIPLEXERS,
                   sizeof(rfcomm_multiplexer_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&rfcomm_service_pool,
                   0,
                   MAX_NO_RFCOMM_SERVICES * sizeof(rfcomm_service_t),
                   MAX_NO_RFCOMM_SERVICES,
                   sizeof(rfcomm_service_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&rfcomm_channel_pool,
                   0,
                   MAX_NO_RFCOMM_CHANNELS * sizeof(rfcomm_channel_t),
                   MAX_NO_RFCOMM_CHANNELS,
                   sizeof(rfcomm_channel_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&db_mem_device_pool,
                   0,
                   MAX_NO_DB_MEM_DEVICES * sizeof(db_mem_device_t),
                   MAX_NO_DB_MEM_DEVICES,
                   sizeof(db_mem_device_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);

    Mem_PoolCreate(&db_mem_service_pool,
                   0,
                   MAX_NO_DB_MEM_SERVICES * sizeof(db_mem_service_t),
                   MAX_NO_DB_MEM_SERVICES,
                   sizeof(db_mem_service_t),
                   sizeof(unsigned long),
                   &octetsReqd,
                   &err);
    SYS_ASSERT(err == LIB_MEM_ERR_NONE);
}
