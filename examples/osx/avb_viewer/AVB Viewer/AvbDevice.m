//
//  AvbDevice.m
//  AVB Viewer
//
//  Created by John Gildred on 10/29/13.
//  Copyright (c) 2013 AVB.io. All rights reserved.
//

#import "AvbDevice.h"

@implementation AvbDevice

enum {
    kAvbioVendorId      = 0x00000000,          // these are not real, just placeholders
    kAvbioBrickModelId  = 0x0000000000000001,  // these are not real, just placeholders
    kAvbioBulletModelId = 0x0000000000000001,  // these are not real, just placeholders
    kXmosVendorId       = 0x00229700,          // these are not real, just placeholders
    kXmosModelId        = 0x0022970000000000   // these are not real, just placeholders
};

NSString *kAvbioBrickIcon = @"AppIcon";
NSString *kXmosIcon       = @"xmos";

- (id)init {
    return [self initWithId: @"Manually Added Device"];
}

- (id)initWithId:(NSString *)deviceId {
    self = [super init];
    if (self) {
        _deviceId      = [deviceId copy];
        _name          = [deviceId copy];
        _image         = [NSApp applicationIconImage];
        _isLocal       = NO;
        _isTalker      = NO;
        _isListener    = NO;
        _isController  = NO;
        _talkerType    = @"Unknown";
        _listenerType  = @"Unknown";
        _talkerSources = 0;
        _listenerSinks = 0;
        _macAddresses  = [[NSMutableArray alloc] init];
        _modelId       = @"Unknown";
        _vendorId      = @"Unknown";
        _children      = [[NSMutableArray alloc] init];
    }
    return self;
}

- (id)initWithEntity:(AVB17221Entity *)entity {
    self = [super init];
    if (self) {
        _deviceId      = [NSString stringWithFormat:@"0x00%llx", entity.entityID];
        _name          = [NSString stringWithFormat:@"Device (%@)", [_deviceId substringFromIndex:(_deviceId.length - 4)]];
        _image         = [self pickDeviceImage:entity];
        _isLocal       = entity.isLocalEntity;
        _isTalker      = entity.talkerCapabilities ? YES : NO;
        _isListener    = entity.listenerCapabilities ? YES : NO;
        _isController  = entity.controllerCapabilities ? YES : NO;
        _talkerType    = [self getTypeFromCapabilities:entity ofTalker:true];
        _listenerType  = [self getTypeFromCapabilities:entity ofTalker:false];
        _talkerSources = entity.talkerStreamSources;
        _listenerSinks = entity.listenerStreamSinks;
        _macAddresses  = (NSMutableArray *) entity.macAddresses;
        _modelId       = [NSString stringWithFormat:@"0x00%llx", entity.entityModelID];
        _vendorId      = [self vendorFromEntity:entity];
        _children      = [[NSMutableArray alloc] init];
    }
    return self;
}

- (void)updateFromEntity:(AVB17221Entity *)entity {
    if (self) {
        _deviceId      = [NSString stringWithFormat:@"0x00%llx", entity.entityID];
        _name          = [NSString stringWithFormat:@"Device (%@)", [_deviceId substringFromIndex:(_deviceId.length - 4)]];
        _image         = [self pickDeviceImage:entity];
        _isLocal       = entity.isLocalEntity;
        _isTalker      = entity.talkerCapabilities ? YES : NO;
        _isListener    = entity.listenerCapabilities ? YES : NO;
        _isController  = entity.controllerCapabilities ? YES : NO;
        _talkerType    = [self getTypeFromCapabilities:entity ofTalker:true];
        _listenerType  = [self getTypeFromCapabilities:entity ofTalker:false];
        _talkerSources = entity.talkerStreamSources;
        _listenerSinks = entity.listenerStreamSinks;
        _macAddresses  = (NSMutableArray *) entity.macAddresses;
        _modelId       = [NSString stringWithFormat:@"0x00%llx", entity.entityModelID];
        _vendorId      = [self vendorFromEntity:entity];
        _children      = [[NSMutableArray alloc] init];
    }
}

- (id)vendorFromEntity:(AVB17221Entity *)entity {
    NSString * vendorId = @"Unknown";
    if (self) {
        vendorId = [[NSString stringWithFormat:@"0x00%llx", entity.entityModelID] substringToIndex:10];
    }
    return vendorId;
}

- (id)pickDeviceImage:(AVB17221Entity *)entity {
    NSImage *image;
    // Macbook icon
    if (entity.isLocalEntity) {
        image = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kComputerIcon)];
    }
    else {
        unsigned vendorID = 0;
        NSScanner *scanner = [NSScanner scannerWithString:[[self vendorFromEntity:entity] substringFromIndex:2]];
        [scanner scanHexInt:&vendorID];
        switch (vendorID) {
            case kAvbioVendorId:
                // Brick icon
                if (entity.entityModelID == kAvbioBrickModelId) {
                    image = [NSApp applicationIconImage];
                }
                // Bullet icon placeholder
                else {
                    image = [NSApp applicationIconImage];
                }
                break;
            case kXmosVendorId:
                image = [NSImage imageNamed:kXmosIcon];
                break;
            default:
                image = [NSApp applicationIconImage];
                break;
        }
    }
    return image;
}

- (id)getTypeFromCapabilities:(AVB17221Entity *)entity ofTalker:(BOOL)isTalker {
    NSString *type;
    NSInteger typeValue;
    if (isTalker) {
        typeValue = [[[NSString stringWithFormat:@"%x", entity.talkerCapabilities] substringToIndex:1] integerValue];
    }
    else {
        typeValue = [[[NSString stringWithFormat:@"%x", entity.listenerCapabilities] substringToIndex:1] integerValue];
    }
    switch (typeValue) {
        case (NSInteger)4:
            type = @"Audio";
            break;
        case (NSInteger)8:
            type = @"Video";
            break;
        default:
            type = @"Unknown";
            break;
    }
    return type;
}

- (void)addChild:(AvbDevice *)device {
    [_children addObject: device];
}

- (void)removeChild:(AvbDevice *)device {
    [_children removeObject: device];
}

@end
