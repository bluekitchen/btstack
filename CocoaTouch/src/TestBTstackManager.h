//
//  TestBTstackManager.h
//
//  Created by Matthias Ringwald on 19.02.10.
//

#import <Foundation/Foundation.h>
#import <BTstack/BTstackManager.h>

@class BTstackManager;

@interface TestBTstackManager : NSObject<BTstackManagerDelegate>{
	BTstackManager *bt;
}

@end
