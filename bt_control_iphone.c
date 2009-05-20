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
#include <errno.h>

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
    strcpy(buffer, "/etc/bluetool/");
    char *machine = get_machine_name();
    strcat(buffer, machine);
    strcat(buffer, ".init.script");
    
    // hack
    // strcpy(buffer, "iPhone1,2.init.script");
    
    // open script
    int input = open(buffer, O_RDONLY);
    
    // open tool
    FILE * outputFile = popen("BlueTool", "r+");
    int output = fileno(outputFile);
     
    int pos = 0;
    int mirror = 0;
    int store = 1;
    struct timeval noTime;
    bzero (&noTime, sizeof(struct timeval));
    while (1){
        int chars = read(input, &buffer[pos], 1);
        
        int ready;
        do {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(output,&fds);
            ready=select(output+1,&fds,NULL,NULL,&noTime);
            if (ready>0)
            {
                if (FD_ISSET(output,&fds)) {
                    char singlechar = fgetc(outputFile);
                    printf("%c", singlechar);
                }
            }
        } while (ready);
        
        // end-of-line
        if (chars == 0 || buffer[pos]=='\n' || buffer[pos]== '\r'){
            if (store) {
                // stored characters
                write(output, buffer, pos+chars);
            }
            if (mirror) {
                write(output, "\n", 1);
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
            write(output, &buffer[pos], 1);
        }
        
        // store
        if (store) {
            pos++;
        }
        
        // check for "csr -p 0x002a=0x0011" (20)
        if (store == 1 && pos == 20) {
            int pskey, value;
            store = 0;
            if (sscanf(buffer, "csr -p 0x%x=0x%x", &pskey, &value) == 2){
                if (pskey == 0x01f9) {       // UART MODE
                    write(output, "Skipping: ", 10);
                    write(output, buffer, pos);
                    mirror = 1;
                } else if (pskey == 0x01be) { // UART Baud
                    write(output, "Skipping: ", 10);
                    write(output, buffer, pos);
                    mirror = 1;
                } else {
                    // dump buffer and start forwarding
                    write(output, buffer, pos);
                    mirror = 1;
                }
            } else {
                write(output, buffer, pos);
                mirror = 1;
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
