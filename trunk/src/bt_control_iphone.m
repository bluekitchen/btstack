/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 *  bt_control_iphone.c
 *
 *  control Bluetooth module using BlueTool
 *
 *  Created by Matthias Ringwald on 5/19/09.
 *
 *  Bluetooth Toggle by BigBoss
 */

#include "../config.h"

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

#ifdef USE_BLUETOOL

// minimal IOKit
#ifdef __APPLE__
#include <Availability.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_2_0
#include <mach/mach.h>
#define IOKIT
#include <device/device_types.h>
#include <UIKit/UIKit.h>
kern_return_t IOMasterPort( mach_port_t	bootstrapPort, mach_port_t * masterPort );
CFMutableDictionaryRef IOServiceNameMatching(const char * name );
CFTypeRef IORegistryEntrySearchCFProperty(mach_port_t entry, const io_name_t plane,
                                          CFStringRef key, CFAllocatorRef allocator, UInt32 options );
mach_port_t IOServiceGetMatchingService(mach_port_t masterPort, CFDictionaryRef matching );
kern_return_t IOObjectRelease(mach_port_t object);
#endif
#endif

int iphone_system_bt_enabled(){
    // return SBA_getBluetoothEnabled();
    return false;
}

void iphone_system_bt_set_enabled(int enabled)
{
    SBA_setBluetoothEnabled(enabled);
    sleep(2); // give change a chance
}

#endif

#define BUFF_LEN 80
static char buffer[BUFF_LEN+1];

// bd addr from iphone/ipod IORegistry
// the Broadcom chipsets done store a BD_ADDR and the one on the iPhone
// is different to the one reported by About in the Settings app
static bd_addr_t ioRegAddr;

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
	if (!strncmp("iPod1", machine, strlen("iPod1"))) return 0;     // 1st gen touch no BT
    return 1;
}

static const char * iphone_name(void *config){
    return get_machine_name();
}

// Get BD_ADDR from IORegistry
static void ioregistry_get_bd_addr() {
#ifdef IOKIT
    mach_port_t mp;
    IOMasterPort(MACH_PORT_NULL,&mp);
    CFMutableDictionaryRef bt_matching = IOServiceNameMatching("bluetooth");
    mach_port_t bt_service = IOServiceGetMatchingService(mp, bt_matching);
    CFTypeRef bt_typeref = IORegistryEntrySearchCFProperty(bt_service,"IODevicTree",CFSTR("local-mac-address"), kCFAllocatorDefault, 1);
    CFDataGetBytes(bt_typeref,CFRangeMake(0,CFDataGetLength(bt_typeref)),ioRegAddr); // buffer needs to be unsigned char
    IOObjectRelease(bt_service);
#else
    // use dummy addr if not on iphone/ipod touch
    int i = 0;
    for (i=0;i<6;i++) {
        ioRegAddr[i] = i;
    }
#endif
}

static int iphone_has_csr(){
    // construct script path from device name
    char *machine = get_machine_name();
	if (strncmp(machine, "iPhone1,", strlen("iPhone1,")) == 0) {
        return 1;
	}
    return 0;
}

static void iphone_write_string(int fd, char *string){
    int len = strlen(string);
    write(fd, string, len);
}

static void iphone_csr_set_pskey(int fd, int key, int value){
    int len = sprintf(buffer, "csr -p 0x%04x=0x%04x\n", key, value);
    write(fd, buffer, len);
}

static void iphone_csr_set_bd_addr(int fd){
    int len = sprintf(buffer,"\ncsr -p 0x0001=0x00%.2x,0x%.2x%.2x,0x00%.2x,0x%.2x%.2x\n",
            ioRegAddr[3], ioRegAddr[4], ioRegAddr[5], ioRegAddr[2], ioRegAddr[0], ioRegAddr[1]);
    write(fd, buffer, len);
}

static void iphone_csr_set_baud(int fd, int baud){
    // calculate baud rate (assume rate is multiply of 100)
    uint32_t baud_key = (4096 * (baud/100) + 4999) / 10000;
    iphone_csr_set_pskey(fd, 0x01be, baud_key);
}

static void iphone_bcm_set_bd_addr(int fd){
    int len = sprintf(buffer, "bcm -a %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", ioRegAddr[0], ioRegAddr[1],
            ioRegAddr[2], ioRegAddr[3], ioRegAddr[4], ioRegAddr[5]);
    write(fd, buffer, len);
}

static void iphone_bcm_set_baud(int fd, int baud){
    int len = sprintf(buffer, "bcm -b %u\n",  baud);
    write(fd, buffer, len);
}

