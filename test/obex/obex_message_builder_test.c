#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_util.h"
#include "btstack_debug.h"


TEST_GROUP(OBEX_MESSAGE_BUILDER){
    void setup(void){
    }
};

TEST(OBEX_MESSAGE_BUILDER, Put){
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}