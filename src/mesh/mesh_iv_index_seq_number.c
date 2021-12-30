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

#define BTSTACK_FILE__ "mesh_iv_index_seq_number.c"

#include "mesh/mesh_iv_index_seq_number.h"

#include "btstack_debug.h"

static uint32_t global_iv_index;
static int global_iv_update_active;

static uint32_t  sequence_number_current;

void mesh_set_iv_index(uint32_t iv_index){
    global_iv_index = iv_index;
}

uint32_t mesh_get_iv_index(void){
    return  global_iv_index;
}

uint32_t mesh_get_iv_index_for_tx(void){
    if (global_iv_update_active){
        return global_iv_index - 1;
    } else {
        return  global_iv_index;
    }
}

int mesh_iv_update_active(void){
    return global_iv_update_active;
}

void mesh_trigger_iv_update(void){
    if (global_iv_update_active) return;

    // "A node shall not start an IV Update procedure more often than once every 192 hours."
    // Unless triggered by user application, it will automatically triggered if sequene numbers are about to roll over

    // "A node shall defer state change from IV Update in Progress to Normal Operation, as defined by this procedure,
    //  when the node has transmitted a Segmented Access message or a Segmented Control message without receiving the
    //  corresponding Segment Acknowledgment messages. The deferred change of the state shall be executed when the appropriate
    //  Segment Acknowledgment message is received or timeout for the delivery of this message is reached.
    //  
    //  Note: This requirement is necessary because upon completing the IV Update procedure the sequence number is reset
    //  to 0x000000 and the SeqAuth value would not be valid."

    // set IV Update in Progress
    global_iv_update_active = 1;
    // increase IV index
    global_iv_index++;
}

void mesh_iv_update_completed(void){
    if (!global_iv_update_active) return;
    // set Normal mode
    global_iv_update_active = 0;
}

void mesh_iv_index_recovered(uint8_t iv_update_active, uint32_t iv_index){
    log_info("mesh_iv_index_recovered: active %u, index %u", iv_update_active, (int) iv_index);
    global_iv_index = iv_index;
    global_iv_update_active = iv_update_active;
}

//

static void (*seq_num_callback)(void);

void mesh_sequence_number_set_update_callback(void (*callback)(void)){
	seq_num_callback = callback;
}

void mesh_sequence_number_set(uint32_t seq){
    sequence_number_current = seq;
}

uint32_t mesh_sequence_number_next(void){
	uint32_t seq_number = sequence_number_current++;

	if (seq_num_callback){
		(*seq_num_callback)();
	}

    return seq_number;
}

uint32_t mesh_sequence_number_peek(void){
    return sequence_number_current;
}


