/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebCoreDecompressionSession.h"

#import "FormatDescriptionUtilities.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "PixelBufferConformerCV.h"
#import "VideoDecoder.h"
#import "VideoFrame.h"
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <wtf/BlockPtr.h>
#import <wtf/MainThread.h>
#import <wtf/MediaTime.h>
#import <wtf/NativePromise.h>
#import <wtf/RunLoop.h>
#import <wtf/StringPrintStream.h>
#import <wtf/Vector.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cf/VectorCF.h>

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

static bool canCopyFormatDescriptionExtension()
{
    static bool canCopyFormatDescriptionExtension = false;
#if PLATFORM(VISION)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        canCopyFormatDescriptionExtension = PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_HasLeftStereoEyeView()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_HasRightStereoEyeView()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_HeroEye()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_HorizontalDisparityAdjustment()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_StereoCameraBaseline()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ProjectionKind()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_ViewPackingKind()
            && PAL::canLoad_CoreMedia_kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection();
    });
#endif
    return canCopyFormatDescriptionExtension;
}

Ref<WebCoreDecompressionSession> WebCoreDecompressionSession::createOpenGL()
{
    return adoptRef(*new WebCoreDecompressionSession(nullptr));
}

Ref<WebCoreDecompressionSession> WebCoreDecompressionSession::createRGB()
{
    return adoptRef(*new WebCoreDecompressionSession(@{
        (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA),
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ }
    }));
}

NSDictionary *WebCoreDecompressionSession::defaultPixelBufferAttributes()
{
    return @{
        (__bridge NSString *)kCVPixelBufferIOSurfaceCoreAnimationCompatibilityKey: (id)kCFBooleanTrue,
        (__bridge NSString *)kCVPixelBufferPreferRealTimeCacheModeIfEveryoneDoesKey: (id)kCFBooleanTrue,
        (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey: @{ /* empty dictionary */ }
    };
}

WebCoreDecompressionSession::WebCoreDecompressionSession(NSDictionary *pixelBufferAttributes)
    : m_decompressionQueue(WorkQueue::create("WebCoreDecompressionSession Decompression Queue"_s))
    , m_pixelBufferAttributes(pixelBufferAttributes ? pixelBufferAttributes : defaultPixelBufferAttributes())
{
}

WebCoreDecompressionSession::~WebCoreDecompressionSession() = default;

void WebCoreDecompressionSession::invalidate()
{
    assertIsMainThread();
    m_invalidated = true;
    Locker lock { m_lock };
    m_decompressionQueue->dispatch([decoder = WTFMove(m_videoDecoder)] {
        if (decoder)
            decoder->close();
    });
}

void WebCoreDecompressionSession::assignResourceOwner(CVImageBufferRef imageBuffer)
{
    if (!m_resourceOwner || !imageBuffer || CFGetTypeID(imageBuffer) != CVPixelBufferGetTypeID())
        return;
    if (auto surface = CVPixelBufferGetIOSurface((CVPixelBufferRef)imageBuffer))
        IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
}

Expected<RetainPtr<VTDecompressionSessionRef>, OSStatus> WebCoreDecompressionSession::ensureDecompressionSessionForSample(CMSampleBufferRef cmSample)
{
    Locker lock { m_lock };

    if (isInvalidated())
        return makeUnexpected(kVTInvalidSessionErr);

    if (m_videoDecoder)
        return RetainPtr<VTDecompressionSessionRef> { };

    CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSample);
    if (m_decompressionSession && !VTDecompressionSessionCanAcceptFormatDescription(m_decompressionSession.get(), videoFormatDescription)) {
        auto status = VTDecompressionSessionWaitForAsynchronousFrames(m_decompressionSession.get());
        Ref sample = MediaSampleAVFObjC::create(cmSample, 0);
        m_decompressionSession = nullptr;
        m_isHardwareAccelerated.reset();
        if (!(sample->flags() & MediaSample::IsSync)) {
            RELEASE_LOG_INFO(Media, "VTDecompressionSession can't accept format description change on non-keyframe status:%d", int(status));
            return makeUnexpected(status == kVTInvalidSessionErr ? status : kVTVideoDecoderBadDataErr);
        }
        RELEASE_LOG_INFO(Media, "VTDecompressionSession can't accept format description change on keyframe, creating new VTDecompressionSession status:%d", int(status));
    }

    if (!m_decompressionSession) {
        auto videoDecoderSpecification = @{ (__bridge NSString *)kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder: @YES };
        ASSERT(m_pixelBufferAttributes);

        VTDecompressionSessionRef decompressionSessionOut = nullptr;
        auto result = VTDecompressionSessionCreate(kCFAllocatorDefault, videoFormatDescription, (__bridge CFDictionaryRef)videoDecoderSpecification, (__bridge CFDictionaryRef)m_pixelBufferAttributes.get(), nullptr, &decompressionSessionOut);
        if (noErr == result) {
            m_decompressionSession = adoptCF(decompressionSessionOut);
            CFArrayRef rawSuggestedQualityOfServiceTiers = nullptr;
            VTSessionCopyProperty(decompressionSessionOut, kVTDecompressionPropertyKey_SuggestedQualityOfServiceTiers, kCFAllocatorDefault, &rawSuggestedQualityOfServiceTiers);
        }
    }

    return m_decompressionSession;
}

