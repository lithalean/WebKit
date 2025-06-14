/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "VelocityData.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HistoricalVelocityData);

VelocityData HistoricalVelocityData::velocityForNewData(FloatPoint newPosition, double scale, MonotonicTime timestamp)
{
    VelocityData velocityData;
    if (m_positionHistory.size() > 0) {
        Seconds timeDelta = timestamp - m_positionHistory.first().timestamp;
        Data& oldestData = m_positionHistory.first();
        velocityData = VelocityData((newPosition.x() - oldestData.position.x()) / timeDelta.seconds(), (newPosition.y() - oldestData.position.y()) / timeDelta.seconds(), (scale - oldestData.scale) / timeDelta.seconds(), timestamp);
    }

    if (m_positionHistory.size() == maxHistoryDepth)
        m_positionHistory.removeFirst();
    m_positionHistory.append({ timestamp, newPosition, scale });

    return velocityData;
}

TextStream& operator<<(TextStream& ts, const VelocityData& velocityData)
{
    ts.dumpProperty("timestamp"_s, velocityData.lastUpdateTime.secondsSinceEpoch().value());
    if (velocityData.horizontalVelocity)
        ts.dumpProperty("horizontalVelocity"_s, velocityData.horizontalVelocity);
    if (velocityData.verticalVelocity)
        ts.dumpProperty("verticalVelocity"_s, velocityData.verticalVelocity);
    if (velocityData.scaleChangeRate)
        ts.dumpProperty("scaleChangeRate"_s, velocityData.scaleChangeRate);

    return ts;
}

} // namespace WebCore
