#include <stdio.h>
#include <string.h>
#include "remote_device_db.h"

bd_addr_t addr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
device_name_t device_name = {'m', 'a', 't', 't'};
link_key_t link_key = {'h', 'i', 'a', 's'};
    
void testPutGetDeleteName(void){
    remote_device_db_memory.put_name(&addr, &device_name);
    if (remote_device_db_memory.get_name(&addr, &device_name)) {
        printf("OK ------> found device \n");
    } else {
        printf("ERROR ------> device not found \n");
    };
    
    remote_device_db_memory.delete_name(&addr);
    if (remote_device_db_memory.get_name(&addr, &device_name)) {
        printf("ERROR ------> device not deleted \n");
    } else {
        printf("OK ------> device deleted \n");
    };
    
    remote_device_db_memory.put_link_key(&addr, &link_key);
    if (remote_device_db_memory.get_name(&addr, &device_name)) {
        printf("ERROR ------> no device with such name\n");
    } else {
        printf("OK ------> device with such name does not exist \n");
    };
    
}

void testPutGetDeleteKey(void){
    remote_device_db_memory.put_link_key(&addr, &link_key);
    if (remote_device_db_memory.get_link_key(&addr, &link_key)) {
        printf("OK ------> found key \n");
    } else {
        printf("ERROR ------> key not found \n");
    };
    
    remote_device_db_memory.delete_link_key(&addr);
    if (remote_device_db_memory.get_link_key(&addr, &link_key)) {
        printf("ERROR ------> key not deleted \n");
    } else {
        printf("OK ------> key deleted \n");
    };
    
    remote_device_db_memory.put_name(&addr, &device_name);
    if (remote_device_db_memory.get_link_key(&addr, &link_key)) {
        printf("ERROR ------> no device with such link key\n");
    } else {
        printf("OK ------> device with such link key does not exist \n");
    };
}

int main (int argc, const char * argv[]){
    printf("\n<remote_device_db_memory_test> \n");
    testPutGetDeleteName();
    testPutGetDeleteKey();      
    printf("</remote_device_db_memory_test> \n\n");
    
	return 0;
}