static int iphone_write_initscript (int output, int baudrate){
        
    // construct script path from device name
    strcpy(buffer, "/etc/bluetool/");
    char *machine = get_machine_name();
    strcat(buffer, machine);
    if (iphone_has_csr()){
		strcat(buffer, ".init.script");
    } else {
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
        			
		// iPhone - set BD_ADDR after "csr -i"
		if (store == 1 && pos == 6 && strncmp(buffer, "csr -i", 6) == 0) {
			store = 0;
			write(output, buffer, pos); // "csr -i"
            iphone_csr_set_bd_addr(output);
		}
		
		// iPod2,1
		// check for "bcm -X" cmds
		if (store == 1 && pos == 6){
 			if (strncmp(buffer, "bcm -", 5) == 0) {
				store  = 0;
				switch (buffer[5]){
					case 'a': // BT Address
                        iphone_bcm_set_bd_addr(output);
						mirror = 0;
						break;
					case 'b': // baud rate command OS 2.x
                    case 'B': // baud rate command OS 3.x
                        iphone_bcm_set_baud(output, baudrate);
						mirror = 0;
						break;
					case 's': // sleep mode - replace with "wake" command?
						iphone_write_string(output, "wake on\n");
						mirror = 0;
						break;
					default: // other "bcm -X" command
						write(output, buffer, pos);
						mirror = 1;
				}
			}
		}
        
        // iPhone1,1 & iPhone 2,1: OS 3.x
        // check for "csr -B" and "csr -T"
        if (store == 1 && pos == 6){
			if (strncmp(buffer, "csr -", 5) == 0) {
                switch(buffer[5]){
                    case 'T':   // Transport Mode
                        store  = 0;
                        break;
                    case 'B':   // Baud rate
                        iphone_csr_set_baud(output, baudrate);
                        store  = 0;
                        break;
                    default:    // wait for full command
                        break;
                }
            }
        }
                
		// iPhone1,1 & iPhone 2,1: OS 2.x
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
                        iphone_csr_set_baud(output, baudrate);
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

static void iphone_write_configscript(int fd, int baudrate){
    iphone_write_string(fd, "device -D -S\n");
    if (iphone_has_csr()) {
        iphone_csr_set_baud(fd, baudrate);
        iphone_csr_set_bd_addr(fd);
        iphone_write_string(fd, "csr -r\n");
    } else {
        iphone_bcm_set_baud(fd, baudrate);
        iphone_write_string(fd, "msleep 200\n");
        iphone_bcm_set_bd_addr(fd);
        iphone_write_string(fd, "msleep 50\n");
    }
    iphone_write_string(fd, "quit\n");
}

static char *os3xBlueTool = "BlueTool";
static char *os4xBlueTool = "/usr/local/bin/BlueToolH4";

static int iphone_on (void *transport_config){

    int err = 0;

    hci_uart_config_t * hci_uart_config = (hci_uart_config_t*) transport_config;

    // get bd_addr from IORegistry 
	ioregistry_get_bd_addr();

#ifdef USE_BLUETOOL
    if (iphone_system_bt_enabled()){
        perror("iphone_on: System Bluetooth enabled!");
        return 1;
    }
    
#if 0
    // use tmp file for testing on os 3.x
    int output = open("/tmp/bt.init", O_WRONLY | O_CREAT | O_TRUNC);
    iphone_write_initscript(hci_uart_config, output);
    close(output);
    err = system ("BlueTool < /tmp/bt.init");
#else
    
    // check for os version >= 4.0
    int os4x = 1;
    NSAutoreleasePool * pool =[[NSAutoreleasePool alloc] init];
    NSString * systemVersion = [[UIDevice currentDevice] systemVersion];
    if ([systemVersion hasPrefix:@"2."]) os4x = 0;
    if ([systemVersion hasPrefix:@"3."]) os4x = 0;
    // NSLog(@"OS Version: %@, ox4x = %u", systemVersion, os4x);
    [pool release];
    
    // OS 4.0
    char * bluetool = os3xBlueTool;
    if (os4x) {
        bluetool = os4xBlueTool;
        // basic config
        sprintf(buffer, "%s -F boot", bluetool);
        system(buffer);
        sprintf(buffer, "%s -F init", bluetool);
        system(buffer);
    }
    
    // advanced config
    FILE * outputFile = popen(bluetool, "r+");
    setvbuf(outputFile, NULL, _IONBF, 0);
    int output = fileno(outputFile);
    
    if (os4x) {
        // 4.x - send custom config
        iphone_write_configscript(output, hci_uart_config->baudrate);
    } else {
        // 3.x - modify original script on the fly
        iphone_write_initscript(output, hci_uart_config->baudrate);
    }
    
    // log output
    fflush(outputFile);
    int singlechar;
    while (1) {
        singlechar = fgetc(outputFile);
        if (singlechar == EOF) break;
        printf("%c", singlechar);
    };
    err = pclose(outputFile);
    
#endif
    
    // if we sleep for about 3 seconds, we miss a strage packet... but we don't care
    // sleep(3); 
    
#else
    // quick test if Bluetooth UART can be opened
    int fd = open(hci_uart_config->device_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("iphone_on: Unable to open port ");
        perror(hci_uart_config->device_name);
        return 1;
    }
    close(fd);
#endif
    
    return err;
}

static int iphone_off (void *config){
	
	char *machine = get_machine_name();
	
	if (strncmp(machine, "iPad", strlen("iPad")) == 0) {
		// put iPad Bluetooth into deep sleep
		system ("echo \"wake off\n quit\" | BlueTool");
	} else {
		// power off for iPhone and iPod
		system ("echo \"power off\n quit\" | BlueTool");
	}
	
    // kill Apple BTServer as it gets confused and fails to start anyway
    system("killall BTServer");
    return 0;
}


// single instance
bt_control_t bt_control_iphone = {
    iphone_on,
    iphone_off,
    iphone_valid,
    iphone_name,
    NULL
};
