#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "remote_device_db.h"
#include "btstack_memory.h"

#include "btstack-config.h"

extern linked_list_t db_mem_link_keys ;
extern linked_list_t db_mem_names ;
// const extern "C" db_mem_device_name_t * btstack_memory_db_mem_device_name_get(void);
// const extern "C" void btstack_memory_init(void);

void dump(linked_list_t list){
    printf("dump:\n");

    int i;
    linked_item_t *it;
    for (it = (linked_item_t *) list, i = 1; it ; it = it->next, i++){
        db_mem_device_t * item = (db_mem_device_t *) it;
        db_mem_device_name_t * item1 = (db_mem_device_name_t *) it;
        db_mem_device_link_key_t * item2 = (db_mem_device_link_key_t *) it;
        printf("%u. %s + %s + %u\n",  i, item1->device_name,  item2->link_key, item->bd_addr[5]);
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
        BD_ADDR_COPY(addr1, addr_1); 
        BD_ADDR_COPY(addr2, addr_2); 
        BD_ADDR_COPY(addr3, addr_3); 
       
        link_key_type = (link_key_type_t)4;
        sprintf((char*)link_key, "%d", 100);
    }
    
    void teardown(void){}
};

TEST(RemoteDeviceDB, MemoryPool){
    CHECK(MAX_NO_DB_MEM_DEVICE_NAMES > 0);
    void * item = btstack_memory_db_mem_device_name_get();
    CHECK(item);
}

TEST(RemoteDeviceDB, SinglePutGetDeleteName){
    sprintf((char*)device_name, "%d", 100);
	remote_device_db_memory.put_name(addr1, &device_name);
    CHECK(remote_device_db_memory.get_name(addr1, &device_name));
    
    remote_device_db_memory.delete_name(addr1);
    CHECK(!remote_device_db_memory.get_name(addr1, &device_name));
}

TEST(RemoteDeviceDB, SortByLastUsedName){
	remote_device_db_memory.put_name(addr1, (device_name_t*) "10");
    // dump(db_mem_names);
	remote_device_db_memory.put_name(addr2, (device_name_t*) "20");
    // dump(db_mem_names);
	remote_device_db_memory.put_name(addr3, (device_name_t*) "30");
    // dump(db_mem_names);
    
    CHECK(!remote_device_db_memory.get_name(addr1, &device_name));
    CHECK(remote_device_db_memory.get_name(addr2, &device_name));
    //get first element of the list
    db_mem_device_name_t * item = (db_mem_device_name_t *) db_mem_names;
    STRCMP_EQUAL((char*)item->device_name, "20"); 
}


TEST(RemoteDeviceDB, SinglePutGetDeleteKey){
	sprintf((char*)link_key, "%d", 100);
	remote_device_db_memory.put_link_key(addr1, link_key, link_key_type);
    // dump(db_mem_link_keys);

	CHECK(remote_device_db_memory.get_link_key(addr1, link_key, &link_key_type));
    
    remote_device_db_memory.delete_link_key(addr1);
    CHECK(!remote_device_db_memory.get_link_key(addr1, link_key, &link_key_type));
}

TEST(RemoteDeviceDB, SortByLastUsedKey){
    sprintf((char*)link_key, "%d", 10);
	remote_device_db_memory.put_link_key(addr1, link_key, link_key_type);
    // dump(db_mem_link_keys);
    sprintf((char*)link_key, "%d", 20);
	remote_device_db_memory.put_link_key(addr2, link_key, link_key_type);
    // dump(db_mem_link_keys);
    sprintf((char*)link_key, "%d", 30);
	remote_device_db_memory.put_link_key(addr3, link_key, link_key_type);
    // dump(db_mem_link_keys);

    CHECK(!remote_device_db_memory.get_link_key(addr1, link_key, &link_key_type));
    CHECK(remote_device_db_memory.get_link_key(addr2, link_key, &link_key_type));
    // dump(db_mem_link_keys);
    
    //get first element of the list
    db_mem_device_link_key_t * item = (db_mem_device_link_key_t *) db_mem_link_keys;
    STRCMP_EQUAL((char*)item->link_key, "20"); 
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}