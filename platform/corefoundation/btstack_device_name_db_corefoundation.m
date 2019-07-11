/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

#define __BTSTACK_FILE__ "btstack_device_name_db_corefoundation.c"

#include "btstack_device_name_db_corefoundation.h"
#include "btstack_debug.h"

#import <Foundation/Foundation.h>

#define BTdaemonID         "ch.ringwald.btdaemon"
#define BTDaemonPrefsPath  "Library/Preferences/ch.ringwald.btdaemon.plist"

#define DEVICES_KEY        "devices"
#define PREFS_REMOTE_NAME  @"RemoteName"
#define PREFS_LINK_KEY     @"LinkKey"

static void put_name(bd_addr_t bd_addr, device_name_t *device_name);

static NSMutableDictionary *remote_devices  = nil;

// Device info
static void db_open(void){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	// NSDictionary * dict = [defaults persistentDomainForName:BTdaemonID];
	
    // NSDictionary * dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    NSDictionary * dict;
    dict = (NSDictionary*) CFPreferencesCopyAppValue(CFSTR(DEVICES_KEY), CFSTR(BTdaemonID));
    remote_devices = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
    
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *deviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[deviceEntry addEntriesFromDictionary:value];
		[remote_devices setObject:deviceEntry forKey:key];
	}
        
    log_info("read prefs for %u devices\n", (unsigned int) [dict count]);
    
    [pool release];
}

static void db_synchronize(void){
    log_info("stored prefs for %u devices\n", (unsigned int) [remote_devices count]);
    
    // 3 different ways
    
    // Core Foundation
    CFPreferencesSetValue(CFSTR(DEVICES_KEY), (CFPropertyListRef) remote_devices, CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    CFPreferencesSynchronize(CFSTR(BTdaemonID), kCFPreferencesCurrentUser, kCFPreferencesCurrentHost);
    
    // NSUserDefaults didn't work
    // 
	// NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    // [defaults setPersistentDomain:remote_devices forName:BTdaemonID];
    // [defaults synchronize];
}

static void db_close(void){ 
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

    // don't call db_synchronize();
    // a) we're calling db_synchronize() after each change already
    // b) db_close is called during the SIGINT handler which causes a corrupt prefs file
    
    [remote_devices release];
    remote_devices = nil;
    [pool release];
}

static NSString * stringForAddress(bd_addr_t addr) {
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]];
}
                             
static void set_value(bd_addr_t bd_addr, NSString *key, id value){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	if (!deviceDict){
		deviceDict = [NSMutableDictionary dictionaryWithCapacity:3];
		[remote_devices setObject:deviceDict forKey:devAddress];
	}
    [deviceDict setObject:value forKey:key];
    db_synchronize();
}

static void delete_value(bd_addr_t bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	[deviceDict removeObjectForKey:key];
    db_synchronize();

}

static id get_value(bd_addr_t bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
    if (!deviceDict) return nil;
    return [deviceDict objectForKey:key];
}

static void put_name(bd_addr_t bd_addr, device_name_t *device_name){
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSString *remoteName = [NSString stringWithUTF8String:(char*)device_name];
	if (!remoteName){
        remoteName = [NSString stringWithCString:(char*)device_name encoding:NSISOLatin1StringEncoding];
    }
    if (remoteName) {
        set_value(bd_addr, PREFS_REMOTE_NAME, remoteName);
    }
    [pool release];
}

static void delete_name(bd_addr_t bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_REMOTE_NAME);
    [pool release];
}

static int  get_name(bd_addr_t bd_addr, device_name_t *device_name) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSString *remoteName = get_value(bd_addr, PREFS_REMOTE_NAME);
    if (remoteName){
        memset(device_name, 0, sizeof(device_name_t));
        strncpy((char*) device_name, [remoteName UTF8String], sizeof(device_name_t)-1);
    }
    [pool release];
    return (remoteName != nil);
}

btstack_device_name_db_t btstack_device_name_db_corefounation = {
    db_open,
    db_close,
    get_name,
    put_name,
    delete_name,
};

const btstack_device_name_db_t * btstack_device_name_db_corefoundation_instance(void) {
    return &btstack_device_name_db_corefounation;
}

