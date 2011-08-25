#include <stdio.h>
#include <string.h>
#include "remote_device_db.h"

void testPutGetDeleteName(void){
	bd_addr_t addr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    device_name_t device_name;
    strcpy(device_name, "matthias");

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
}

void testPutGetDeleteKey(void){
	bd_addr_t addr = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05 };
    link_key_t link_key;
    strcpy(link_key, "matthias");

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
}

int main (int argc, const char * argv[]){
    
    testPutGetDeleteName();
    testPutGetDeleteKey();      
    printf("DONE \n\n");
    
	return 0;
}