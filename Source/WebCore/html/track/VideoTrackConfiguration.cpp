/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "VideoTrackConfiguration.h"

#if ENABLE(VIDEO)

#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(VideoTrackConfiguration);

void VideoTrackConfiguration::setState(const VideoTrackConfigurationInit& state)
{
    if (m_state == state && m_colorSpace->state() == m_state.colorSpace)
        return;

    m_state = state;
    m_colorSpace->setState(m_state.colorSpace);
    notifyObservers();
}

void VideoTrackConfiguration::setCodec(String codec)
{
    if (m_state.codec == codec)
        return;

    m_state.codec = codec;
    notifyObservers();
}

void VideoTrackConfiguration::setWidth(uint32_t width)
{
    if (m_state.width == width)
        return;

    m_state.width = width;
    notifyObservers();
}

void VideoTrackConfiguration::setHeight(uint32_t height)
{
    if (m_state.height == height)
        return;

    m_state.height = height;
    notifyObservers();
}

void VideoTrackConfiguration::setColorSpace(Ref<VideoColorSpace>&& colorSpace)
{
    if (m_colorSpace == colorSpace)
        return;

    m_colorSpace = WTFMove(colorSpace);
    notifyObservers();
}

void VideoTrackConfiguration::setFramerate(double framerate)
{
    if (m_state.framerate == framerate)
        return;

    m_state.framerate = framerate;
    notifyObservers();
}

void VideoTrackConfiguration::setBitrate(uint64_t bitrate)
{
    if (m_state.bitrate == bitrate)
        return;

    m_state.bitrate = bitrate;
    notifyObservers();
}

void VideoTrackConfiguration::setSpatialVideoMetadata(std::optional<SpatialVideoMetadata> metadata)
{
    if (m_state.spatialVideoMetadata == metadata)
        return;

    m_state.spatialVideoMetadata = metadata;
    notifyObservers();
}

void VideoTrackConfiguration::setVideoProjectionMetadata(std::optional<VideoProjectionMetadata> metadata)
{
    if (m_state.videoProjectionMetadata == metadata)
        return;

    m_state.videoProjectionMetadata = metadata;
    notifyObservers();
}

void VideoTrackConfiguration::notifyObservers()
{
    m_observers.forEach([] (auto& observer) {
        observer();
    });
}

Ref<JSON::Object> VideoTrackConfiguration::toJSON() const
{
    Ref json = JSON::Object::create();
    json->setString("codec"_s, codec());
    json->setInteger("width"_s, width());
    json->setInteger("height"_s, height());
    json->setObject("colorSpace"_s, colorSpace()->toJSON());
    json->setDouble("framerate"_s, framerate());
    json->setInteger("bitrate"_s, bitrate());
    json->setBoolean("isSpatial"_s, !!spatialVideoMetadata());
    json->setBoolean("isImmersive"_s, !!videoProjectionMetadata());
    return json;
}

}

#endif
