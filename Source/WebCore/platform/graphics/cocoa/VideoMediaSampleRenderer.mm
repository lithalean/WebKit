/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "VideoMediaSampleRenderer.h"

#import "EffectiveRateChangedListener.h"
#import "FourCC.h"
#import "IOSurface.h"
#import "Logging.h"
#import "MediaSampleAVFObjC.h"
#import "VP9UtilitiesCocoa.h"
#import "WebCoreDecompressionSession.h"
#import "WebSampleBufferVideoRendering.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CMFormatDescription.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Locker.h>
#import <wtf/MainThreadDispatcher.h>
#import <wtf/MonotonicTime.h>
#import <wtf/NativePromise.h>
#import <wtf/cf/TypeCastsCF.h>

#pragma mark - Soft Linking

#import "CoreVideoSoftLink.h"
#import "VideoToolboxSoftLink.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

@interface AVSampleBufferDisplayLayer (WebCoreAVSampleBufferDisplayLayerQueueManagementPrivate)
- (void)expectMinimumUpcomingSampleBufferPresentationTime:(CMTime)minimumUpcomingPresentationTime;
@end

#if ENABLE(LINEAR_MEDIA_PLAYER)
@interface AVSampleBufferVideoRenderer (Staging_127455709)
- (void)removeAllVideoTargets;
@end
#endif

#if HAVE(RECOMMENDED_PIXEL_ATTRIBUTES_API)
@interface AVSampleBufferVideoRenderer (Staging_152246223)
@property (readonly, nonnull) NSDictionary<NSString*, NS_SWIFT_SENDABLE id> *recommendedPixelBufferAttributes;
@end
#endif

// Equivalent to WTF_DECLARE_CF_TYPE_TRAIT(CMSampleBuffer);
// Needed due to requirement of specifying the PAL namespace.
template <>
struct WTF::CFTypeTrait<CMSampleBufferRef> {
    static inline CFTypeID typeID(void) { return PAL::CMSampleBufferGetTypeID(); }
};

namespace WebCore {

static constexpr CMItemCount SampleQueueHighWaterMark = 30;
static constexpr CMItemCount SampleQueueLowWaterMark = 15;

static constexpr MediaTime DecodeLowWaterMark { 100, 1000 };
static constexpr MediaTime DecodeHighWaterMark { 133, 1000 };

static bool isRendererThreadSafe(WebSampleBufferVideoRendering *renderering)
{
    if (!renderering)
        return true;
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return true;
#else
    return false;
#endif
}

static CMTime getDecodeTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetDecodeTimeStamp(sample);
}

static CMTime getPresentationTime(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetPresentationTimeStamp(sample);
}

static CMTime getDuration(CMBufferRef buf, void*)
{
    CMSampleBufferRef sample = checked_cf_cast<CMSampleBufferRef>(buf);
    return PAL::CMSampleBufferGetDuration(sample);
}

static CFComparisonResult compareBuffers(CMBufferRef buf1, CMBufferRef buf2, void* refcon)
{
    return (CFComparisonResult)PAL::CMTimeCompare(getPresentationTime(buf1, refcon), getPresentationTime(buf2, refcon));
}

static RetainPtr<CMBufferQueueRef> createBufferQueue()
{
    // CMBufferCallbacks contains 64-bit pointers that aren't 8-byte aligned. To suppress the linker
    // warning about this, we prepend 4 bytes of padding when building.
    const size_t padSize = 4;

#pragma pack(push, 4)
    struct BufferCallbacks {
        uint8_t pad[padSize];
        CMBufferCallbacks callbacks;
    } callbacks {
        { }, {
            0,
            nullptr,
            &getDecodeTime,
            &getPresentationTime,
            &getDuration,
            nullptr,
            &compareBuffers,
            nullptr,
            nullptr,
        }
    };
#pragma pack(pop)
    static_assert(sizeof(callbacks.callbacks.version) == sizeof(uint32_t), "Version field must be 4 bytes");
    static_assert(alignof(BufferCallbacks) == 4, "CMBufferCallbacks struct must have 4 byte alignment");

    static const CMItemCount kMaximumCapacity = 120;

    CMBufferQueueRef outQueue { nullptr };
    PAL::CMBufferQueueCreate(kCFAllocatorDefault, kMaximumCapacity, &callbacks.callbacks, &outQueue);
    return adoptCF(outQueue);
}

#define LogPerformance(...) RELEASE_LOG_DEBUG_IF(LOG_CHANNEL(MediaPerformance).level >= WTFLogLevel::Debug, MediaPerformance, __VA_ARGS__)

VideoMediaSampleRenderer::VideoMediaSampleRenderer(WebSampleBufferVideoRendering *renderer)
    : m_workQueue(isRendererThreadSafe(renderer) ? RefPtr { WorkQueue::create("VideoMediaSampleRenderer Queue"_s) } : nullptr)
    , m_frameRateMonitor([] (auto info) {
        RELEASE_LOG_ERROR(MediaPerformance, "VideoMediaSampleRenderer Frame decoding allowance exceeded:%f expected:%f", Seconds { info.lastFrameTime - info.frameTime }.value(), 1 / info.observedFrameRate);
    })
{
    if (!renderer)
        return;

    if (auto *displayLayer = dynamic_objc_cast<AVSampleBufferDisplayLayer>(renderer)) {
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
        m_renderer = [displayLayer sampleBufferRenderer];
#endif
        m_displayLayer = displayLayer;
        return;
    }
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    ASSERT(dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderer));
    m_renderer = dynamic_objc_cast<AVSampleBufferVideoRenderer>(renderer);