static bool isNonRecoverableError(OSStatus status)
{
    return status != noErr && status != kVTVideoDecoderReferenceMissingErr;
}

static RetainPtr<CMVideoFormatDescriptionRef> copyDescriptionExtensionValuesIfNeeded(CMVideoFormatDescriptionRef imageDescription, const CMVideoFormatDescriptionRef originalDescription)
{
    if (!canCopyFormatDescriptionExtension())
        return imageDescription;

    static CFStringRef keys[] = {
        PAL::kCMFormatDescriptionExtension_CameraCalibrationDataLensCollection,
        PAL::kCMFormatDescriptionExtension_HasLeftStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HasRightStereoEyeView,
        PAL::kCMFormatDescriptionExtension_HeroEye,
        PAL::kCMFormatDescriptionExtension_HorizontalFieldOfView,
        PAL::kCMFormatDescriptionExtension_HorizontalDisparityAdjustment,
        PAL::kCMFormatDescriptionExtension_StereoCameraBaseline,
        PAL::kCMFormatDescriptionExtension_ProjectionKind,
        PAL::kCMFormatDescriptionExtension_ViewPackingKind
    };
    static constexpr size_t numberOfKeys = sizeof(keys) / sizeof(keys[0]);
    auto keysSpan = unsafeMakeSpan(keys, numberOfKeys);
    size_t keysSet = 0;
    Vector<RetainPtr<CFPropertyListRef>, numberOfKeys> values(numberOfKeys, [&](size_t index) -> RetainPtr<CFPropertyListRef> {
        RetainPtr value = PAL::CMFormatDescriptionGetExtension(originalDescription, keysSpan[index]);
        if (!value)
            return nullptr;
        keysSet++;
        return value;
    });

    if (!keysSet)
        return imageDescription;

    RetainPtr<CFMutableDictionaryRef> copyExtensions;
    if (RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(imageDescription))
        copyExtensions = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(extensions.get()) + keysSet, extensions.get()));
    else
        copyExtensions = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, keysSet, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    for (size_t index = 0; index < numberOfKeys; index++) {
        if (values[index])
            CFDictionarySetValue(copyExtensions.get(), keysSpan[index], values[index].get());
    }

    auto codecType = PAL::CMFormatDescriptionGetMediaSubType(imageDescription);
    auto dimensions = PAL::CMVideoFormatDescriptionGetDimensions(imageDescription);

    CMVideoFormatDescriptionRef newImageDescription;
    if (auto status = PAL::CMVideoFormatDescriptionCreate(kCFAllocatorDefault, codecType, dimensions.width, dimensions.height, copyExtensions.get(), &newImageDescription); status != noErr)
        return imageDescription;
    return adoptCF(newImageDescription);
}

