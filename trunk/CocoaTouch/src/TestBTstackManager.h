//
//  TestBTstackManager.h
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import <Foundation/Foundation.h>
#import <BTstack/BTstackManager.h>

@class BTstackManager;
@class BTDiscoveryViewController;
@interface TestBTstackManager : NSObject<BTstackManagerDelegate,BTstackManagerListener>{
	BTstackManager *bt;
	BTDiscoveryViewController* discoveryView;
}

@end
