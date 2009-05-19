/*
 *  bt_control_iphone.c
 *
 *  control Bluetooth module using BlueTool
 *
 *  Created by Matthias Ringwald on 5/19/09.
 */

#include "bt_control_iphone.h"

#include <sys/utsname.h>  // uname
#include <stdlib.h>       // system
#include <stdio.h>        // sscanf, printf
#include <fcntl.h>        // open
#include <string.h>       // strcpy, strcat, strncmp
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>


#define BUFF_LEN 80
static char buffer[BUFF_LEN+1];

/** 
 * get machine name
 */
static struct utsname unmae_info;
static char *get_machine_name(void){
	uname(&unmae_info);
	return unmae_info.machine;
}

/**
 * on iPhone/iPod touch
 */
static int iphone_valid(void *config){
	char * machine = get_machine_name();
	return strncmp("iPhone", machine, strlen("iPhone")) == 0;
}

static const char * iphone_name(void *config){
    return get_machine_name();
}

static int iphone_on (void *config){
    // get path to script
    strcpy(buffer, "./etc/bluetool/");
    char *machine = get_machine_name();
    strcat(buffer, machine);
    strcat(buffer, ".init.script");
    
    // open script
    int input = open(buffer, O_RDONLY);
    int pos = 0;
    int mirror = 0;
    int store = 1;
    while (1){
        int chars = read(input, &buffer[pos], 1);
        
        // end-of-line
        if (chars == 0 || buffer[pos]=='\n' || buffer[pos]== '\r'){
            if (store) {
                // stored characters
                write(fileno(stdout), buffer, pos+chars);
            }
            if (mirror) {
                write(fileno(stdout), "\n", 1);
            }
            pos = 0;
            mirror = 0;
            store = 1;
            if (chars) {
                continue;
            } else {
                break;
            }
        }
        
        // mirror
        if (mirror){
            write(fileno(stdout), &buffer[pos], 1);
        }
        
        // store
        if (store) {
            pos++;
        }
        
        // enough chars, check for pskey store command
        if (mirror == 0 && pos == 9) {
            if (strncmp(buffer, "csr -p 0x", 9) != 0) {
                write(fileno(stdout), buffer, pos);
                mirror = 1;
                store = 0;
            }
        }
        // check for "csr -p 0x002a=0x0011" (20)
        if (mirror == 0 && store == 1 && pos == 20) {
            int pskey, value;
            store = 0;
            if (sscanf(buffer, "csr -p 0x%x=0x%x", &pskey, &value) == 2){
                if (pskey == 0x01f9) {       // UART MODE
                    write(fileno(stdout), "Skipping: ", 10);
                    write(fileno(stdout), buffer, pos);
                    mirror = 1;
                } else if (pskey == 0x01be) { // UART Baud
                    write(fileno(stdout), "Skipping: ", 10);
                    write(fileno(stdout), buffer, pos);
                    mirror = 1;
                } else {
                    // dump buffer and start forwarding
                    write(fileno(stdout), buffer, pos);
                    mirror = 1;
                }
            }
        }
    }
    return 0;
}

static int iphone_off (void *config){
    // power off 
    system ("echo \"power off\n quit\" | BlueTool");
    return 0;
}


// single instance
bt_control_t bt_control_iphone = {
    iphone_on,
    iphone_off,
    iphone_valid,
    iphone_name
};
