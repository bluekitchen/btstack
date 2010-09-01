/*
 * Copyright (C) 2010 by Matthias Ringwald
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
#include "remote_device_db.h"
#import <Foundation/Foundation.h>

#define BTdaemonID         @"ch.ringwald.btdaemon"
#define PREFS_REMOTE_NAME  @"RemoteName"
#define PREFS_LINK_KEY     @"LinkKey"

static NSMutableDictionary *remote_devices = nil;

// Device info
static void db_open(){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	NSDictionary * dict = [defaults persistentDomainForName:BTdaemonID];
	remote_devices = [[NSMutableDictionary alloc] initWithCapacity:([dict count]+5)];
	
	// copy entries
	for (id key in dict) {
		NSDictionary *value = [dict objectForKey:key];
		NSMutableDictionary *deviceEntry = [NSMutableDictionary dictionaryWithCapacity:[value count]];
		[deviceEntry addEntriesFromDictionary:value];
		[remote_devices setObject:deviceEntry forKey:key];
	}
	NSLog(@"read prefs (retain %u) %@", [remote_devices retainCount], remote_devices );
    [pool release];
}

static void db_close(){ 
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSLog(@"store prefs %@", remote_devices );
    
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setPersistentDomain:remote_devices forName:BTdaemonID];
    [defaults synchronize];
    
    [remote_devices release];
    remote_devices = nil;
    [pool release];
}

static NSString * stringForAddress(bd_addr_t* address) {
	uint8_t *addr = (uint8_t*) *address;
	return [NSString stringWithFormat:@"%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2],
			addr[3], addr[4], addr[5]];
}
                             
static void set_value(bd_addr_t *bd_addr, NSString *key, id value){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	if (!deviceDict){
		deviceDict = [NSMutableDictionary dictionaryWithCapacity:3];
		[remote_devices setObject:deviceDict forKey:devAddress];
	}
    [deviceDict setObject:value forKey:key];
}

static void delete_value(bd_addr_t *bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
	[deviceDict removeObjectForKey:key];
}

static id get_value(bd_addr_t *bd_addr, NSString *key){
	NSString *devAddress = stringForAddress(bd_addr);
	NSMutableDictionary * deviceDict = [remote_devices objectForKey:devAddress];
    if (!deviceDict) return nil;
    return [deviceDict objectForKey:key];
}

static int get_link_key(bd_addr_t *bd_addr, link_key_t *link_key) {
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSData *linkKey = get_value(bd_addr, PREFS_LINK_KEY);
    if ([linkKey length] == LINK_KEY_LEN){
        memcpy(link_key, [linkKey bytes], LINK_KEY_LEN);
    }
    [pool release];
    return (linkKey != nil);
}

static void put_link_key(bd_addr_t *bd_addr, link_key_t *link_key){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSData *linkKey = [NSData dataWithBytes:link_key length:16];
    set_value(bd_addr, PREFS_LINK_KEY, linkKey);
    [pool release];
}

static void delete_link_key(bd_addr_t *bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_LINK_KEY);
    [pool release];
}

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name){
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSString *remoteName = [NSString stringWithUTF8String:(char*)device_name];
    set_value(bd_addr, PREFS_REMOTE_NAME, remoteName);
    [pool release];
}

static void delete_name(bd_addr_t *bd_addr){
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    delete_value(bd_addr, PREFS_REMOTE_NAME);
    [pool release];
}

static int  get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSString *remoteName = get_value(bd_addr, PREFS_REMOTE_NAME);
    if (remoteName){
        bzero(device_name, sizeof(device_name_t));
        strncpy((char*) device_name, [remoteName UTF8String], sizeof(device_name_t)-1);
    }
    [pool release];
    return (remoteName != nil);
}

remote_device_db_t remote_device_db_iphone = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name
};

