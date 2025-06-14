/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SkiaRecordingResult.h"

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)

namespace WebCore {

SkiaRecordingResult::SkiaRecordingResult(sk_sp<SkPicture>&& picture, SkiaImageToFenceMap&& imageToFenceMap, const IntRect& recordRect, RenderingMode renderingMode, bool contentsOpaque, float contentsScale)
    : m_picture(WTFMove(picture))
    , m_imageToFenceMap(WTFMove(imageToFenceMap))
    , m_recordRect(recordRect)
    , m_renderingMode(renderingMode)
    , m_contentsOpaque(contentsOpaque)
    , m_contentsScale(contentsScale)
{
}

SkiaRecordingResult::~SkiaRecordingResult() = default;

Ref<SkiaRecordingResult> SkiaRecordingResult::create(sk_sp<SkPicture>&& picture, SkiaImageToFenceMap&& imageToFenceMap, const IntRect& recordRect, RenderingMode renderingMode, bool contentsOpaque, float contentsScale)
{
    return adoptRef(*new SkiaRecordingResult(WTFMove(picture), WTFMove(imageToFenceMap), recordRect, renderingMode, contentsOpaque, contentsScale));
}

bool SkiaRecordingResult::hasFences()
{
    Locker locker { m_imageToFenceMapLock };
    return !m_imageToFenceMap.isEmpty();
}

void SkiaRecordingResult::waitForFenceIfNeeded(const SkImage& image)
{
    Locker locker { m_imageToFenceMapLock };
    if (auto fence = m_imageToFenceMap.get(&image))
        fence->serverWait();
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