#endif
}

VideoMediaSampleRenderer::~VideoMediaSampleRenderer()
{
    assertIsMainThread();

    clearTimebase();

    flushCompressedSampleQueue();

    RefPtr<WebCoreDecompressionSession> decompressionSession = [&] {
        Locker lock { m_lock };
        return std::exchange(m_decompressionSession, nullptr);
    }();
    if (decompressionSession)
        decompressionSession->invalidate();

    if (auto renderer = this->renderer()) {
        [renderer flush];
        [renderer stopRequestingMediaData];
#if ENABLE(LINEAR_MEDIA_PLAYER)
        if ([m_renderer respondsToSelector:@selector(removeAllVideoTargets)])
            [m_renderer removeAllVideoTargets];
#endif
    }
}

RefPtr<WebCoreDecompressionSession> VideoMediaSampleRenderer::decompressionSession() const
{
    Locker lock { m_lock };
    return m_decompressionSession;
}

bool VideoMediaSampleRenderer::useDecompressionSessionForProtectedFallback() const
{
    return useDecompressionSessionForProtectedContent() || m_preferences.contains(VideoMediaSampleRendererPreference::ProtectedFallbackDisabled);
}

bool VideoMediaSampleRenderer::useDecompressionSessionForProtectedContent() const
{
    return m_preferences.contains(VideoMediaSampleRendererPreference::UseDecompressionSessionForProtectedContent);
}

size_t VideoMediaSampleRenderer::decodedSamplesCount() const
{
    return m_decodedSampleQueue ? PAL::CMBufferQueueGetBufferCount(m_decodedSampleQueue.get()) : 0;
}

RetainPtr<CMSampleBufferRef> VideoMediaSampleRenderer::nextDecodedSample() const
{
    if (!decodedSamplesCount())
        return nullptr;
    return static_cast<CMSampleBufferRef>(const_cast<void*>(PAL::CMBufferQueueGetHead(m_decodedSampleQueue.get())));
}

struct SampleCallbackData {
    bool isFirstSample { true };
    CMTime presentationTime = PAL::kCMTimeZero;
};

static OSStatus sampleCallback(CMBufferRef buffer, void* refcon)
{
    auto* data = static_cast<SampleCallbackData*>(refcon);
    if (data->isFirstSample) {
        data->isFirstSample = false;
        data->presentationTime = PAL::kCMTimePositiveInfinity;
        return 0;
    }
    data->presentationTime = PAL::CMSampleBufferGetPresentationTimeStamp(static_cast<CMSampleBufferRef>(const_cast<void*>(buffer)));
    return 1;
}

CMTime VideoMediaSampleRenderer::nextDecodedSampleEndTime() const
{
    SampleCallbackData data;
    PAL::CMBufferQueueCallForEachBuffer(m_decodedSampleQueue.get(), sampleCallback, &data);
    return data.presentationTime;
}

MediaTime VideoMediaSampleRenderer::lastDecodedSampleTime() const
{
    return PAL::toMediaTime(PAL::CMBufferQueueGetMaxPresentationTimeStamp(m_decodedSampleQueue.get()));
}

void VideoMediaSampleRenderer::enqueueDecodedSample(RetainPtr<CMSampleBufferRef>&& sample)
{
    ASSERT(sample);
    PAL::CMBufferQueueEnqueue(m_decodedSampleQueue.get(), sample.get());
}

bool VideoMediaSampleRenderer::isReadyForMoreMediaData() const
{
    assertIsMainThread();

    return areSamplesQueuesReadyForMoreMediaData(SampleQueueHighWaterMark) && (!renderer() || [renderer() isReadyForMoreMediaData]);
}

bool VideoMediaSampleRenderer::areSamplesQueuesReadyForMoreMediaData(size_t waterMark) const
{
    return m_compressedSamplesCount <= waterMark;
}

void VideoMediaSampleRenderer::maybeBecomeReadyForMoreMediaData()
{
    assertIsCurrent(dispatcher().get());

    RetainPtr renderer = rendererOrDisplayLayer();
    if (renderer && ![renderer isReadyForMoreMediaData]) {
        if (m_waitingForMoreMediaData)
            return;
        m_waitingForMoreMediaData = true;
        ThreadSafeWeakPtr weakThis { *this };
        [renderer requestMediaDataWhenReadyOnQueue:dispatchQueue() usingBlock:^{
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->dispatcher().get());
                protectedThis->m_waitingForMoreMediaData = false;
                [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
                protectedThis->maybeBecomeReadyForMoreMediaData();
            }
        }];
        return;
    }

    if (!areSamplesQueuesReadyForMoreMediaData(SampleQueueLowWaterMark))
        return;

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
        assertIsMainThread();
        if (RefPtr protectedThis = weakThis.get(); protectedThis && protectedThis->m_readyForMoreMediaDataFunction)
            protectedThis->m_readyForMoreMediaDataFunction();
    });
}

void VideoMediaSampleRenderer::stopRequestingMediaData()
{
    assertIsMainThread();

    m_readyForMoreMediaDataFunction = nullptr;

    if (isUsingDecompressionSession()) {
        // stopRequestingMediaData may deadlock if used on the main thread while enqueuing on the workqueue
        dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get()) {
                assertIsCurrent(protectedThis->dispatcher().get());
                protectedThis->m_waitingForMoreMediaData = false;
                [protectedThis->rendererOrDisplayLayer() stopRequestingMediaData];
            }
        });
        return;
    }
    [renderer() stopRequestingMediaData];
}

