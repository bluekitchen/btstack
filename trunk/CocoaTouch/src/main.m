//
//  main.m
//  BTstackCocoa
//
//  Created by Matthias Ringwald on 10/8/09.
//  Copyright Dybuster AG 2009. All rights reserved.
//

#import <UIKit/UIKit.h>

int main(int argc, char *argv[]) {
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int retVal = UIApplicationMain(argc, argv, nil, @"BTstackCocoaAppDelegate");
    [pool release];
    return retVal;
}
