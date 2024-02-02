/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include "../../../include/gainput/gainput.h"
#include "../../../include/gainput/GainputLog.h"

#if defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC)

#include "../../../include/gainput/apple/GainputGCKit.h"
#include "../../../../../../../../Utilities/Interfaces/IMemory.h"


#ifdef GAINPUT_GC_HAPTICS

@interface GainputGCHapticMotor ()
@property (strong, nonatomic) CHHapticEngine * _Nullable pHapticEngine;
@property (strong, nonatomic) id<CHHapticPatternPlayer> _Nullable pHapticPatternPlayer;
@property (strong, nonatomic) CHHapticDynamicParameter * _Nullable pIntensityDynamicParam;
@property (strong, nonatomic) CHHapticDynamicParameter * _Nullable pSharpnessDynamicParam;
@property (nonatomic) bool isInitialized;
@end

@implementation GainputGCHapticMotor {
}

-(void)cleanup
{
	@autoreleasepool
	{
		if (self.pHapticPatternPlayer != nil) {
			[self.pHapticPatternPlayer cancelAndReturnError:nil];
			self.pHapticPatternPlayer = nil;
		}
		if (self.pHapticEngine != nil) {
			[self.pHapticEngine stopWithCompletionHandler:nil];
			self.pHapticEngine = nil;
		}
		if (self.pIntensityDynamicParam != nil) {
			self.pIntensityDynamicParam = nil;
		}
		if (self.pSharpnessDynamicParam != nil) {
			self.pSharpnessDynamicParam = nil;
		}
		self.isInitialized = false;
	}
}