bool VideoMediaSampleRenderer::prefersDecompressionSession() const
{
    assertIsMainThread();

    return m_preferences.contains(VideoMediaSampleRendererPreference::PrefersDecompressionSession);
}

void VideoMediaSampleRenderer::setPreferences(Preferences preferences)
{
    assertIsMainThread();

    if (m_preferences == preferences || isUsingDecompressionSession())
        return;

    m_preferences = preferences;
}

void VideoMediaSampleRenderer::setTimebase(RetainPtr<CMTimebaseRef>&& timebase)
{
    if (!timebase)
        return;

    Locker locker { m_lock };

    ASSERT(!m_timebaseAndTimerSource.first);

    auto timerSource = adoptOSObject(dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatchQueue()));
    dispatch_source_set_event_handler(timerSource.get(), [weakThis = ThreadSafeWeakPtr { *this }] {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->purgeDecodedSampleQueue(protectedThis->m_flushId);
    });
    dispatch_activate(timerSource.get());
    PAL::CMTimebaseAddTimerDispatchSource(timebase.get(), timerSource.get());
    m_effectiveRateChangedListener = EffectiveRateChangedListener::create([weakThis = ThreadSafeWeakPtr { *this }, dispatcher = dispatcher()] {
        dispatcher->dispatch([weakThis] {
            if (RefPtr protectedThis = weakThis.get()) {
                RetainPtr timebase = protectedThis->timebase();
                if (!timebase)
                    return;
                if (PAL::CMTimebaseGetRate(timebase.get()))
                    protectedThis->purgeDecodedSampleQueue(protectedThis->m_flushId);
            }
        });
    }, timebase.get());
    m_timebaseAndTimerSource = { WTFMove(timebase), WTFMove(timerSource) };
}

void VideoMediaSampleRenderer::clearTimebase()
{
    Locker locker { m_lock };

    auto [timebase, timerSource] = std::exchange(m_timebaseAndTimerSource, { });
    if (!timebase)
        return;

    PAL::CMTimebaseRemoveTimerDispatchSource(timebase.get(), timerSource.get());
    dispatch_source_cancel(timerSource.get());
    m_effectiveRateChangedListener = nullptr;
}

VideoMediaSampleRenderer::TimebaseAndTimerSource VideoMediaSampleRenderer::timebaseAndTimerSource() const
{
    Locker locker { m_lock };

    return m_timebaseAndTimerSource;
}

RetainPtr<CMTimebaseRef> VideoMediaSampleRenderer::timebase() const
{
    Locker locker { m_lock };

    return m_timebaseAndTimerSource.first;
}

MediaTime VideoMediaSampleRenderer::currentTime() const
{
    if (RetainPtr timebase = this->timebase())
        return PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get()));
    return MediaTime::invalidTime();
}

void VideoMediaSampleRenderer::enqueueSample(const MediaSample& sample, const MediaTime& minimumUpcomingTime)
{
    assertIsMainThread();

    ASSERT(sample.platformSampleType() == PlatformSample::Type::CMSampleBufferType);
    if (sample.platformSampleType() != PlatformSample::Type::CMSampleBufferType)
        return;

    ASSERT(!m_needsFlushing);
    auto cmSampleBuffer = sample.platformSample().sample.cmSampleBuffer;

    bool needsDecompressionSession = false;
#if ENABLE(VP9)
    if (!isUsingDecompressionSession() && !m_currentCodec) {
        // Only use a decompression session for vp8 or vp9 when software decoded.
        CMVideoFormatDescriptionRef videoFormatDescription = PAL::CMSampleBufferGetFormatDescription(cmSampleBuffer);
        auto fourCC = PAL::CMFormatDescriptionGetMediaSubType(videoFormatDescription);
        needsDecompressionSession = fourCC == 'vp08' || (fourCC == 'vp09' && !vp9HardwareDecoderAvailable());
        m_currentCodec = fourCC;
    }
#endif

    if (!decompressionSession() && (!renderer() || ((prefersDecompressionSession() || needsDecompressionSession || isUsingDecompressionSession()) && (!sample.isProtected() || useDecompressionSessionForProtectedContent()))))
        initializeDecompressionSession();

    if (!isUsingDecompressionSession()) {
        [renderer() enqueueSampleBuffer:cmSampleBuffer];
        return;
    }

    if (!useDecompressionSessionForProtectedFallback() && !m_protectedContentEncountered && sample.isProtected()) {
        m_protectedContentEncountered = true;
#if !PLATFORM(WATCHOS)
        auto numberOfDroppedVideoFrames = [renderer() videoPerformanceMetrics].numberOfDroppedVideoFrames;
        if (m_droppedVideoFrames >= numberOfDroppedVideoFrames)
            m_droppedVideoFramesOffset = m_droppedVideoFrames - numberOfDroppedVideoFrames;
#endif
    }
    ++m_compressedSamplesCount;
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }, sample = Ref { sample }, minimumUpcomingTime, flushId = m_flushId.load()]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        assertIsCurrent(protectedThis->dispatcher().get());

        if (flushId != protectedThis->m_flushId) {
            protectedThis->m_compressedSamplesCount = 0;
            protectedThis->m_compressedSampleQueue.clear();
            protectedThis->maybeBecomeReadyForMoreMediaData();
            return;
        }
        protectedThis->m_compressedSampleQueue.append({ WTFMove(sample), minimumUpcomingTime, flushId });
        protectedThis->decodeNextSampleIfNeeded();
    });
}

