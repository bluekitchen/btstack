
//*****************************************************************************
//
// des iterator tests
//
//*****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "des_iterator.h"
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"


static uint8_t  prot_des_list_des[] = { 
    0x35, 0x1E, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x0F, 0x35, 0x14, 0x19, 0x00, 0x0F, 0x09, 
    0x01, 0x00, 0x35, 0x0C, 0x09, 0x08, 0x00, 0x09, 0x08, 0x06, 0x09, 0x86, 0xDD, 0x09, 0x88, 0x0B
};

static uint16_t expected_values[] = {
    0x0100, 0xf, 0x000f, 0x0100, 0x800, 0x806, 0x86dd, 0x880b
};



TEST_GROUP(DESParser){
    int value_index;

    void setup(){
        value_index = 0;
    }
    
    void CHECK_EQUAL_UINT16(des_iterator_t *it){
        uint16_t value;
        uint8_t * element = des_iterator_get_element(it);
        if (des_iterator_get_type(it) == DE_UUID){
            value = de_element_get_uuid16(element);
        } else {
            value = 0xffff;
            de_element_get_uint16(element, &value);
        }
        CHECK_EQUAL(expected_values[value_index], value);
        value_index++;
    }
};

// DES { DES{0x0100, 0xf}, DES{0x000f, 0x0100, DES{0x800, 0x806, 0x86dd, 0x880b}}}
TEST(DESParser, DESIterator){
    des_iterator_t des_protocol_list_it;
    des_iterator_t prot_it;
    des_iterator_t packet_it;
    int iterator_initialized;
    uint8_t * prot_des;
    uint8_t * packet_des;
    // printf("\n *** protocol list\n");
    iterator_initialized = des_iterator_init(&des_protocol_list_it, prot_des_list_des);
    CHECK_EQUAL(iterator_initialized, 1);
    CHECK_EQUAL(des_iterator_get_type(&des_protocol_list_it), DE_DES);
    
    // printf("\n *** l2cap uuid, psm\n");
    prot_des = des_iterator_get_element(&des_protocol_list_it);
    for (des_iterator_init(&prot_it, prot_des) ; des_iterator_has_more(&prot_it) ; des_iterator_next(&prot_it) ){
        CHECK_EQUAL_UINT16(&prot_it);
    }
    
    des_iterator_next(&des_protocol_list_it);
    CHECK_EQUAL(des_iterator_get_type(&des_protocol_list_it), DE_DES);

    // printf("\n *** bnep uuid, value\n");
    prot_des = des_iterator_get_element(&des_protocol_list_it);

    for (des_iterator_init(&prot_it, prot_des) ; des_iterator_has_more(&prot_it) ; des_iterator_next(&prot_it)){
        if (des_iterator_get_type(&prot_it) != DE_DES){
            CHECK_EQUAL_UINT16(&prot_it);
            continue;
        }

        packet_des = des_iterator_get_element(&prot_it);
        for (des_iterator_init(&packet_it, packet_des) ; des_iterator_has_more(&packet_it) ; des_iterator_next(&packet_it)){
            CHECK_EQUAL_UINT16(&packet_it);
        }
    }
    des_iterator_next(&des_protocol_list_it);
    CHECK_EQUAL(value_index, 8);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
