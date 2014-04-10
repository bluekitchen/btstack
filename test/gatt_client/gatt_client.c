
//*****************************************************************************
//
// test rfcomm query tests
//
//*****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include <btstack/hci_cmds.h>

#include "btstack_memory.h"
#include "hci.h"

// #include "hci_dump.h"
// #include "l2cap.h"
// #include "sdp_parser.h"

TEST_GROUP(ADParser){
};


TEST(ADParser, TestDataParsing){
}

TEST(ADParser, TestHasUUID){

}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