void VideoMediaSampleRenderer::decodeNextSampleIfNeeded()
{
    assertIsCurrent(dispatcher().get());

    RefPtr decompressionSession = this->decompressionSession();

    if (m_isDecodingSample || m_gotDecodingError || !decompressionSession)
        return;

    if (m_compressedSampleQueue.isEmpty())
        return;

    RetainPtr timebase = this->timebase();
    MediaTime currentTime = timebase ? PAL::toMediaTime(PAL::CMTimebaseGetTime(timebase.get())) : MediaTime::invalidTime();
    double playbackRate = timebase ? PAL::CMTimebaseGetRate(timebase.get()) : 0;

    WebCoreDecompressionSession::DecodingFlags decodingFlags;

    if (currentTime.isValid() && !m_wasProtected) {
        auto lowWaterMarkTime = currentTime + DecodeLowWaterMark;
        auto highWaterMarkTime = currentTime + DecodeHighWaterMark;
        auto endTime = lastDecodedSampleTime();
        if (endTime.isValid() && endTime > highWaterMarkTime) {
            auto [sample, upcomingMinimum, flushId] = m_compressedSampleQueue.first();
            upcomingMinimum = std::min(sample->presentationTime(), upcomingMinimum.isValid() ? upcomingMinimum : MediaTime::positiveInfiniteTime());

            if (endTime < upcomingMinimum) {
                if (m_lastMinimumUpcomingPresentationTime.isInvalid() || upcomingMinimum != m_lastMinimumUpcomingPresentationTime) {
                    ASSERT(m_lastMinimumUpcomingPresentationTime.isInvalid() || m_lastMinimumUpcomingPresentationTime < upcomingMinimum);
                    m_lastMinimumUpcomingPresentationTime = upcomingMinimum;
                    LogPerformance("VideoMediaSampleRenderer::decodeNextSampleIfNeeded currentTime:%0.2f expectMinimumUpcomingSampleBufferPresentationTime:%0.2f decoded queued:%zu upcoming:%zu high watermark reached", currentTime.toDouble(), m_lastMinimumUpcomingPresentationTime.toDouble(), decodedSamplesCount(), m_compressedSampleQueue.size());
                    [rendererOrDisplayLayer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(m_lastMinimumUpcomingPresentationTime)];
                }
                return;
            }
            LogPerformance("Out of order frames detected, forcing extra decode");
        }
        if (endTime.isValid() && endTime >= lowWaterMarkTime && playbackRate > 0.9 && playbackRate < 1.1) {
            LogPerformance("VideoMediaSampleRenderer::decodeNextSampleIfNeeded expectMinimumUpcomingSampleBufferPresentationTime:%0.2f decoded queued:%zu upcoming:%zu currentTime:%0.2f endTime:%0.2f low:%0.2f high:%0.2f low watermark reached", m_lastMinimumUpcomingPresentationTime.toDouble(), decodedSamplesCount(), m_compressedSampleQueue.size(), currentTime.toDouble(), endTime.toDouble(), lowWaterMarkTime.toDouble(), highWaterMarkTime.toDouble());
            decodingFlags.add(WebCoreDecompressionSession::DecodingFlag::RealTime);
        }
    }

    auto [sample, upcomingMinimum, flushId] = m_compressedSampleQueue.takeFirst();
    m_compressedSamplesCount = m_compressedSampleQueue.size();
    maybeBecomeReadyForMoreMediaData();

    if (flushId != m_flushId)
        return decodeNextSampleIfNeeded();

    if (!shouldDecodeSample(sample)) {
        ++m_totalVideoFrames;
        ++m_droppedVideoFrames;

        decodeNextSampleIfNeeded();
        return;
    }

    ASSERT(m_lastMinimumUpcomingPresentationTime.isInvalid() || sample->isNonDisplaying() || sample->presentationTime() >= std::min(sample->presentationTime(), m_lastMinimumUpcomingPresentationTime));

    if (sample->isNonDisplaying())
        decodingFlags.add(WebCoreDecompressionSession::DecodingFlag::NonDisplaying);

    auto cmSample = sample->platformSample().sample.cmSampleBuffer;

    if (!useDecompressionSessionForProtectedFallback() && m_wasProtected != sample->isProtected()) {
        ASSERT(sample->isSync());
        RELEASE_LOG(Media, "Changing protection type (was:%d) content at:%0.2f", m_wasProtected, sample->presentationTime().toFloat());
        m_wasProtected = sample->isProtected();
    }
    if (!useDecompressionSessionForProtectedFallback() && m_wasProtected) {
        decodedFrameAvailable(cmSample, flushId);
        decodeNextSampleIfNeeded();
        return;
    }

    auto decodePromise = decompressionSession->decodeSample(cmSample, decodingFlags);
    m_isDecodingSample = true;
    decodePromise->whenSettled(dispatcher(), [weakThis = ThreadSafeWeakPtr { *this }, this, decodingFlags, flushId = flushId, startTime = MonotonicTime::now(), numberOfSamples = PAL::CMSampleBufferGetNumSamples(cmSample)](auto&& result) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        assertIsCurrent(dispatcher().get());

        m_isDecodingSample = false;

        if (flushId != m_flushId || (!result && result.error() == noErr)) {
            RELEASE_LOG(Media, "Decoder was flushed");
            decodeNextSampleIfNeeded();
            return;
        }

        m_totalVideoFrames += numberOfSamples;

        if (!result) {
            if (result.error() == kVTInvalidSessionErr) {
                RELEASE_LOG(Media, "VTDecompressionSession got invalidated, requesting flush");
                RefPtr decompressionSession = [&] {
                    Locker lock { m_lock };
                    return std::exchange(m_decompressionSession, nullptr);
                }();
                if (decompressionSession)
                    decompressionSession->invalidate();
                callOnMainThread([protectedThis] {
                    protectedThis->notifyVideoRendererRequiresFlushToResumeDecoding();
                });
                return;
            }

            m_gotDecodingError = true;
            ++m_corruptedVideoFrames;

            callOnMainThread([protectedThis, status = result.error()] {
                assertIsMainThread();

                if (protectedThis->renderer()) {
                    // Simulate AVSBDL decoding error.
                    RetainPtr error = [NSError errorWithDomain:@"com.apple.WebKit" code:status userInfo:nil];
                    NSDictionary *userInfoDict = @{ (__bridge NSString *)AVSampleBufferDisplayLayerFailedToDecodeNotificationErrorKey: (__bridge NSError *)error.get() };
                    [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferDisplayLayerFailedToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
                    [NSNotificationCenter.defaultCenter postNotificationName:AVSampleBufferVideoRendererDidFailToDecodeNotification object:protectedThis->renderer() userInfo:userInfoDict];
                    return;
                }
                protectedThis->notifyErrorHasOccurred(status);
            });
            return;
        }

        if (LOG_CHANNEL(MediaPerformance).level >= WTFLogLevel::Debug) {
            auto now = MonotonicTime::now();
            m_frameRateMonitor.update();
            OSType format = '----';
            MediaTime presentationTime = MediaTime::invalidTime();
            if (RetainPtr firstFrame = result->isEmpty() ? nullptr : (*result)[0]) {
                RetainPtr imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(static_cast<CMSampleBufferRef>(firstFrame.get()));
                format = CVPixelBufferGetPixelFormatType(imageBuffer.get());
                presentationTime = PAL::toMediaTime(PAL::CMSampleBufferGetOutputPresentationTimeStamp(firstFrame.get()));
            }
            LogPerformance("VideoMediaSampleRenderer pts:%0.2f minimum upcoming:%0.2f decoding rate:%0.1fHz rolling:%0.1f decoder rate:%0.1fHz compressed queue:%u decoded queue:%zu hw:%d format:%s", presentationTime.toDouble(), m_lastMinimumUpcomingPresentationTime.toDouble(), 1.0f / Seconds { now - std::exchange(m_timeSinceLastDecode, now) }.value(), m_frameRateMonitor.observedFrameRate(), 1.0f / Seconds { now - startTime }.value(), m_compressedSamplesCount.load(), decodedSamplesCount(), protectedThis->decompressionSession()->isHardwareAccelerated(), &FourCC(format).string()[0]);
        }

        if (!decodingFlags.contains(WebCoreDecompressionSession::DecodingFlag::NonDisplaying)) {
            for (auto& decodedFrame : *result) {
                if (decodedFrame)
                    decodedFrameAvailable(WTFMove(decodedFrame), flushId);
            }
        }

        decodeNextSampleIfNeeded();
    });
}

