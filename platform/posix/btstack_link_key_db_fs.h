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

#ifndef __BTSTACK_LINK_KEY_DB_FS_H
#define __BTSTACK_LINK_KEY_DB_FS_H

#include "classic/btstack_link_key_db.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * @brief Get basic link key db implementation that stores link keys in /tmp
 */
const btstack_link_key_db_t * btstack_link_key_db_fs_instance(void);

/**
 * @brief Create human readable represenationt of link key
 * @note uses fixed global buffer
 * @return pointer to Bluetooth address string
 */
char * link_key_to_str(link_key_t link_key);

/**
 * @brief return string for link key type
 * @note uses fixed global buffer
 * @param link_key_type
 * @return link_key_type as string
 */
char *link_key_type_to_str(link_key_type_t link_key_type);

/**
 * @brief Parse link key string
 * @param addr_strings
 * @param link_key buffer for result
 * @return 1 on success
 */
int sscanf_link_key(char * addr_string, link_key_t link_key);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_LINK_KEY_DB_FS_H
