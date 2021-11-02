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

/**
 * @title Link Key DB
 * 
 * Interface to provide link key storage.
 *
 */

#ifndef BTSTACK_LINK_KEY_DB_H
#define BTSTACK_LINK_KEY_DB_H

#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	void * context;
} btstack_link_key_iterator_t;

/* API_START */
typedef struct {

    // management

    /**
     * @brief Open the Link Key DB
     */
    void (*open)(void);

    /**
     * @brief Sets BD Addr of local Bluetooth Controller.
     * @note Only needed if Bluetooth Controller can be swapped, e.g. USB Dongles on desktop systems
     */
    void (*set_local_bd_addr)(bd_addr_t bd_addr);

    /**
     * @brief Close the Link Key DB
     */
    void (*close)(void);
    
    // get/set/delete link key

    /**
     * @brief Get Link Key for given address
     * @param addr to lookup
     * @param link_key (out)
     * @param type (out)
     * @return 1 on success
     */
    int  (*get_link_key)(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * type);

    /**
     * @brief Update/Store Link key
     * @brief addr
     * @brief link_key
     * @brief type of link key
     */
    void (*put_link_key)(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t type);

    /**
     * @brief Delete Link Keys
     * @brief addr
     */
    void (*delete_link_key)(bd_addr_t bd_addr);

    // iterator: it's allowed to delete 

    /**
     * @brief Setup iterator
     * @param it
     * @return 1 on success
     */
    int (*iterator_init)(btstack_link_key_iterator_t * it);

    /**
     * @brief Get next Link Key
     * @param it
     * @brief addr
     * @brief link_key
     * @brief type of link key
     * @return 1, if valid link key found
     */
    int  (*iterator_get_next)(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * type);

    /**
     * @brief Frees resources allocated by iterator_init
     * @note Must be called after iteration to free resources
     * @param it
     */
    void (*iterator_done)(btstack_link_key_iterator_t * it);

} btstack_link_key_db_t;

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_LINK_KEY_DB_H