bool VideoMediaSampleRenderer::shouldDecodeSample(const MediaSample& sample)
{
    if (sample.isNonDisplaying())
        return true;

    auto currentTime = this->currentTime();
    if (currentTime.isInvalid())
        return true;

    if (sample.presentationEndTime() >= currentTime)
        return true;

    const CFArrayRef attachments = PAL::CMSampleBufferGetSampleAttachmentsArray(sample.platformSample().sample.cmSampleBuffer, false);
    if (!attachments)
        return true;

    for (CFIndex index = 0, count = CFArrayGetCount(attachments); index < count; ++index) {
        CFDictionaryRef attachmentDict = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, index);
        if (CFDictionaryGetValue(attachmentDict, PAL::kCMSampleAttachmentKey_IsDependedOnByOthers) == kCFBooleanFalse)
            return false;
    }

    return true;
}

void VideoMediaSampleRenderer::initializeDecompressionSession()
{
    assertIsMainThread();

    {
        Locker locker { m_lock };

        ASSERT(!m_decompressionSession);
        if (m_decompressionSession)
            return;

#if HAVE(RECOMMENDED_PIXEL_ATTRIBUTES_API)
        if (m_renderer) {
            if ([m_renderer.get() respondsToSelector:@selector(recommendedPixelBufferAttributes)])
                m_decompressionSession = WebCoreDecompressionSession::create([m_renderer recommendedPixelBufferAttributes]);
        }
#endif
        if (!m_decompressionSession)
            m_decompressionSession = WebCoreDecompressionSession::createOpenGL();
        m_isUsingDecompressionSession = true;
    }
    if (!m_decodedSampleQueue) {
        m_decodedSampleQueue = createBufferQueue();
        m_startupTime = MonotonicTime::now();
    }

    resetReadyForMoreMediaData();
}

void VideoMediaSampleRenderer::decodedFrameAvailable(RetainPtr<CMSampleBufferRef>&& sample, FlushId flushId)
{
    assignResourceOwner(sample.get());

    if (auto timebase = this->timebase()) {
        enqueueDecodedSample(WTFMove(sample));
        maybeReschedulePurge(flushId);
    } else
        maybeQueueFrameForDisplay(PAL::kCMTimeInvalid, sample.get(), flushId);
    [rendererOrDisplayLayer() enqueueSampleBuffer:sample.get()];
}

