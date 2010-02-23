//
//  TestBTstackManager.h
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import <Foundation/Foundation.h>
#import <BTstack/BTstackManager.h>
#import <BTstack/BTDiscoveryViewController.h>

@class BTstackManager;
@class BTDiscoveryViewController;
@interface TestBTstackManager : NSObject<BTstackManagerDelegate,BTstackManagerListener,BTDiscoveryDelegate>{
	BTstackManager *bt;
	BTDiscoveryViewController* discoveryView;
	BTDevice *selectedDevice;
}

@end
