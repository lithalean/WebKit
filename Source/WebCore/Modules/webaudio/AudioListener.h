/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioArray.h"
#include "FloatPoint3D.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class AudioParam;
class BaseAudioContext;
template<typename> class ExceptionOr;

// AudioListener maintains the state of the listener in the audio scene as defined in the OpenAL specification.

class AudioListener : public RefCounted<AudioListener> {
public:
    static Ref<AudioListener> create(BaseAudioContext& context)
    {
        return adoptRef(*new AudioListener(context));
    }
    ~AudioListener();

    AudioParam& positionX() { return m_positionX.get(); }
    AudioParam& positionY() { return m_positionY.get(); }
    AudioParam& positionZ() { return m_positionZ.get(); }
    AudioParam& forwardX() { return m_forwardX.get(); }
    AudioParam& forwardY() { return m_forwardY.get(); }
    AudioParam& forwardZ() { return m_forwardZ.get(); }
    AudioParam& upX() { return m_upX.get(); }
    AudioParam& upY() { return m_upY.get(); }
    AudioParam& upZ() { return m_upZ.get(); }

    // Position
    ExceptionOr<void> setPosition(float x, float y, float z);
    FloatPoint3D position() const;

    // Orientation
    ExceptionOr<void> setOrientation(float x, float y, float z, float upX, float upY, float upZ);
    FloatPoint3D orientation() const;

    FloatPoint3D upVector() const;

    bool hasSampleAccurateValues() const;
    bool shouldUseARate() const;

    std::span<const float> positionXValues(size_t framesToProcess);
    std::span<const float> positionYValues(size_t framesToProcess);
    std::span<const float> positionZValues(size_t framesToProcess);

    std::span<const float> forwardXValues(size_t framesToProcess);
    std::span<const float> forwardYValues(size_t framesToProcess);
    std::span<const float> forwardZValues(size_t framesToProcess);

    std::span<const float> upXValues(size_t framesToProcess);
    std::span<const float> upYValues(size_t framesToProcess);
    std::span<const float> upZValues(size_t framesToProcess);

    void updateValuesIfNeeded(size_t framesToProcess);

    void updateDirtyState();
    bool isPositionDirty() const { return m_isPositionDirty; }
    bool isOrientationDirty() const { return m_isOrientationDirty; }
    bool isUpVectorDirty() const { return m_isUpVectorDirty; }

protected:
    explicit AudioListener(BaseAudioContext&);

private:

    const Ref<AudioParam> m_positionX;
    const Ref<AudioParam> m_positionY;
    const Ref<AudioParam> m_positionZ;
    const Ref<AudioParam> m_forwardX;
    const Ref<AudioParam> m_forwardY;
    const Ref<AudioParam> m_forwardZ;
    const Ref<AudioParam> m_upX;
    const Ref<AudioParam> m_upY;
    const Ref<AudioParam> m_upZ;

    // Last time that the automations were updated.
    double m_lastUpdateTime { -1 };

    AudioFloatArray m_positionXValues;
    AudioFloatArray m_positionYValues;
    AudioFloatArray m_positionZValues;

    AudioFloatArray m_forwardXValues;
    AudioFloatArray m_forwardYValues;
    AudioFloatArray m_forwardZValues;

    AudioFloatArray m_upXValues;
    AudioFloatArray m_upYValues;
    AudioFloatArray m_upZValues;

    FloatPoint3D m_lastPosition;
    FloatPoint3D m_lastOrientation;
    FloatPoint3D m_lastUpVector;
    bool m_isPositionDirty { false };
    bool m_isOrientationDirty { false };
    bool m_isUpVectorDirty { false };
};

} // namespace WebCore