VideoMediaSampleRenderer::DecodedFrameResult VideoMediaSampleRenderer::maybeQueueFrameForDisplay(const CMTime& currentTime, CMSampleBufferRef sample, FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(sample);

    if (CMTIME_IS_VALID(currentTime)) {
        // Always display the first video frame available if we aren't displaying any yet, regardless of its time as it's always either:
        // 1- The first frame of the video.
        // 2- The first visible frame after a seek.

        if (m_lastDisplayedSample) {
            auto comparisonResult = PAL::CMTimeCompare(presentationTime, *m_lastDisplayedSample);
            if (comparisonResult < 0)
                return DecodedFrameResult::TooLate;
            if (!comparisonResult)
                return DecodedFrameResult::AlreadyDisplayed;
        }
        if (!m_forceLateSampleToBeDisplayed && m_isDisplayingSample && m_lastDisplayedTime && PAL::CMTimeCompare(presentationTime, *m_lastDisplayedTime) < 0)
            return DecodedFrameResult::TooLate;

        if (m_isDisplayingSample && PAL::CMTimeCompare(presentationTime, currentTime) > 0)
            return DecodedFrameResult::TooEarly;

        m_lastDisplayedTime = currentTime;
        m_lastDisplayedSample = presentationTime;
    }

    ++m_presentedVideoFrames;
    m_isDisplayingSample = true;
    m_forceLateSampleToBeDisplayed = false;

    notifyHasAvailableVideoFrame(PAL::toMediaTime(presentationTime), (MonotonicTime::now() - m_startupTime).seconds(), flushId);

    return DecodedFrameResult::Displayed;
}

void VideoMediaSampleRenderer::flushCompressedSampleQueue()
{
    assertIsMainThread();

    ++m_flushId;
    m_compressedSamplesCount = 0;
    m_gotDecodingError = false;
}

void VideoMediaSampleRenderer::flushDecodedSampleQueue()
{
    assertIsCurrent(dispatcher().get());
    ASSERT(m_decodedSampleQueue);

    PAL::CMBufferQueueReset(m_decodedSampleQueue.get());
    m_nextScheduledPurge.reset();
    m_lastDisplayedSample.reset();
    m_lastDisplayedTime.reset();
    m_isDisplayingSample = false;
    m_lastMinimumUpcomingPresentationTime = MediaTime::invalidTime();
}

void VideoMediaSampleRenderer::cancelTimer()
{
    schedulePurgeAtTime(PAL::kCMTimeInvalid);
}

void VideoMediaSampleRenderer::purgeDecodedSampleQueue(FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    if (!decompressionSession())
        return;

    if (!decodedSamplesCount()) {
        decodeNextSampleIfNeeded();
        return;
    }

    bool samplesPurged = false;

    auto timebase = this->timebase();
    if (timebase) {
        CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());

        samplesPurged = purgeDecodedSampleQueueUntilTime(currentTime);
        if (RetainPtr nextSample = nextDecodedSample()) {
            auto result = maybeQueueFrameForDisplay(currentTime, nextSample.get(), flushId);
#if !LOG_DISABLED
            if (LOG_CHANNEL(Media).level >= WTFLogLevel::Debug) {
                auto presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());
                auto presentationEndTime = nextDecodedSampleEndTime();
                auto resultLiteral = [](DecodedFrameResult result) {
                    switch (result) {
                    case DecodedFrameResult::TooEarly: return "tooEarly"_s;
                    case DecodedFrameResult::TooLate: return "tooLate"_s;
                    case DecodedFrameResult::AlreadyDisplayed: return "alreadyDisplayed"_s;
                    case DecodedFrameResult::Displayed: return "displayed"_s;
                    };
                }(result);
                LOG(Media, "maybeQueueFrameForDisplay: currentTime:%f start:%f end:%f result:%s", PAL::CMTimeGetSeconds(currentTime), PAL::CMTimeGetSeconds(presentationTime), PAL::CMTimeGetSeconds(presentationEndTime), resultLiteral.characters());
            }
#else
            UNUSED_VARIABLE(result);
#endif
        }
    }
    if (samplesPurged) {
        maybeBecomeReadyForMoreMediaData();
        decodeNextSampleIfNeeded();
    }
}

bool VideoMediaSampleRenderer::purgeDecodedSampleQueueUntilTime(const CMTime& currentTime)
{
    assertIsCurrent(dispatcher().get());

    if (!m_decodedSampleQueue)
        return false;

    CMTime nextPurgeTime = PAL::kCMTimeInvalid;
    bool samplesPurged = false;

    while (RetainPtr nextSample = nextDecodedSample()) {
        auto presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());
        auto presentationEndTime = nextDecodedSampleEndTime();

        if (PAL::CMTimeCompare(presentationEndTime, currentTime) >= 0) {
            nextPurgeTime = presentationEndTime;
            break;
        }

        if (m_lastDisplayedSample && PAL::CMTimeCompare(*m_lastDisplayedSample, presentationTime) < 0) {
            ++m_droppedVideoFrames; // This frame was never displayed.
            LOG(Media, "purgeDecodedSampleQueueUntilTime: currentTime:%f start:%f end:%f result:tooLate scheduled:%f (dropped:%u)", PAL::CMTimeGetSeconds(currentTime), PAL::CMTimeGetSeconds(presentationTime), PAL::CMTimeGetSeconds(presentationEndTime), PAL::CMTimeGetSeconds(m_nextScheduledPurge.value_or(PAL::kCMTimePositiveInfinity)), m_droppedVideoFrames.load());
        }

        RetainPtr sampleToBePurged = adoptCF(PAL::CMBufferQueueDequeueAndRetain(m_decodedSampleQueue.get()));
        sampleToBePurged = nil;
        samplesPurged = true;
    }

    if (!CMTIME_IS_VALID(nextPurgeTime))
        return samplesPurged;

    schedulePurgeAtTime(nextPurgeTime);

    return samplesPurged;
}

