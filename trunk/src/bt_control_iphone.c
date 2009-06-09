/*
 *  bt_control_iphone.c
 *
 *  control Bluetooth module using BlueTool
 *
 *  Created by Matthias Ringwald on 5/19/09.
 */

#include "bt_control_iphone.h"
#include "hci_transport.h"
#include "hci.h"

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
	if (!strncmp("iPhone",  machine, strlen("iPhone" ))) return 1;
	if (!strncmp("iPod2,1", machine, strlen("iPod2,1"))) return 1;
	return 0;
}

static const char * iphone_name(void *config){
    return get_machine_name();
}

static void iphone_set_pskey(int fd, int key, int value){
    sprintf(buffer, "csr -p 0x%04x=0x%04x\n", key, value);
    write(fd, buffer, 21);
}

static int iphone_write_initscript (void *config, int output){
    
    // get uart config 
    hci_uart_config_t * uart_config = (hci_uart_config_t *) config;
    
    // calculate baud rate (assume rate is multiply of 100)
    uint32_t baud_key = (4096 * (uart_config->baudrate/100) + 4999) / 10000;
    printf("Baud key %u\n", baud_key);
    
	// pick random BT address
	uint32_t random_nr = random();
	
    // construct script path from device name
    strcpy(buffer, "/etc/bluetool/");
    char *machine = get_machine_name();
    strcat(buffer, machine);
	if (strncmp(machine, "iPhone", strlen("iPhone")) == 0){
		// It's an iPhone
		strcat(buffer, ".init.script");
	} else {
		// It's an iPod Touch (2G)
		strcat(buffer, ".boot.script");
	}
    
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
        
		// iPod2,1
		// check for "bcm -X" cmds
		if (store == 1 && pos == 7){
			if (strncmp(buffer, "bcm -", 5) == 0) {
				store  = 0;
				switch (buffer[5]){
					case 'a': // BT Address
						write(output, buffer, pos); // "bcm -a "
						sprintf(buffer, "00:00:%.2x:%.2x:%.2x:%.2x\n", random_nr & 0xff, (random_nr >> 8) & 0xff,
								(random_nr >> 16) & 0xff, (random_nr >> 24) & 0xff);
						write(output, buffer, 18);
						mirror = 0;
						break;
					case 'b': // baud rate command
						write(output, buffer, pos); // "bcm -b "
						sprintf(buffer, "%u\n",  uart_config->baudrate);
						write(output, buffer, strlen(buffer));
						mirror = 0;
						break;
					case 's': // sleep mode - replace with "wake" command?
						write(output,"wake on\n", strlen("wake on\n"));
						mirror = 0;
						break;
					default: // other "bcm -X" command
						write(output, buffer, pos);
						mirror = 1;
				}
			}
		}
		
		// iPhone1,1 & iPhone 2,1:
        // check for "csr -p 0x1234=0x5678" (20)
        if (store == 1 && pos == 20) {
            int pskey, value;
            store = 0;
            if (sscanf(buffer, "csr -p 0x%x=0x%x", &pskey, &value) == 2){
                switch (pskey) {
                    case 0x01f9:    // UART MODE
                    case 0x01c1:    // Configure H5 mode
                        mirror = 0;
                        break;
                    case 0x01be:    // PSKET_UART_BAUD_RATE
                        iphone_set_pskey(output, 0x01be, baud_key);
                        mirror = 0;
                        break;
                    default:
                        // anything else: dump buffer and start forwarding
                        write(output, buffer, pos);
                        mirror = 1;
                        break;
                }
            } else {
                write(output, buffer, pos);
                mirror = 1;
            }
        }
    }
    // close input
    close(input);
    return 0;
}

static int iphone_on (void *config){

#if 0
    // use tmp file
    int output = open("/tmp/bt.init", O_WRONLY | O_CREAT | O_TRUNC);
    iphone_write_initscript(config, output);
    close(output);
    system ("BlueTool < /tmp/bt.init");
#else
    // modify original script on the fly
    FILE * outputFile = popen("BlueTool", "r+");
    setvbuf(outputFile, NULL, _IONBF, 0);
    int output = fileno(outputFile);
    iphone_write_initscript(config, output);
    
    // log output
    fflush(outputFile);
    int singlechar;
    while (1) {
        singlechar = fgetc(outputFile);
        if (singlechar == EOF) break;
        printf("%c", singlechar);
    };
    pclose(outputFile);
        
#endif
    // if we sleep for about 3 seconds, we miss a strage packet
    // sleep(3); 
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
