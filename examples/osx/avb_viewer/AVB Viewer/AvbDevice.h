//
//  AvbDevice.h
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AudioVideoBridging/AudioVideoBridging.h>

@interface AvbDevice : NSObject

@property (copy) NSString *name;
@property (copy) NSImage *image;
@property (copy) NSString *deviceId;
@property (copy) NSMutableArray *macAddresses;
@property (copy) NSString *modelId;
@property (copy) NSString *vendorId;
@property BOOL isLocal;
@property BOOL isTalker;
@property BOOL isListener;
@property BOOL isController;
@property (copy) NSString *talkerType;
@property (copy) NSString *listenerType;
@property int talkerSources;
@property int listenerSinks;
@property (readonly, copy) NSMutableArray *children;

- (id)initWithId:(NSString *)deviceId;
- (id)initWithEntity:(AVB17221Entity *)entity;
- (void)updateFromEntity:(AVB17221Entity *)entity;
- (NSString *)vendorFromEntity:(AVB17221Entity *)entity;
- (void)addChild:(AvbDevice *)device;
- (void)removeChild:(AvbDevice *)device;

@end