static Expected<RetainPtr<CMSampleBufferRef>, OSStatus> handleDecompressionOutput(WebCoreDecompressionSession::DecodingFlags flags, OSStatus status, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration, CMVideoFormatDescriptionRef description = nullptr)
{
    if (isNonRecoverableError(status)) {
        RELEASE_LOG_ERROR(Media, "Video sample decompression failed with error:%d", int(status));
        return makeUnexpected(status);
    }

    if (flags.contains(WebCoreDecompressionSession::DecodingFlag::NonDisplaying) || !imageBuffer)
        return { };

    CMVideoFormatDescriptionRef rawImageBufferDescription = nullptr;
    if (auto status = PAL::CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, imageBuffer, &rawImageBufferDescription); status != noErr)
        return makeUnexpected(status);
    RetainPtr<CMVideoFormatDescriptionRef> imageBufferDescription = copyDescriptionExtensionValuesIfNeeded(rawImageBufferDescription, description);

    CMSampleTimingInfo imageBufferTiming {
        presentationDuration,
        presentationTimeStamp,
        presentationTimeStamp,
    };

    CMSampleBufferRef rawImageSampleBuffer = nullptr;
    if (auto status = PAL::CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, imageBuffer, imageBufferDescription.get(), &imageBufferTiming, &rawImageSampleBuffer); status != noErr)
        return makeUnexpected(status);

    return adoptCF(rawImageSampleBuffer);
}

auto WebCoreDecompressionSession::decodeSample(CMSampleBufferRef sample, DecodingFlags flags) -> Ref<DecodingPromise>
{
    DecodingPromise::Producer producer;
    auto promise = producer.promise();
    m_decompressionQueue->dispatch([protectedThis = RefPtr { this }, producer = WTFMove(producer), sample = RetainPtr { sample }, flags, flushId = m_flushId.load()]() mutable {
        if (flushId == protectedThis->m_flushId)
            protectedThis->decodeSampleInternal(sample.get(), flags)->chainTo(WTFMove(producer));
        else
            producer.reject(noErr);
    });
    return promise;
}

