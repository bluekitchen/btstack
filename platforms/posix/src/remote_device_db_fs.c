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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "remote_device_db.h"
#include "debug.h"

#include <btstack/utils.h>

#define LINK_KEY_PATH "/tmp/"
#define LINK_KEY_PREFIX "btstack_link_key_"
#define LINK_KEY_SUFIX ".txt"

static char keypath[sizeof(LINK_KEY_PATH) + sizeof(LINK_KEY_PREFIX) + 17 + sizeof(LINK_KEY_SUFIX) + 1];

static char bd_addr_to_dash_str_buffer[6*3];  // 12-45-78-01-34-67\0
static char * bd_addr_to_dash_str(bd_addr_t addr){
    char * p = bd_addr_to_dash_str_buffer;
    int i;
    for (i = 0; i < 6 ; i++) {
        *p++ = char_for_nibble((addr[i] >> 4) & 0x0F);
        *p++ = char_for_nibble((addr[i] >> 0) & 0x0F);
        *p++ = '-';
    }
    *--p = 0;
    return (char *) bd_addr_to_dash_str_buffer;
}

static void set_path(bd_addr_t bd_addr){
    strcpy(keypath, LINK_KEY_PATH);
    strcat(keypath, LINK_KEY_PREFIX);
    strcat(keypath, bd_addr_to_dash_str(bd_addr));
    strcat(keypath, LINK_KEY_SUFIX);
}

// Device info
static void db_open(void){
}

static void db_close(void){ 
}

static void put_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t link_key_type){
    set_path(bd_addr);
    char * link_key_str = link_key_to_str(link_key);
    char * link_key_type_str = link_key_type_to_str(link_key_type); 

    FILE * wFile = fopen(keypath,"w+");
    fwrite(link_key_str, strlen(link_key_str), 1, wFile);
    fwrite(link_key_type_str, strlen(link_key_type_str), 1, wFile);
    fclose(wFile);
}

static int get_link_key(bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * link_key_type) {
    set_path(bd_addr);
    if (access(keypath, R_OK)) return 0;
    
    char link_key_str[LINK_KEY_STR_LEN + 1];
    char link_key_type_str[2];

    FILE * rFile = fopen(keypath,"r+");
    size_t objects_read = fread(link_key_str, LINK_KEY_STR_LEN, 1, rFile );
    if (objects_read == 1){
        link_key_str[LINK_KEY_STR_LEN] = 0;
        log_info("Found link key %s\n", link_key_str);
        objects_read = fread(link_key_type_str, 1, 1, rFile );
    }
    fclose(rFile);
    
    if (objects_read != 1) return 0;
    link_key_type_str[1] = 0;
    log_info("Found link key type %s\n", link_key_type_str);
    
    int scan_result = sscan_link_key(link_key_str, link_key);
    if (scan_result == 0 ) return 0;

    int link_key_type_buffer;
    scan_result = sscanf( (char *) link_key_type_str, "%d", &link_key_type_buffer);
    if (scan_result == 0 ) return 0;
    *link_key_type = (link_key_type_t) link_key_type_buffer;
    return 1;
}

static void delete_link_key(bd_addr_t bd_addr){
    set_path(bd_addr);
    if (access(keypath, R_OK)) return;

    if(remove(keypath) != 0){
        log_error("File %s could not be deleted.\n", keypath);
    }
}

static int get_name(bd_addr_t bd_addr, device_name_t *device_name) {
    return 0;
}

static void delete_name(bd_addr_t bd_addr){
}

static void put_name(bd_addr_t bd_addr, device_name_t *device_name){
}

const remote_device_db_t remote_device_db_fs = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name
};

const remote_device_db_t * remote_device_db_fs_instance(void){
    return &remote_device_db_fs;
}
