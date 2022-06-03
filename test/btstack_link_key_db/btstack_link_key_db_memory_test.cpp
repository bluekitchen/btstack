#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "classic/btstack_link_key_db.h"
#include "btstack_memory.h"

#include "btstack_config.h"

extern btstack_linked_list_t db_mem_link_keys ;
extern btstack_linked_list_t db_mem_names ;
// const extern "C" db_mem_device_name_t * btstack_memory_db_mem_device_name_get(void);
// const extern "C" void btstack_memory_init(void);

extern "C" uint32_t btstack_run_loop_get_time_ms(void) { return 0; }

void dump(btstack_linked_list_t list){
    printf("dump:\n");

    int i;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) list, i = 1; it ; it = it->next, i++){
        // btstack_link_key_db_memory_entry_t * item = (btstack_link_key_db_memory_entry_t *) it;
        // printf("%u.  %s + %u\n",  i, item->link_key, item->bd_addr[5]);
        // TODO printf broken
    }
}


TEST_GROUP(RemoteDeviceDB){
    bd_addr_t addr1, addr2, addr3;
    device_name_t device_name;
    link_key_t link_key;
    link_key_type_t link_key_type;

    void setup(void){
        btstack_memory_init();

        bd_addr_t addr_1 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x01 };
        bd_addr_t addr_2 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x02 };
        bd_addr_t addr_3 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x03 };
        bd_addr_copy(addr1, addr_1); 
        bd_addr_copy(addr2, addr_2); 
        bd_addr_copy(addr3, addr_3); 
       
        link_key_type = (link_key_type_t)4;
        sprintf((char*)link_key, "%d", 100);
    }
    
    void teardown(void){}
};

TEST(RemoteDeviceDB, MemoryPool){
    CHECK(MAX_NR_BTSTACK_LINK_KEY_DB_MEMORY_ENTRIES ==  2);
    // void * item = btstack_memory_db_mem_device_name_get();
    // CHECK(item);
}

// TEST(RemoteDeviceDB, SinglePutGetDeleteName){
//     sprintf((char*)device_name, "%d", 100);
// 	btstack_link_key_db_memory_instance()->put_name(addr1, &device_name);
//     CHECK(btstack_link_key_db_memory_instance()->get_name(addr1, &device_name));
    
//     btstack_link_key_db_memory_instance()->delete_name(addr1);
//     CHECK(!btstack_link_key_db_memory_instance()->get_name(addr1, &device_name));
// }

// TEST(RemoteDeviceDB, SortByLastUsedName){
// 	btstack_link_key_db_memory_instance()->put_name(addr1, (device_name_t*) "10");
//     // dump(db_mem_names);
// 	btstack_link_key_db_memory_instance()->put_name(addr2, (device_name_t*) "20");
//     // dump(db_mem_names);
// 	btstack_link_key_db_memory_instance()->put_name(addr3, (device_name_t*) "30");
//     // dump(db_mem_names);
    
//     CHECK(!btstack_link_key_db_memory_instance()->get_name(addr1, &device_name));
//     CHECK(btstack_link_key_db_memory_instance()->get_name(addr2, &device_name));
//     //get first element of the list
//     db_mem_device_name_t * item = (db_mem_device_name_t *) db_mem_names;
//     STRCMP_EQUAL((char*)item->device_name, "20"); 
// }


TEST(RemoteDeviceDB, SinglePutGetDeleteKey){
	sprintf((char*)link_key, "%d", 100);
	btstack_link_key_db_memory_instance()->put_link_key(addr1, link_key, link_key_type);
    // dump(db_mem_link_keys);

	CHECK(btstack_link_key_db_memory_instance()->get_link_key(addr1, link_key, &link_key_type));
    
    btstack_link_key_db_memory_instance()->delete_link_key(addr1);
    CHECK(!btstack_link_key_db_memory_instance()->get_link_key(addr1, link_key, &link_key_type));
}

TEST(RemoteDeviceDB, SortByLastUsedKey){
    sprintf((char*)link_key, "%d", 10);
	btstack_link_key_db_memory_instance()->put_link_key(addr1, link_key, link_key_type);
    // dump(db_mem_link_keys);
    sprintf((char*)link_key, "%d", 20);
	btstack_link_key_db_memory_instance()->put_link_key(addr2, link_key, link_key_type);
    // dump(db_mem_link_keys);
    sprintf((char*)link_key, "%d", 30);
	btstack_link_key_db_memory_instance()->put_link_key(addr3, link_key, link_key_type);
    // dump(db_mem_link_keys);

    CHECK(!btstack_link_key_db_memory_instance()->get_link_key(addr1, link_key, &link_key_type));
    CHECK(btstack_link_key_db_memory_instance()->get_link_key(addr2, link_key, &link_key_type));
    // dump(db_mem_link_keys);
    
    //get first element of the list
    btstack_link_key_db_memory_entry_t * item = (btstack_link_key_db_memory_entry_t *) db_mem_link_keys;
    STRCMP_EQUAL((char*)item->link_key, "20"); 
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}