Ref<WebCoreDecompressionSession::DecodingPromise> WebCoreDecompressionSession::decodeSampleInternal(CMSampleBufferRef sample, DecodingFlags flags)
{
    assertIsCurrent(m_decompressionQueue.get());

    m_lastDecodingError = noErr;
    size_t numberOfSamples = PAL::CMSampleBufferGetNumSamples(sample);
    m_lastDecodedSamples = { };
    m_lastDecodedSamples.reserveInitialCapacity(numberOfSamples);

    auto result = ensureDecompressionSessionForSample(sample);
    if (!result)
        return DecodingPromise::createAndReject(result.error());
    RetainPtr decompressionSession = WTFMove(*result);

    if (!decompressionSession && !m_videoDecoderCreationFailed) {
        RefPtr<MediaPromise> initPromise;

        {
            Locker lock { m_lock };
            if (!m_videoDecoder) {
                if (isInvalidated())
                    return DecodingPromise::createAndReject(0);
                RetainPtr videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(sample);
                auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription.get());

                RetainPtr extensions = PAL::CMFormatDescriptionGetExtensions(videoFormatDescription.get());
                if (!extensions)
                    return DecodingPromise::createAndReject(0);

                RetainPtr extensionAtoms = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(extensions.get(), PAL::kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms));
                if (!extensionAtoms)
                    return DecodingPromise::createAndReject(0);

                // We should only hit this code path for VP8 and SW VP9 decoder, look for the vpcC path.
                RetainPtr configurationRecord = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(extensionAtoms.get(), CFSTR("vpcC")));
                if (!configurationRecord)
                    return DecodingPromise::createAndReject(0);

                auto colorSpace = colorSpaceFromFormatDescription(videoFormatDescription.get());

                initPromise = initializeVideoDecoder(fourCC, span(configurationRecord.get()), colorSpace);
            }
        }
        auto decode = [protectedThis = Ref { *this }, this, cmSamples = RetainPtr { sample }, flags] {
            Locker lock { m_lock };
            RefPtr videoDecoder = m_videoDecoder;
            if (!videoDecoder)
                return DecodingPromise::createAndReject(0);

            assertIsCurrent(m_decompressionQueue.get());

            m_pendingDecodeData = { flags };
            MediaTime totalDuration = PAL::toMediaTime(PAL::CMSampleBufferGetDuration(cmSamples.get()));

            Vector<Ref<VideoDecoder::DecodePromise>> promises;
            for (Ref sample : MediaSampleAVFObjC::create(cmSamples.get(), 0)->divide()) {
                auto cmSample = sample->platformSample().sample.cmSampleBuffer;
                MediaTime presentationTimestamp = PAL::toMediaTime(PAL::CMSampleBufferGetPresentationTimeStamp(cmSample));
                CMBlockBufferRef rawBuffer = PAL::CMSampleBufferGetDataBuffer(cmSample);
                ASSERT(rawBuffer);
                RetainPtr buffer = rawBuffer;
                // Make sure block buffer is contiguous.
                if (!PAL::CMBlockBufferIsRangeContiguous(rawBuffer, 0, 0)) {
                    CMBlockBufferRef contiguousBuffer;
                    if (auto status = PAL::CMBlockBufferCreateContiguous(nullptr, rawBuffer, nullptr, nullptr, 0, 0, 0, &contiguousBuffer))
                        return DecodingPromise::createAndReject(status);
                    buffer = adoptCF(contiguousBuffer);
                }
                auto data = PAL::CMBlockBufferGetDataSpan(buffer.get());
                if (!data.data())
                    return DecodingPromise::createAndReject(-1);
                promises.append(videoDecoder->decode({ data, true, presentationTimestamp.toMicroseconds(), 0 }));
            }
            DecodingPromise::Producer producer;
            auto promise = producer.promise();
            VideoDecoder::DecodePromise::all(promises)->whenSettled(m_decompressionQueue.get(), [weakThis = ThreadSafeWeakPtr { *this }, totalDuration = PAL::toCMTime(totalDuration), producer = WTFMove(producer)] (auto&& result) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis || protectedThis->isInvalidated()) {
                    producer.reject(0);
                    return;
                }
                assertIsCurrent(protectedThis->m_decompressionQueue.get());
                if (protectedThis->m_lastDecodingError)
                    producer.reject(protectedThis->m_lastDecodingError);
                else if (!result)
                    producer.reject(kVTVideoDecoderBadDataErr);
                else
                    producer.resolve(std::exchange(protectedThis->m_lastDecodedSamples, { }));
                if (!protectedThis->m_pendingDecodeData)
                    return;
                protectedThis->m_pendingDecodeData.reset();
            });
            return promise;
        };
        if (initPromise) {
            return initPromise->then(m_decompressionQueue, WTFMove(decode), [] {
                return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);
            });
        }
        return decode();
    }

    if (!decompressionSession)
        return DecodingPromise::createAndReject(kVTVideoDecoderNotAvailableNowErr);

    DecodingPromise::Producer producer;
    auto promise = producer.promise();

    auto handler = makeBlockPtr([flags, producer = WTFMove(producer), numberOfTimesCalled = 0u, numberOfSamples, decodedSamples = std::exchange(m_lastDecodedSamples, { }), description = RetainPtr { PAL::CMSampleBufferGetFormatDescription(sample) }](OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer, CMTime presentationTimeStamp, CMTime presentationDuration) mutable {
        if (producer.isSettled())
            return;
        auto result = handleDecompressionOutput(flags, status, infoFlags, imageBuffer, presentationTimeStamp, presentationDuration, description.get());
        if (!result) {
            producer.reject(result.error());
            return;
        }
        decodedSamples.append(WTFMove(*result));
        if (++numberOfTimesCalled == numberOfSamples)
            producer.resolve(std::exchange(decodedSamples, { }));
    });

    VTDecodeInfoFlags decodeInfoFlags = kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_EnableTemporalProcessing;
    if (flags.contains(DecodingFlag::NonDisplaying))
        decodeInfoFlags |= kVTDecodeFrame_DoNotOutputFrame;
    if (flags.contains(DecodingFlag::RealTime))
        decodeInfoFlags |= kVTDecodeFrame_1xRealTimePlayback;

    if (auto result = VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, decodeInfoFlags, nullptr, handler.get()); result != noErr)
        handler(result, 0, nullptr, PAL::kCMTimeInvalid, PAL::kCMTimeInvalid); // If VTDecompressionSessionDecodeFrameWithOutputHandler returned an error, the handler would not have been called.

    return promise;
}