void VideoMediaSampleRenderer::schedulePurgeAtTime(const CMTime& nextPurgeTime)
{
    auto [timebase, timerSource] = timebaseAndTimerSource();
    if (!timebase)
        return;

    PAL::CMTimebaseSetTimerDispatchSourceNextFireTime(timebase.get(), timerSource.get(), nextPurgeTime, 0);

    if (CMTIME_IS_VALID(nextPurgeTime)) {
        assertIsCurrent(dispatcher().get());
        m_nextScheduledPurge = nextPurgeTime;
    }
}

void VideoMediaSampleRenderer::maybeReschedulePurge(FlushId flushId)
{
    assertIsCurrent(dispatcher().get());

    if (!decodedSamplesCount())
        return;
    auto presentationEndTime = nextDecodedSampleEndTime();
    if (m_nextScheduledPurge && PAL::CMTimeCompare(*m_nextScheduledPurge, presentationEndTime) <= 0)
        return;

    if ((m_nextScheduledPurge && CMTIME_IS_POSITIVE_INFINITY(*m_nextScheduledPurge)) || CMTIME_IS_POSITIVE_INFINITY(presentationEndTime)) {
        purgeDecodedSampleQueue(flushId);
        return;
    }
    schedulePurgeAtTime(presentationEndTime);
}

void VideoMediaSampleRenderer::flush()
{
    assertIsMainThread();
    [renderer() flush];

    if (!isUsingDecompressionSession())
        return;

    m_needsFlushing = false;
    cancelTimer();
    flushCompressedSampleQueue();

    if (RefPtr decompressionSession = this->decompressionSession())
        decompressionSession->flush();
    dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }]() mutable {
        if (RefPtr protectedThis = weakThis.get()) {
            protectedThis->flushDecodedSampleQueue();
            protectedThis->maybeBecomeReadyForMoreMediaData();
        }
    });
}

void VideoMediaSampleRenderer::requestMediaDataWhenReady(Function<void()>&& function)
{
    assertIsMainThread();
    m_readyForMoreMediaDataFunction = WTFMove(function);
    resetReadyForMoreMediaData();
}

void VideoMediaSampleRenderer::resetReadyForMoreMediaData()
{
    assertIsMainThread();

    if (!rendererOrDisplayLayer() || isUsingDecompressionSession()) {
        dispatcher()->dispatch([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->maybeBecomeReadyForMoreMediaData();
        });
        return;
    }

    ThreadSafeWeakPtr weakThis { *this };
    [renderer() requestMediaDataWhenReadyOnQueue:dispatch_get_main_queue() usingBlock:^{
        assertIsMainThread();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (![protectedThis->renderer() isReadyForMoreMediaData])
            return;
        if (protectedThis->m_readyForMoreMediaDataFunction)
            protectedThis->m_readyForMoreMediaDataFunction();
    }];
}

