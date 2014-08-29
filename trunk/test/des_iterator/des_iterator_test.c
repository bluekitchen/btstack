
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


static uint8_t  des_test[] = { 
    0x35, 0x1E, 0x35, 0x06, 0x19, 0x01, 0x00, 0x09, 0x00, 0x0F, 0x35, 0x14, 0x19, 0x00, 0x0F, 0x09, 
    0x01, 0x00, 0x35, 0x0C, 0x09, 0x08, 0x00, 0x09, 0x08, 0x06, 0x09, 0x86, 0xDD, 0x09, 0x88, 0x0B
};


TEST_GROUP(DESParser){
    des_iterator_t des_protcol_list_it;
    int iterator_initialized;

    void setup(){
        iterator_initialized = des_iterator_init(&des_protcol_list_it, des_test);
    }
};


TEST(DESParser, DESIterator){
    CHECK_EQUAL(iterator_initialized, 1);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