-(bool)setIntensity:(float)intensity sharpness:(float)sharpness duration:(float)duration
{
	if(!self.isInitialized)
		return false;

	@autoreleasepool
	{
		NSError *error = nil;

		if (self.pHapticEngine == nil) {
			return false;
		}

		if (intensity == 0.0f) {
			if (self.pHapticPatternPlayer) {
				[self.pHapticPatternPlayer stopAtTime:CHHapticTimeImmediate error:&error];

				if (error != nil)
				{
					GAINPUT_LOG("Error stopping existing haptics player. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
					return false;
				}
			}
			return true;
		}
		[self SetupPatternPlayer];
        
        //only need to update intensity
		//time stays as CHHapticTimeImmediate and sharpness uses secondary motor;
		self.pIntensityDynamicParam.value = intensity;
        self.pSharpnessDynamicParam.value = sharpness;
		NSArray* events= [NSArray arrayWithObjects:self.pIntensityDynamicParam, self.pSharpnessDynamicParam, nil];
		[self.pHapticPatternPlayer sendParameters:events atTime:CHHapticTimeImmediate error:&error];
		//events = nil;
		if (error != nil)
		{
			GAINPUT_LOG("Error sending Haptics Pattern to haptics player. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
			return false;
		}

		// start vibrating now
		[self.pHapticPatternPlayer startAtTime:CHHapticTimeImmediate error:&error];
		if (error != nil)
		{
			GAINPUT_LOG("Error setting vibrations using Pattern Player. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
			return false;
		}

		//GAINPUT_LOG("Current Engine Time. %.5f\n", self.pHapticEngine.currentTime);
		[self.pHapticPatternPlayer stopAtTime:self.pHapticEngine.currentTime + duration error:&error];
		if (error != nil)
		{
			GAINPUT_LOG("Error setting vibrations stop time. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
			return false;
		}
		return true;
	}
}

-(id) initWithController:(GCController*)controller locality:(GCHapticsLocality)locality
{
	@autoreleasepool {
		self = [super init];
		self.isInitialized = false;

		// early out if we don't support haptics.
		if(!controller.haptics)
			return self;

		self.pHapticEngine = [controller.haptics createEngineWithLocality:locality];
		if (self.pHapticEngine == nil) {
			return self;
		}

		if(![self StartHapticEngines])
		{
			[self.pHapticEngine stopWithCompletionHandler:nil];
			self.pHapticEngine = nil;
		}

		return self;
	}
}

-(id) initWithIOSDevice
{
	@autoreleasepool {
		self = [super init];
		self.isInitialized = false;

		// early out if we don't support haptics.
		id<CHHapticDeviceCapability> capabilities = [CHHapticEngine capabilitiesForHardware];
		if(!capabilities.supportsHaptics)
			return self;

		NSError *error;
		//Creatae without locality and haptics gamecontroller reference
		//Creates a physical ios device haptic engine.
		self.pHapticEngine = [[CHHapticEngine alloc] initAndReturnError:&error];
		if (self.pHapticEngine == nil || error != nil) {
			return self;
		}

		if(![self StartHapticEngines])
		{
			self.pHapticEngine = nil;
		}
		return self;
	}
}

-(bool) StartHapticEngines 
{
	NSError *error;
	self.pHapticEngine.isMutedForAudio = true;
	self.pHapticEngine.playsHapticsOnly = true;

	GainputGCHapticMotor* weakSelf = self;
	self.pHapticEngine.stoppedHandler = ^(CHHapticEngineStoppedReason stoppedReason) {
		GainputGCHapticMotor *_this = weakSelf;
		if (_this == nil) {
		   return;
	   }

	   _this.pHapticPatternPlayer = nil;
	   _this.pHapticEngine= nil;
   };

	self.pHapticEngine.resetHandler = ^{
		GainputGCHapticMotor *_this = weakSelf;
	   if (_this == nil) {
		   return;
	   }

	   _this.pHapticPatternPlayer= nil;
	   [_this.pHapticEngine startAndReturnError:nil];
   };

	[self.pHapticEngine startAndReturnError:&error];
	if (error != nil)
	{
		return false;
	}

	self.isInitialized = true;
	[self SetupPatternPlayer];

	return true;
}

-(void)SetupDynamicHapticParams
{
	//only needs to be allocated once
	if (self.pIntensityDynamicParam == nil) {
		self.pIntensityDynamicParam =[[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl value:1.0 relativeTime:CHHapticTimeImmediate];
	}
	if (self.pSharpnessDynamicParam == nil) {
		self.pSharpnessDynamicParam = [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticSharpnessControl value:1.0 relativeTime:CHHapticTimeImmediate];;
	}
}

-(void)SetupPatternPlayer
{
	// Should only be done once
	if (self.pHapticPatternPlayer == nil)
	{
		NSError * error = nil;
		CHHapticEventParameter *paramEvIntensity = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1.0f] autorelease];
		CHHapticEventParameter *paramEvSharpness = [[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:1.0f] autorelease];

		//use minimum duration of 16 ms in case duration is 0.
		CHHapticEvent * event = [[[CHHapticEvent alloc]
					initWithEventType:CHHapticEventTypeHapticContinuous
					parameters:[NSArray arrayWithObjects:paramEvIntensity, paramEvSharpness, nil]
					relativeTime:CHHapticTimeImmediate
					duration: FLT_MAX] autorelease];
		CHHapticPattern *pattern = [[[CHHapticPattern alloc] initWithEvents:[NSArray arrayWithObject:event] parameters:[[NSArray alloc] init] error:&error] autorelease];

		if (error != nil)
		{
			GAINPUT_LOG("Error creating Haptics Pattern for specific motor. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
			return;
		}

		self.pHapticPatternPlayer = [self.pHapticEngine createPlayerWithPattern:pattern error:&error];
		if (error != nil)
		{
			GAINPUT_LOG("Error creating Haptics Pattern Player. %s\n", [[error description] cStringUsingEncoding:NSASCIIStringEncoding]);
			return;
		}
		//ensure arc is satisfied.
		paramEvIntensity = nil;
		paramEvSharpness = nil;
		event = nil;
		pattern = nil;
		//ensure dynamic params are setup at least once.
		[self SetupDynamicHapticParams];
	}
}

@end

#endif //GAINPUT_GC_HAPTICS

#endif
