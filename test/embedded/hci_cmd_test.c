#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_cmd.h"

TEST_GROUP(HCI_Command){
    void setup(void){
    }
    void teardown(void){
    }
};


TEST(HCI_Command, test){

}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