RetainPtr<CVPixelBufferRef> WebCoreDecompressionSession::decodeSampleSync(CMSampleBufferRef sample)
{
    auto result = ensureDecompressionSessionForSample(sample);
    if (!result || !*result)
        return nullptr;

    RetainPtr decompressionSession = WTFMove(*result);
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    VTDecodeInfoFlags flags { 0 };
    WTF::Semaphore syncDecompressionOutputSemaphore { 0 };
    Ref protectedThis { *this };
    VTDecompressionSessionDecodeFrameWithOutputHandler(decompressionSession.get(), sample, flags, nullptr, [&protectedThis, &pixelBuffer, &syncDecompressionOutputSemaphore] (OSStatus, VTDecodeInfoFlags, CVImageBufferRef imageBuffer, CMTime, CMTime) mutable {
        protectedThis->assignResourceOwner(imageBuffer);
        if (imageBuffer && CFGetTypeID(imageBuffer) == CVPixelBufferGetTypeID())
            pixelBuffer = (CVPixelBufferRef)imageBuffer;
        syncDecompressionOutputSemaphore.signal();
    });
    syncDecompressionOutputSemaphore.wait();
    return pixelBuffer;
}

void WebCoreDecompressionSession::flush()
{
    m_flushId++;
}

Ref<MediaPromise> WebCoreDecompressionSession::initializeVideoDecoder(FourCharCode codec, std::span<const uint8_t> description, const std::optional<PlatformVideoColorSpace>& colorSpace)
{
    VideoDecoder::Config config {
        .description = description,
        .colorSpace = colorSpace,
        .decoding = VideoDecoder::HardwareAcceleration::Yes,
        .pixelBuffer = VideoDecoder::HardwareBuffer::Yes,
        .noOutputAsError = VideoDecoder::TreatNoOutputAsError::No
    };
    MediaPromise::Producer producer;
    auto promise = producer.promise();

    VideoDecoder::create(VideoDecoder::fourCCToCodecString(codec), config, [weakThis = ThreadSafeWeakPtr { *this }, queue = m_decompressionQueue] (auto&& result) {
        queue->dispatch([weakThis, result = WTFMove(result)] () {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->m_decompressionQueue.get());
                if (protectedThis->isInvalidated() || !protectedThis->m_pendingDecodeData)
                    return;

                if (!result) {
                    protectedThis->m_lastDecodingError = -1;
                    return;
                }
                if (protectedThis->m_lastDecodingError)
                    return;

                auto presentationTime = PAL::toCMTime(MediaTime(result->timestamp, 1000000));
                auto presentationDuration = PAL::toCMTime(MediaTime(result->duration.value_or(0), 1000000));
                auto sampleResult = handleDecompressionOutput(protectedThis->m_pendingDecodeData->flags, noErr, 0, result->frame->pixelBuffer(), presentationTime, presentationDuration);
                if (!sampleResult)
                    protectedThis->m_lastDecodingError = sampleResult.error();
                else
                    protectedThis->m_lastDecodedSamples.append(WTFMove(*sampleResult));
            }
        });
    })->whenSettled(m_decompressionQueue, [protectedThis = Ref { *this }, this, producer = WTFMove(producer), queue = m_decompressionQueue] (auto&& result) mutable {
        assertIsCurrent(m_decompressionQueue.get());
        if (!result || isInvalidated()) {
            producer.reject(PlatformMediaError::DecoderCreationError);
            return;
        }
        Locker lock { m_lock };
        m_videoDecoder = WTFMove(*result);
        producer.resolve();
    });

    return promise;
}

bool WebCoreDecompressionSession::isHardwareAccelerated() const
{
    Locker lock { m_lock };
    if (m_videoDecoder)
        return false;
    if (m_isHardwareAccelerated)
        return *m_isHardwareAccelerated;
    if (!m_decompressionSession)
        return false;
    CFBooleanRef isHardwareAccelerated = NULL;
    VTSessionCopyProperty(m_decompressionSession.get(), kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, kCFAllocatorDefault, &isHardwareAccelerated);
    m_isHardwareAccelerated = isHardwareAccelerated && isHardwareAccelerated == kCFBooleanTrue;
    return *m_isHardwareAccelerated;
}

} // namespace WebCore
