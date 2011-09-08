#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "remote_device_db.h"

extern linked_list_t db_mem_link_keys ;
extern linked_list_t db_mem_names ;

TEST_GROUP(RemoteDeviceDB){
    bd_addr_t addr1, addr2, addr3;
    device_name_t device_name;
    link_key_t link_key;
    
    void setup(){
        bd_addr_t addr1 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x01 };
        bd_addr_t addr2 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x02 };
        bd_addr_t addr3 = {0x00, 0x01, 0x02, 0x03, 0x04, 0x03 };
    }
    
    void teardown(){}
};

TEST(RemoteDeviceDB, SinglePutGetDeleteName){
    sprintf((char*)device_name, "%d", 100);
	remote_device_db_memory.put_name(&addr1, &device_name);
    CHECK(remote_device_db_memory.get_name(&addr1, &device_name));
    
    remote_device_db_memory.delete_name(&addr1);
    CHECK(!remote_device_db_memory.get_name(&addr1, &device_name));
}

TEST(RemoteDeviceDB, SortByLastUsedName){
    sprintf((char*)device_name, "%d", 10);
	remote_device_db_memory.put_name(&addr1, &device_name);
    sprintf((char*)device_name, "%d", 20);
	remote_device_db_memory.put_name(&addr2, &device_name);
    sprintf((char*)device_name, "%d", 30);
	remote_device_db_memory.put_name(&addr3, &device_name);
    
    CHECK(remote_device_db_memory.get_name(&addr2, &device_name));
    
    //get first element of the list
    db_mem_device_name_t * item = (db_mem_device_name_t *) db_mem_names;
    STRCMP_EQUAL((char*)item->device_name, "10"); 
}

TEST(RemoteDeviceDB, SinglePutGetDeleteKey){
	sprintf((char*)link_key, "%d", 100);
	remote_device_db_memory.put_link_key(&addr1, &link_key);
    CHECK(remote_device_db_memory.get_link_key(&addr1, &link_key));
        
    remote_device_db_memory.delete_link_key(&addr1);
    CHECK(!remote_device_db_memory.get_link_key(&addr1, &link_key));
}

TEST(RemoteDeviceDB, SortByLastUsedKey){
    sprintf((char*)link_key, "%d", 10);
	remote_device_db_memory.put_link_key(&addr1, &link_key);
    sprintf((char*)link_key, "%d", 20);
	remote_device_db_memory.put_link_key(&addr2, &link_key);
    sprintf((char*)link_key, "%d", 30);
	remote_device_db_memory.put_link_key(&addr3, &link_key);
    
    CHECK(remote_device_db_memory.get_link_key(&addr2, &link_key));
    
    //get first element of the list
    db_mem_device_link_key_t * item = (db_mem_device_link_key_t *) db_mem_link_keys;
    STRCMP_EQUAL((char*)item->link_key, "10"); 
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}