void VideoMediaSampleRenderer::expectMinimumUpcomingSampleBufferPresentationTime(const MediaTime& time)
{
    assertIsMainThread();
    if (isUsingDecompressionSession() || ![PAL::getAVSampleBufferDisplayLayerClass() instancesRespondToSelector:@selector(expectMinimumUpcomingSampleBufferPresentationTime:)])
        return;

    [renderer() expectMinimumUpcomingSampleBufferPresentationTime:PAL::toCMTime(time)];
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::renderer() const
{
    if (m_displayLayer) {
        assertIsMainThread();
        return m_displayLayer.get();
    }
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    return nil;
#endif
}

template <>
AVSampleBufferVideoRenderer* VideoMediaSampleRenderer::as() const
{
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    return nil;
#endif
}

WebSampleBufferVideoRendering *VideoMediaSampleRenderer::rendererOrDisplayLayer() const
{
#if HAVE(AVSAMPLEBUFFERVIDEORENDERER)
    return m_renderer.get();
#else
    if (m_displayLayer) {
        assertIsMainThread();
        return m_displayLayer.get();
    }
    return nil;
#endif
}

auto VideoMediaSampleRenderer::copyDisplayedPixelBuffer() -> DisplayedPixelBufferEntry
{
    assertIsMainThread();

    if (!isUsingDecompressionSession()) {
        RetainPtr buffer = adoptCF([renderer() copyDisplayedPixelBuffer]);
        if (auto surface = CVPixelBufferGetIOSurface(buffer.get()); surface && m_resourceOwner)
            IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
        return { WTFMove(buffer), MediaTime::invalidTime() };
    }

    RetainPtr<CVPixelBufferRef> imageBuffer;
    MediaTime presentationTimeStamp;

    ensureOnDispatcherSync([&] {
        assertIsCurrent(dispatcher().get());

        RetainPtr timebase = this->timebase();
        if (!timebase)
            return;

        m_forceLateSampleToBeDisplayed = true;
        purgeDecodedSampleQueue(m_flushId);

        auto nextSample = nextDecodedSample();
        if (!nextSample)
            return;
        CMTime currentTime = PAL::CMTimebaseGetTime(timebase.get());
        CMTime presentationTime = PAL::CMSampleBufferGetOutputPresentationTimeStamp(nextSample.get());

        if (PAL::CMTimeCompare(presentationTime, currentTime) > 0 && (!m_lastDisplayedSample || PAL::CMTimeCompare(presentationTime, *m_lastDisplayedSample) > 0))
            return;

        imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(nextSample.get());
        presentationTimeStamp = PAL::toMediaTime(presentationTime);
    });

    if (!imageBuffer)
        return { nullptr, MediaTime::invalidTime() };

    ASSERT(CFGetTypeID(imageBuffer.get()) == CVPixelBufferGetTypeID());
    if (CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return { nullptr, MediaTime::invalidTime() };
    return { WTFMove(imageBuffer), presentationTimeStamp };
}

unsigned VideoMediaSampleRenderer::totalDisplayedFrames() const
{
    return m_presentedVideoFrames;
}

unsigned VideoMediaSampleRenderer::totalVideoFrames() const
{
    if (isUsingDecompressionSession() && !m_protectedContentEncountered)
        return m_totalVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].totalNumberOfVideoFrames;
#endif
}

unsigned VideoMediaSampleRenderer::droppedVideoFrames() const
{
    assertIsMainThread();

    if (isUsingDecompressionSession() && !m_protectedContentEncountered)
        return m_droppedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfDroppedVideoFrames + m_droppedVideoFramesOffset;
#endif
}

unsigned VideoMediaSampleRenderer::corruptedVideoFrames() const
{
    if (isUsingDecompressionSession() && !m_protectedContentEncountered)
        return m_corruptedVideoFrames;
#if PLATFORM(WATCHOS)
    return 0;
#else
    return [renderer() videoPerformanceMetrics].numberOfCorruptedVideoFrames + m_corruptedVideoFrames;
#endif
}

MediaTime VideoMediaSampleRenderer::totalFrameDelay() const
{
    if (isUsingDecompressionSession() && !m_protectedContentEncountered)
        return m_totalFrameDelay;
#if PLATFORM(WATCHOS)
    return MediaTime::invalidTime();
#else
    return MediaTime::createWithDouble([renderer() videoPerformanceMetrics].totalFrameDelay);
#endif
}

void VideoMediaSampleRenderer::setResourceOwner(const ProcessIdentity& resourceOwner)
{
    m_resourceOwner = resourceOwner;
}

void VideoMediaSampleRenderer::assignResourceOwner(CMSampleBufferRef sampleBuffer)
{
    assertIsCurrent(dispatcher());
    if (!m_resourceOwner || !sampleBuffer)
        return;

    RetainPtr<CVPixelBufferRef> imageBuffer = (CVPixelBufferRef)PAL::CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer || CFGetTypeID(imageBuffer.get()) != CVPixelBufferGetTypeID())
        return;

    if (auto surface = CVPixelBufferGetIOSurface(imageBuffer.get()))
        IOSurface::setOwnershipIdentity(surface, m_resourceOwner);
}

void VideoMediaSampleRenderer::notifyWhenHasAvailableVideoFrame(Function<void(const MediaTime&, double)>&& callback)
{
    assertIsMainThread();

    m_hasAvailableFrameCallback = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyHasAvailableVideoFrame(const MediaTime& presentationTime, double displayTime, FlushId flushId)
{
    assertIsCurrent(dispatcher());

    callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }, flushId, presentationTime, displayTime] {
        if (RefPtr protectedThis = weakThis.get(); protectedThis && flushId == protectedThis->m_flushId) {
            assertIsMainThread();
            if (protectedThis->m_hasAvailableFrameCallback)
                protectedThis->m_hasAvailableFrameCallback(presentationTime, displayTime);
        }
    });
}

void VideoMediaSampleRenderer::notifyWhenDecodingErrorOccurred(Function<void(OSStatus)>&& callback)
{
    assertIsMainThread();
    m_errorOccurredFunction = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyWhenVideoRendererRequiresFlushToResumeDecoding(Function<void()>&& callback)
{
    assertIsMainThread();
    m_rendererNeedsFlushFunction = WTFMove(callback);
}

void VideoMediaSampleRenderer::notifyErrorHasOccurred(OSStatus status)
{
    assertIsMainThread();

    if (m_errorOccurredFunction)
        m_errorOccurredFunction(status);
}

void VideoMediaSampleRenderer::notifyVideoRendererRequiresFlushToResumeDecoding()
{
    assertIsMainThread();

    m_needsFlushing = true;
    if (m_rendererNeedsFlushFunction)
        m_rendererNeedsFlushFunction();
}

Ref<GuaranteedSerialFunctionDispatcher> VideoMediaSampleRenderer::dispatcher() const
{
    return m_workQueue ? *m_workQueue : static_cast<GuaranteedSerialFunctionDispatcher&>(MainThreadDispatcher::singleton());
}

dispatch_queue_t VideoMediaSampleRenderer::dispatchQueue() const
{
    return m_workQueue ? m_workQueue->dispatchQueue() : WorkQueue::protectedMain()->dispatchQueue();
}

void VideoMediaSampleRenderer::ensureOnDispatcher(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_workQueue)
        return m_workQueue->dispatch(WTFMove(function));
    callOnMainThread(WTFMove(function));
}

void VideoMediaSampleRenderer::ensureOnDispatcherSync(Function<void()>&& function) const
{
    if (dispatcher()->isCurrent()) {
        function();
        return;
    }

    if (m_workQueue)
        return m_workQueue->dispatchSync(WTFMove(function));
    callOnMainThreadAndWait(WTFMove(function));
}

#undef LogPerformance

} // namespace WebCore
