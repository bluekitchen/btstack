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

#include "test_sequences.h"


#define TEST_SEQUENCE(test_sequence) { (char *)#test_sequence, (char**)test_sequence, sizeof(test_sequence) / sizeof(char *)}

/* Service Level Connection (slc) test sequences */

// with codec negotiation feature
const char * slc_test1[] = {
    "AT+BRSF=127",
    "+BRSF:4079", 
    "OK",
    "AT+BAC=1,2", 
    "OK",
    "AT+CIND=?",
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))",
    "OK",
    "AT+CIND?",
    "+CIND:1,0,0,3,5,0,0",
    "OK",
    "AT+CMER=3,0,0,1",
    "OK",
    "AT+CHLD=?",
    "+CHLD:(1,1x,2,2x,3)",
    "OK"
};

// without codec negotiation feature
const char * slc_test2[] = {
    "AT+BRSF=438",
    "+BRSF:495", 
    "OK",
    "AT+CIND=?",
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))",
    "OK",
    "AT+CIND?",
    "+CIND:1,0,0,3,5,0,0",
    "OK",
    "AT+CMER=3,0,0,1",
    "OK",
    "AT+CHLD=?",
    "+CHLD:(1,1x,2,2x,3)",
    "OK"
};


hfp_test_item_t slc_tests[] = {
    TEST_SEQUENCE(slc_test1),
    TEST_SEQUENCE(slc_test2)
};

/* Service Level Connection (slc) common commands */
const char * slc_cmds_test1[] = {
    "AT+BAC=1,3", 
    "OK"
};

hfp_test_item_t slc_cmds_tests[] = {
    TEST_SEQUENCE(slc_cmds_test1)
};

/* Codecs Connection (cc) test sequences */
const char * cc_test1[] = {
    "AT+BCC", 
    "OK",
    "+BCS:1",
    "AT+BCS=1",
    "OK"
};

const char * cc_test2[] = {
    "AT+BAC=1,2", 
    "OK",
    "AT+BCC",
    "OK",
    "BCS:1",
    "AT+BCS=1",
    "OK"
};


const char * cc_test3[] = {
    "AT+BAC=1,2", 
    "OK",
    "AT+BCC",
    "OK",
    "+BCS:1",
    "AT+BAC=2,3", 
    "OK",
    "+BCS:3",
    "AT+BCS=3",
    "OK"
};

const char * cc_test4[] = {
    "AT+BCC", 
    "OK",
    "+BCS:1",
    "AT+BAC=2,3", 
    "OK",
    "+BCS:3",
    "AT+BCS=3",
    "OK"
};


hfp_test_item_t cc_tests[] = {
    TEST_SEQUENCE(cc_test1),
    TEST_SEQUENCE(cc_test2),
    TEST_SEQUENCE(cc_test3),
    TEST_SEQUENCE(cc_test4)
};

/* Incoming call sequence */
const char * ic_test1[] = {
    "+CIEV:3,1",
    "BCS:1",
    "AT+BCS=1",
    "OK"
};

const char * ic_alert_test1[] = {
    "ATA",
    "OK",
    "+CIEV:2,1", // call = 1
    "+CIEV:3,0", 
};

const char * ic_ag_terminates_call[] = {
    // AG terminates call
    "+CIEV:2,0"  
};

const char * ic_hf_terminates_call[] = {
    // HF terminates call
    "AT+CHUP",
    "OK",
    "+CIEV:2,0"  
};

hfp_test_item_t ic_tests[] = {
    TEST_SEQUENCE(ic_test1)
};

// PTS test sequences

