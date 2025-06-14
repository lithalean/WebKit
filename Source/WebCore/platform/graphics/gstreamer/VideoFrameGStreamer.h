/*
 * Copyright (C) 2022 Igalia S.L
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "VideoFrame.h"
#include "VideoFrameMetadataGStreamer.h"
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GstSample GstSample;

namespace WebCore {

class PixelBuffer;
class IntSize;
class ImageGStreamer;

using DMABufFormat = std::pair<uint32_t, uint64_t>;

class VideoFrameGStreamer final : public VideoFrame {
public:
    struct Info {
        GstVideoInfo info;
        std::optional<DMABufFormat> dmaBufFormat { std::nullopt };
    };
    static Info infoFromCaps(const GRefPtr<GstCaps>&);

    static Ref<VideoFrameGStreamer> create(GRefPtr<GstSample>&&, std::optional<Info>&&, const IntSize& presentationSize, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation videoRotation = Rotation::None, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& = std::nullopt, std::optional<PlatformVideoColorSpace>&& = std::nullopt);

    static Ref<VideoFrameGStreamer> createWrappedSample(const GRefPtr<GstSample>&, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation videoRotation = Rotation::None);

    static RefPtr<VideoFrameGStreamer> createFromPixelBuffer(Ref<PixelBuffer>&&, Rotation videoRotation = VideoFrame::Rotation::None, const MediaTime& presentationTime = MediaTime::invalidTime(), const IntSize& destinationSize = { }, double frameRate = 1, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& metadata = std::nullopt, PlatformVideoColorSpace&& = { });

    void setFrameRate(double);
    void setMaxFrameRate(double);

    void setPresentationTime(const MediaTime&);

    RefPtr<VideoFrameGStreamer> resizeTo(const IntSize&);

    GRefPtr<GstSample> resizedSample(const IntSize&);

    GRefPtr<GstSample> downloadSample(std::optional<GstVideoFormat> = { });

    GstSample* sample() const { return m_sample.get(); }

    RefPtr<ImageGStreamer> convertToImage();

    IntSize presentationSize() const final { return m_presentationSize; }
    uint32_t pixelFormat() const final { return GST_VIDEO_INFO_FORMAT(&m_info.info); }

    enum class MemoryType : uint8_t {
        Unsupported,
        System,
#if USE(GSTREAMER_GL)
        GL,
#if USE(GBM)
        DMABuf
#endif
#endif
    };
    MemoryType memoryType() const { return m_memoryType; }

    const GstVideoInfo& info() const { return m_info.info; }
    std::optional<DMABufFormat> dmaBufFormat() const { return m_info.dmaBufFormat; }

private:
    VideoFrameGStreamer(GRefPtr<GstSample>&&, std::optional<Info>&&, const IntSize& presentationSize, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation = Rotation::None, bool videoMirrored = false, std::optional<VideoFrameTimeMetadata>&& = std::nullopt, PlatformVideoColorSpace&& = { });
    VideoFrameGStreamer(const GRefPtr<GstSample>&, Info&&, const IntSize& presentationSize, const MediaTime& presentationTime = MediaTime::invalidTime(), Rotation = Rotation::None, PlatformVideoColorSpace&& = { });

    bool isGStreamer() const final { return true; }
    Ref<VideoFrame> clone() final;

    GRefPtr<GstSample> convert(GstVideoFormat, const IntSize&);

    void setMemoryTypeFromCaps();

    GRefPtr<GstSample> m_sample;
    Info m_info;
    std::optional<DMABufFormat> m_dmaBufFormat;
    IntSize m_presentationSize;
    MemoryType m_memoryType;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::VideoFrameGStreamer)
static bool isType(const WebCore::VideoFrame& frame) { return frame.isGStreamer(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
