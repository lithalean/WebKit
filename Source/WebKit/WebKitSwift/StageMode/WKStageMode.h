/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <wtf/Platform.h>

#if ENABLE(MODEL_PROCESS)

#import "WKRKEntity.h"
#import <simd/simd.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

typedef NS_ENUM(NSInteger, WKStageModeOperation) {
    WKStageModeOperationNone = 0,
    WKStageModeOperationOrbit,
};

NS_SWIFT_UI_ACTOR
@protocol WKStageModeInteractionAware <NSObject>
- (void)stageModeInteractionDidUpdateModel;
@end

NS_SWIFT_UI_ACTOR
@interface WKStageModeInteractionDriver : NSObject
@property (nonatomic, readonly) REEntityRef interactionContainerRef;
@property (nonatomic, readonly) bool stageModeInteractionInProgress;

- (instancetype)initWithModel:(WKRKEntity *)model container:(REEntityRef)container delegate:(id<WKStageModeInteractionAware> _Nullable)delegate;
- (void)setContainerTransformInPortal;
- (void)interactionDidBegin:(simd_float4x4)transform;
- (void)interactionDidUpdate:(simd_float4x4)transform;
- (void)interactionDidEnd;
- (void)operationDidUpdate:(WKStageModeOperation)operation;
- (void)removeInteractionContainerFromSceneOrParent;
@end

NS_HEADER_AUDIT_END(nullability, sendability)

#endif // ENABLE(MODEL_PROCESS)
