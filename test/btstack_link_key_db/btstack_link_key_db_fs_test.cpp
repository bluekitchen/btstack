#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/btstack_link_key_db.h"
#include "btstack_link_key_db_fs.h"
#include "btstack_util.h"

#include "btstack_config.h"

extern "C" uint32_t btstack_run_loop_get_time_ms(void) { return 0; }

TEST_GROUP(RemoteDeviceDB){
    bd_addr_t bd_addr;
    link_key_t link_key;
    link_key_type_t link_key_type;

    void setup(void){
        bd_addr_t addr_1 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x01 };
        bd_addr_copy(bd_addr, addr_1); 

        link_key_type = (link_key_type_t)4;
        sprintf((char*)link_key, "%d", 100);
    }
    
    void teardown(void){}
};


TEST(RemoteDeviceDB, SinglePutGetDeleteKey){
	link_key_t test_link_key;
    link_key_type_t test_link_key_type;
    
    btstack_link_key_db_fs_instance()->delete_link_key(bd_addr);
    CHECK(btstack_link_key_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 0);
    
	btstack_link_key_db_fs_instance()->put_link_key(bd_addr, link_key, link_key_type);
    CHECK(btstack_link_key_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 1);
    
    CHECK(memcmp(link_key, test_link_key, 16) == 0);
    
    btstack_link_key_db_fs_instance()->delete_link_key(bd_addr);
    CHECK(btstack_link_key_db_fs_instance()->get_link_key(bd_addr, test_link_key, &test_link_key_type) == 0);
}


int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}