//
//  main.m
//
//  Created by Matthias Ringwald on 10/8/09.
//

#import <UIKit/UIKit.h>

int main(int argc, char *argv[]) {
    
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    int retVal = UIApplicationMain(argc, argv, nil, @"BTstackCocoaAppDelegate");
    [pool release];
    return retVal;
}
