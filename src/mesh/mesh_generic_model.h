/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#ifndef __MESH_GENERIC_MODEL_H
#define __MESH_GENERIC_MODEL_H

#include "mesh/mesh_access.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define MESH_GENERIC_ON_OFF_GET                                     0x8201u    
#define MESH_GENERIC_ON_OFF_SET                                     0x8202u
#define MESH_GENERIC_ON_OFF_SET_UNACKNOWLEDGED                      0x8203u
#define MESH_GENERIC_ON_OFF_STATUS                                  0x8204u

#define MESH_GENERIC_DEFAULT_TRANSITION_TIME_GET                    0x820Du    
#define MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET                    0x820Eu
#define MESH_GENERIC_DEFAULT_TRANSITION_TIME_SET_UNACKNOWLEDGED     0x820Fu
#define MESH_GENERIC_DEFAULT_TRANSITION_TIME_STATUS                 0x8210u

#define MESH_GENERIC_LEVEL_GET                                      0x8205u    
#define MESH_GENERIC_LEVEL_SET                                      0x8206u
#define MESH_GENERIC_LEVEL_SET_UNACKNOWLEDGED                       0x8207u
#define MESH_GENERIC_LEVEL_STATUS                                   0x8208u
#define MESH_GENERIC_DELTA_SET                                      0x8209u
#define MESH_GENERIC_DELTA_SET_UNACKNOWLEDGED                       0x820Au
#define MESH_GENERIC_MOVE_SET                                       0x820Bu
#define MESH_GENERIC_MOVE_SET_UNACKNOWLEDGED                        0x820Cu
 
typedef struct {
    mesh_transition_t base_transition;

    uint8_t  current_value;
    uint8_t  target_value;
} mesh_transition_bool_t;

typedef struct {
    mesh_transition_t base_transition;

    int16_t current_value;
    int16_t initial_value;
    int16_t target_value;
    int16_t stepwise_value_increment;
} mesh_transition_int16_t;

typedef struct {
    mesh_transition_bool_t transition_data;          
} mesh_generic_on_off_state_t;

typedef struct {
    uint8_t  value;          
} mesh_generic_default_transition_time_state_t;

typedef struct {
    mesh_transition_int16_t transition_data;       
} mesh_generic_level_state_t;

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
