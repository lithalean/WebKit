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
#include "AXTextRun.h"
#include "Logging.h"

#if ENABLE(AX_THREAD_TEXT_APIS)

#define TEXT_RUN_ASSERT_AND_LOG(assertion, methodName) do { \
    if (!(assertion)) { \
        RELEASE_LOG(Accessibility, "[AX Thread Text Run] hit assertion in %" PUBLIC_LOG_STRING, methodName); \
        ASSERT(assertion); \
    } \
} while (0)
#define TEXT_RUN_ASSERT_NOT_REACHED_AND_LOG(methodName) do { \
    TEXT_RUN_ASSERT_AND_LOG(false, methodName); \
} while (0)

#include <wtf/text/MakeString.h>

namespace WebCore {

String AXTextRuns::debugDescription() const
{
    return makeString('[', interleave(runs, [&](auto& run) { return run.debugDescription(containingBlock); }, ", "_s), ']');
}

size_t AXTextRuns::indexForOffset(unsigned textOffset, Affinity affinity) const
{
    size_t cumulativeLength = 0;
    for (size_t i = 0; i < runs.size(); i++) {
        cumulativeLength += runLength(i);
        if (cumulativeLength > textOffset) {
            // The offset points into the middle of a run, which is never amibiguous.
            return i;
        }
        if (cumulativeLength == textOffset) {
            // The offset points to the end of a run, which could make this an ambiguous position
            // when considering soft linebreaks.
            if (affinity == Affinity::Downstream && i < lastRunIndex())
                return i + 1;
            return i;
        }
    }
    return notFound;
}

unsigned AXTextRuns::runLengthSumTo(size_t index) const
{
    unsigned length = 0;
    for (size_t i = 0; i <= index && i < runs.size(); i++)
        length += runLength(i);
    return length;
}

String AXTextRuns::substring(unsigned start, unsigned length) const
{
    if (!length)
        return emptyString();

    StringBuilder result;
    size_t charactersSeen = 0;
    auto remaining = [&] () {
        return result.length() >= length ? 0 : length - result.length();
    };
    for (unsigned i = 0; i < runs.size() && result.length() < length; i++) {
        size_t runLength = this->runLength(i);
        if (charactersSeen >= start) {
            // The start points entirely within bounds of this run.
            result.append(runs[i].text.left(remaining()));
        } else if (charactersSeen + runLength > start) {
            // start points somewhere in the middle of the current run, collect part of the text.
            unsigned startInRun = start - charactersSeen;
            TEXT_RUN_ASSERT_AND_LOG(startInRun < runLength, "substring");
            if (startInRun >= runLength)
                startInRun = runLength - 1;
            result.append(runs[i].text.substring(startInRun, remaining()));
        }
        // If charactersSeen + runLength == start, the start points to the end of the run, and there is no text to gather.

        charactersSeen += runLength;
    }
    return result.toString();
}

unsigned AXTextRuns::domOffset(unsigned renderedTextOffset) const
{
    unsigned cumulativeDomOffset = 0;
    unsigned previousEndDomOffset = 0;
    for (size_t i = 0; i < size(); i++) {
        const auto& domOffsets = at(i).domOffsets();
        for (const auto& domOffsetPair : domOffsets) {
            TEXT_RUN_ASSERT_AND_LOG(domOffsetPair[0] >= previousEndDomOffset, "domOffset");
            if (domOffsetPair[0] < previousEndDomOffset)
                return renderedTextOffset;
            // domOffsetPair[0] represents the start DOM offset of this run. Subtracting it
            // from the previous run's end DOM offset, we know how much whitespace was collapsed,
            // and thus know the offset between the DOM text and what was actually rendered.
            // For example, given domOffsets: [2, 10], [13, 18]
            // The first offset to rendered text is 2 (2 - 0), e.g. because of two leading
            // whitespaces that were trimmed: "  foo"
            // The second offset to rendered text is 3 (13 - 10), e.g. because of three
            // collapsed whitespaces in between the first and second runs.
            cumulativeDomOffset += domOffsetPair[0] - previousEndDomOffset;

            // Using the example above, these values would be 0 and 8 for the first run,
            // and 8 and 13 for the second run. Text that would fits this example would be:
            // "  Charlie    Delta", rendered as: "Charlie Delta".
            unsigned startRenderedTextOffset = domOffsetPair[0] - cumulativeDomOffset;
            unsigned endRenderedTextOffset = domOffsetPair[1] - cumulativeDomOffset;
            if (renderedTextOffset >= startRenderedTextOffset && renderedTextOffset <= endRenderedTextOffset) {
                // The rendered text offset is in range of this run. We can get the DOM offset
                // by adding the accumulated difference between the rendered text and DOM text.
                return renderedTextOffset + cumulativeDomOffset;
            }
            previousEndDomOffset = domOffsetPair[1];
        }
    }
    // We were provided with a rendered-text offset that didn't actually fit into our
    // runs. This should never happen.
    TEXT_RUN_ASSERT_NOT_REACHED_AND_LOG("renderedTextOffset");
    return renderedTextOffset;
}

FloatRect AXTextRuns::localRect(unsigned start, unsigned end, FontOrientation orientation) const
{
    unsigned smallerOffset = start;
    unsigned largerOffset = end;
    if (smallerOffset > largerOffset)
        std::swap(smallerOffset, largerOffset);

    // Hardcode Affinity::Downstream to avoid unnecessarily accounting for the end of the line above.
    unsigned runIndexOfSmallerOffset = indexForOffset(smallerOffset, Affinity::Downstream);
    unsigned runIndexOfLargerOffset = indexForOffset(largerOffset, Affinity::Downstream);

    auto computeAdvance = [&] (const AXTextRun& run, unsigned offsetOfFirstCharacterInRun, unsigned startIndex, unsigned endIndex) {
        const auto& characterAdvances = run.advances();
        float totalAdvance = 0;
        unsigned startIndexInRun = startIndex - offsetOfFirstCharacterInRun;
        unsigned endIndexInRun = endIndex - offsetOfFirstCharacterInRun;
        RELEASE_ASSERT(startIndexInRun <= endIndexInRun);
        for (size_t i = startIndexInRun; i < endIndexInRun; i++)
            totalAdvance += (float)characterAdvances[i];
        return totalAdvance;
    };

    // FIXME: Probably want a special case for hard linebreaks (<br>s). Investigate how the main-thread does this.
    // FIXME: We'll need to flip the result rect based on writing mode.
    unsigned offsetFromOriginInDirection = 0;
    unsigned maxWidthInDirection = 0;
    float measuredHeightInDirection = 0.0f;
    float heightBeforeRuns = 0.0f;
    for (unsigned i = 0; i <= runIndexOfLargerOffset; i++) {
        const auto& run = at(i);
        if (i < runIndexOfSmallerOffset) {
            // Each text run represents a line, so count up the height of lines prior to our range start.
            heightBeforeRuns += run.lineHeight;
        } else {
            unsigned measuredWidthInDirection = 0;
            if (i == runIndexOfSmallerOffset) {
                unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
                TEXT_RUN_ASSERT_AND_LOG(smallerOffset >= offsetOfFirstCharacterInRun, "localRect (1)");
                if (smallerOffset < offsetOfFirstCharacterInRun)
                    smallerOffset = offsetOfFirstCharacterInRun;
                // Measure the characters in this run (accomplished by smallerOffset - offsetOfFirstCharacterInRun)
                // prior to the offset.
                unsigned widthPriorToStart = 0;
                if (smallerOffset - offsetOfFirstCharacterInRun > 0)
                    widthPriorToStart = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, smallerOffset);

                // If the larger offset goes beyond this line, use the end of the current line to for computing this run's bounds.
                unsigned endOffsetInLine = runIndexOfSmallerOffset == runIndexOfLargerOffset
                    ? largerOffset
                    : !i ? run.text.length() : runLengthSumTo(i - 1) + run.text.length();

                if (endOffsetInLine - smallerOffset > 0)
                    measuredWidthInDirection = computeAdvance(run, offsetOfFirstCharacterInRun, smallerOffset, endOffsetInLine);

                if (!measuredWidthInDirection) {
                    bool isCollapsedRange = (runIndexOfSmallerOffset == runIndexOfLargerOffset && smallerOffset == largerOffset);

                    if (isCollapsedRange) {
                        // If this is a collapsed range (start.offset == end.offset), we want to return the width of a cursor.
                        // Use 2px for this, matching CaretRectComputation::caretWidth. This overall behavior for collapsed
                        // ranges matches that of CaretRectComputation::computeLocalCaretRect, which is downstream of
                        // the main-thread-text-implementation equivalent of this function, AXObjectCache::boundsForRange.
                        measuredWidthInDirection = 2;
                    } else {
                        // There was no measured width in this run, so we should count this as a line before the actual rect starts.
                        heightBeforeRuns += run.lineHeight;
                    }
                }

                if (measuredWidthInDirection)
                    offsetFromOriginInDirection = widthPriorToStart + run.distanceFromBoundsInDirection;
            } else if (i == runIndexOfLargerOffset) {
                // We're measuring the end of the range, so measure from the first character in the run up to largerOffset.
                unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
                TEXT_RUN_ASSERT_AND_LOG(largerOffset >= offsetOfFirstCharacterInRun, "localRect (3)");
                if (largerOffset < offsetOfFirstCharacterInRun)
                    largerOffset = offsetOfFirstCharacterInRun;

                measuredWidthInDirection = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, largerOffset);
                if (measuredWidthInDirection) {
                    // If we have an offset from origin at this point, that means this range has wrapped from the previous line. We need
                    // to adjust the width to now encompass the whole line, since the origin will be shifted left to 0.
                    if (offsetFromOriginInDirection)
                        measuredWidthInDirection = offsetFromOriginInDirection + maxWidthInDirection;
                    // Because our rect now includes the beginning of a run, set |x| to be 0, indicating the rect is not
                    // offset from its container.
                    offsetFromOriginInDirection = 0;
                }
            } else {
                // We're in some run between runIndexOfSmallerOffset and runIndexOfLargerOffset, so measure the whole run.
                // For example, this could be the "bbb" runs:
                // a|aa
                // bbb
                // cc|c
                unsigned offsetOfFirstCharacterInRun = !i ? 0 : runLengthSumTo(i - 1);
                measuredWidthInDirection = computeAdvance(run, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun, offsetOfFirstCharacterInRun + run.text.length());
                if (measuredWidthInDirection) {
                    // Since we are measuring from the beginning of a run, x should be 0.
                    offsetFromOriginInDirection = 0;
                }
            }

            if (measuredWidthInDirection) {
                // This run is within the range specified by |start| and |end|, so if we measured a width for it,
                // also add to the height. It's important to only do this if we actually measured a width, as an
                // offset pointing past the last character in a run will not add any width and thus should not
                // contribute any height.
                measuredHeightInDirection += run.lineHeight;
            }
            maxWidthInDirection = std::max(maxWidthInDirection, measuredWidthInDirection);
        }
    }

    // Compared to the main-thread implementation, we regularly produce rects that are 1-3px smaller due to the various
    // levels of float rounding that happen to get here. It's better to be a bit wider to ensure AT cursors capture the
    // entire range of text than it is to be too small. Concretely, too-wide is better than too-small for low-vision
    // VoiceOver users who magnify the VoiceOver cursor's contents. Subjectively, the main-thread implementation feels
    // a bit too large, even favoring too-wide sizes, so only bump by 1px. This is especially impactful when navigating
    // character-by-character in small text.
    static constexpr unsigned sizeBump = 1;

    if (orientation == FontOrientation::Horizontal)
        return { static_cast<float>(offsetFromOriginInDirection), heightBeforeRuns, static_cast<float>(maxWidthInDirection) + sizeBump, measuredHeightInDirection };

    return { heightBeforeRuns, static_cast<float>(offsetFromOriginInDirection), measuredHeightInDirection + sizeBump, static_cast<float>(maxWidthInDirection) };
}

} // namespace WebCore
#endif // ENABLE(AX_THREAD_TEXT_APIS)
