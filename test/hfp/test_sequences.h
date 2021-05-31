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
 
// *****************************************************************************
//
// HFP Test Sequences
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined __cplusplus
extern "C" {
#endif

typedef struct hfp_test_item{
    char * name;
    char ** test;
    int len;
} hfp_test_item_t;

/* Codecs Connection (cc) test sequences */
int hfp_cc_tests_size();
hfp_test_item_t * hfp_cc_tests();
hfp_test_item_t * default_hfp_cc_test();

/* PTS test sequences - SLC Group */
int hfp_pts_ag_slc_tests_size();
hfp_test_item_t * hfp_pts_ag_slc_tests();
int hfp_pts_hf_slc_tests_size();
hfp_test_item_t * hfp_pts_hf_slc_tests();

/* PTS test sequences - ATA Group */
int hfp_pts_ag_ata_tests_size();
hfp_test_item_t * hfp_pts_ag_ata_tests();
int hfp_pts_hf_ata_tests_size();
hfp_test_item_t * hfp_pts_hf_ata_tests();

/* PTS test sequences - TWC Group */
int hfp_pts_ag_twc_tests_size();
hfp_test_item_t * hfp_pts_ag_twc_tests();
int hfp_pts_hf_twc_tests_size();
hfp_test_item_t * hfp_pts_hf_twc_tests();

/* PTS test sequences - ECS Group */
int hfp_pts_ag_ecs_tests_size();
hfp_test_item_t * hfp_pts_ag_ecs_tests();
int hfp_pts_hf_ecs_tests_size();
hfp_test_item_t * hfp_pts_hf_ecs_tests();

/* PTS test sequences - ECC Group */
int hfp_pts_ag_ecc_tests_size();
hfp_test_item_t * hfp_pts_ag_ecc_tests();
int hfp_pts_hf_ecc_tests_size();
hfp_test_item_t * hfp_pts_hf_ecc_tests();

/* PTS test sequences - RHH Group */
int hfp_pts_ag_rhh_tests_size();
hfp_test_item_t * hfp_pts_ag_rhh_tests();
int hfp_pts_hf_rhh_tests_size();
hfp_test_item_t * hfp_pts_hf_rhh_tests();

#if defined __cplusplus
}
#endif
