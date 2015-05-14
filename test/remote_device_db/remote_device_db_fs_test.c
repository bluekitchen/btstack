#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "remote_device_db.h"

#include "btstack-config.h"

const remote_device_db_t * remote_device_db_fs_instance();

TEST_GROUP(RemoteDeviceDB){
    bd_addr_t bd_addr;
    link_key_t link_key;
    link_key_type_t link_key_type;

    void setup(void){
        bd_addr_t addr_1 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x01 };
        BD_ADDR_COPY(bd_addr, addr_1); 

        link_key_type = (link_key_type_t)4;
        sprintf((char*)link_key, "%d", 100);
    }
    
    void teardown(void){}
};


TEST(RemoteDeviceDB, SinglePutGetDeleteKey){
	link_key_t test_link_key;
    link_key_type_t test_link_key_type;
    
    remote_device_db_fs_instance()->delete_link_key(bd_addr);
    CHECK(remote_device_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 0);
    
	remote_device_db_fs_instance()->put_link_key(bd_addr, link_key, link_key_type);
    CHECK(remote_device_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 1);
    
    CHECK(strcmp(link_key_to_str(link_key), link_key_to_str(test_link_key)) == 0);
    
    remote_device_db_fs_instance()->delete_link_key(bd_addr);
    CHECK(remote_device_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 0);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}