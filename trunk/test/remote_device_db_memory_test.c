#include <stdio.h>
#include <string.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "remote_device_db.h"

TEST_GROUP(RemoteDeviceDB){
    bd_addr_t addr;
    device_name_t device_name;
    link_key_t link_key;
    
    void setup(){
        bd_addr_t addr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
        strcpy((char*)device_name, "matthias");
        strcpy((char*)link_key, "matthias");
    }
    
    void teardown(){}
};

TEST(RemoteDeviceDB, PutGetDeleteName){
    remote_device_db_memory.put_name(&addr, &device_name);
    CHECK(remote_device_db_memory.get_name(&addr, &device_name));
    
    remote_device_db_memory.delete_name(&addr);
    CHECK(!remote_device_db_memory.get_name(&addr, &device_name));
}

TEST(RemoteDeviceDB, PutGetDeleteKey){
	remote_device_db_memory.put_link_key(&addr, &link_key);
    CHECK(remote_device_db_memory.get_link_key(&addr, &link_key));
        
    remote_device_db_memory.delete_link_key(&addr);
    CHECK(!remote_device_db_memory.get_link_key(&addr, &link_key));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}