const char * TC_AG_SLC_BV_01_C[] = {
    "AT+BRSF=127" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_02_C[] = {
    "AT+BRSF=127" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_03_C[] = {
    "AT+BRSF=125" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_04_C[] = {
    "AT+BRSF=125" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_05_I[] = {
    "AT+BRSF=255" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+BAC=1" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_06_I[] = {
    "AT+BRSF=255" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+BAC=1" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_07_I[] = {
    "AT+BRSF=127" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_09_I[] = {
    "AT+BRSF=895" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+BIND=1,99" ,
    "OK" ,
    "AT+BIND=?" ,
    "+BIND:(1,2,)" ,
    "OK" ,
    "AT+BIND?" ,
    "+BIND:1,1",
    "+BIND:2,1" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

const char * TC_AG_SLC_BV_10_I[] = {
    "AT+BRSF=127" ,
    "+BRSF:4079" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND:(\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0,3)),(\"battchg\",(0,5)),(\"signal\",(0,5)),(\"roam\",(0,1)),(\"callheld\",(0,2))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND:1,0,0,3,5,0,0" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD:(1,1x,2,2x,3)" ,
    "OK" ,
    "AT+VGS=9" ,
    "OK" ,
    "AT+VGM=9" ,
    "OK" ,
    "AT+CLIP=1" ,
    "OK" ,
    "AT+CCWA=1" ,
    "OK" ,
    "AT+CMEE=1" ,
    "OK"
};

hfp_test_item_t pts_ag_slc_tests[] = {
    TEST_SEQUENCE(TC_AG_SLC_BV_01_C),
    TEST_SEQUENCE(TC_AG_SLC_BV_02_C),
    TEST_SEQUENCE(TC_AG_SLC_BV_03_C),
    TEST_SEQUENCE(TC_AG_SLC_BV_04_C),
    TEST_SEQUENCE(TC_AG_SLC_BV_05_I),
    TEST_SEQUENCE(TC_AG_SLC_BV_06_I),
    TEST_SEQUENCE(TC_AG_SLC_BV_07_I),
    TEST_SEQUENCE(TC_AG_SLC_BV_09_I),
    TEST_SEQUENCE(TC_AG_SLC_BV_10_I)
};


const char * TC_HF_SLC_BV_01_C[] = {
    "AT+BRSF=951" ,
    "+BRSF: 511" ,
    "OK" ,
    "AT+CIND=?" ,
    "+CIND: (\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0-3)),(\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0-1)),(\"battchg\",(0-5))" ,
    "OK" ,
    "AT+CIND?" ,
    "+CIND: 1,0,0,0,5,0,5" ,
    "OK" ,
    "AT+CMER=3,0,0,1" ,
    "OK" ,
    "AT+CHLD=?" ,
    "+CHLD: (0,1,1x,2,2x,3,4)" ,
    "OK" ,
    "AT+VGM=9" ,
    "+BSIR: 0" ,
    "AT+VGS=9" ,
    "OK" ,
    "OK"
};


hfp_test_item_t pts_hf_slc_tests[] = {
    TEST_SEQUENCE(TC_HF_SLC_BV_01_C),
//    TEST_SEQUENCE(TC_HF_SLC_BV_02_C),
//    TEST_SEQUENCE(TC_HF_SLC_BV_03_C),
//    TEST_SEQUENCE(TC_HF_SLC_BV_04_C),
//    TEST_SEQUENCE(TC_HF_SLC_BV_05_I),
//    TEST_SEQUENCE(TC_HF_SLC_BV_06_I),
//    TEST_SEQUENCE(TC_HF_SLC_BV_07_I),
//    TEST_SEQUENCE(TC_HF_SLC_BV_09_I),
//    TEST_SEQUENCE(TC_HF_SLC_BV_10_I),
};

//////////////

static int test_item_size = sizeof(hfp_test_item_t);

// SLC
int hfp_slc_tests_size(){ return sizeof(slc_tests)/test_item_size;}
hfp_test_item_t * hfp_slc_tests(){ return slc_tests;}
hfp_test_item_t * default_hfp_slc_test(){return  &slc_tests[0];}

// SLC commands
int hfp_slc_cmds_tests_size(){ return sizeof(slc_cmds_tests)/test_item_size;}
hfp_test_item_t * hfp_slc_cmds_tests(){ return slc_cmds_tests;}
hfp_test_item_t * default_slc_cmds_test() { return  &slc_tests[0];}

// CC
int hfp_cc_tests_size(){ return sizeof(cc_tests) /test_item_size;}
hfp_test_item_t * hfp_cc_tests(){ return cc_tests;}
hfp_test_item_t * default_hfp_cc_test(){ return &cc_tests[0];}

// PTS
int hfp_pts_ag_slc_tests_size(){ return sizeof(pts_ag_slc_tests)/test_item_size;}
hfp_test_item_t * hfp_pts_ag_slc_tests(){ return pts_ag_slc_tests;}

int hfp_pts_hf_slc_tests_size(){ return sizeof(pts_hf_slc_tests)/test_item_size;}
hfp_test_item_t * hfp_pts_hf_slc_tests(){ return pts_hf_slc_tests;}

 