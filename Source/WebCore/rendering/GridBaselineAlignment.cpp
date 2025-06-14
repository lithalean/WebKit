/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GridBaselineAlignment.h"

#include "AncestorSubgridIterator.h"
#include "BaselineAlignmentInlines.h"
#include "RenderBoxInlines.h"
#include "RenderGrid.h"
#include "RenderStyleConstants.h"

namespace WebCore {

LayoutUnit GridBaselineAlignment::logicalAscentForGridItem(const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType, ItemPosition position) const
{
    auto hasOrthogonalAncestorSubgrids = [&] {
        for (auto& currentAncestorSubgrid : ancestorSubgridsOfGridItem(gridItem, GridTrackSizingDirection::ForRows)) {
            if (currentAncestorSubgrid.isHorizontalWritingMode() != currentAncestorSubgrid.parent()->isHorizontalWritingMode())
                return true;
        }
        return false;
    };

    ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids;
    if (alignmentContextType == GridTrackSizingDirection::ForRows && !hasOrthogonalAncestorSubgrids())
        extraMarginsFromAncestorSubgrids = GridLayoutFunctions::extraMarginForSubgridAncestors(GridTrackSizingDirection::ForRows, gridItem);

    LayoutUnit ascent = ascentForGridItem(gridItem, alignmentContextType, position) + extraMarginsFromAncestorSubgrids.extraTrackStartMargin();
    return (isDescentBaselineForGridItem(gridItem, alignmentContextType) || position == ItemPosition::LastBaseline) ? descentForGridItem(gridItem, ascent, alignmentContextType, extraMarginsFromAncestorSubgrids) : ascent;
}

LayoutUnit GridBaselineAlignment::ascentForGridItem(const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType, ItemPosition position) const
{
    static const LayoutUnit noValidBaseline = LayoutUnit(-1);

    ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
    auto baseline = 0_lu;
    auto gridItemMargin = alignmentContextType == GridTrackSizingDirection::ForRows
        ? gridItem.marginBefore(m_writingMode)
        : gridItem.marginStart(m_writingMode);
    auto& parentStyle = gridItem.parent()->style();

    if (alignmentContextType == GridTrackSizingDirection::ForRows) {
        auto alignmentContextDirection = [&] {
            return parentStyle.writingMode().isHorizontal() ? LineDirectionMode::HorizontalLine : LineDirectionMode::VerticalLine;
        };

        if (!isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType))
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        auto ascent = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        if (!ascent)
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, alignmentContextDirection(), BaselineSynthesisEdge::BorderBox);
        baseline = ascent.value();
    } else {
        auto computedBaselineValue = position == ItemPosition::Baseline ? gridItem.firstLineBaseline() : gridItem.lastLineBaseline();
        baseline = isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType) ? computedBaselineValue.value_or(noValidBaseline) : noValidBaseline;
        // We take border-box's under edge if no valid baseline.
        if (baseline == noValidBaseline) {
            ASSERT(!gridItem.needsLayout());
            if (isVerticalAlignmentContext(alignmentContextType))
                return m_writingMode.isBlockFlipped() ? gridItemMargin + gridItem.size().width().toInt() : gridItemMargin;
            return gridItemMargin + synthesizedBaseline(gridItem, parentStyle, LineDirectionMode::HorizontalLine, BaselineSynthesisEdge::BorderBox);
        }
    }

    return gridItemMargin + baseline;
}

LayoutUnit GridBaselineAlignment::descentForGridItem(const RenderBox& gridItem, LayoutUnit ascent, GridTrackSizingDirection alignmentContextType, ExtraMarginsFromSubgrids extraMarginsFromAncestorSubgrids) const
{
    ASSERT(!gridItem.needsLayout());
    if (isParallelToAlignmentAxisForGridItem(gridItem, alignmentContextType))
        return extraMarginsFromAncestorSubgrids.extraTotalMargin() + gridItem.marginLogicalHeight() + gridItem.logicalHeight() - ascent;
    return gridItem.marginLogicalWidth() + gridItem.logicalWidth() - ascent;
}

bool GridBaselineAlignment::isDescentBaselineForGridItem(const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType) const
{
    return isVerticalAlignmentContext(alignmentContextType)
        && ((gridItem.writingMode().isBlockFlipped() && !m_writingMode.isBlockFlipped())
            || (gridItem.writingMode().isLineInverted() && m_writingMode.isBlockFlipped()));
}

bool GridBaselineAlignment::isVerticalAlignmentContext(GridTrackSizingDirection alignmentContextType) const
{
    return (alignmentContextType == GridTrackSizingDirection::ForColumns) == m_writingMode.isHorizontal();
}

bool GridBaselineAlignment::isOrthogonalGridItemForBaseline(const RenderBox& gridItem) const
{
    return m_writingMode.isOrthogonal(gridItem.writingMode());
}

bool GridBaselineAlignment::isParallelToAlignmentAxisForGridItem(const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType) const
{
    return alignmentContextType == GridTrackSizingDirection::ForRows ? !isOrthogonalGridItemForBaseline(gridItem) : isOrthogonalGridItemForBaseline(gridItem);
}

const BaselineGroup& GridBaselineAlignment::baselineGroupForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType) const
{
    ASSERT(isBaselinePosition(preference));
    auto& baselineAlignmentStateMap = alignmentContextType == GridTrackSizingDirection::ForRows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    auto* baselineAlignmentState = baselineAlignmentStateMap.get(sharedContext);
    ASSERT(baselineAlignmentState);
    return baselineAlignmentState->sharedGroup(gridItem, preference);
}

void GridBaselineAlignment::updateBaselineAlignmentContext(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType)
{
    ASSERT(isBaselinePosition(preference));
    ASSERT(!gridItem.needsLayout());

    // Determine Ascent and Descent values of this grid item with respect to
    // its grid container.
    LayoutUnit ascent = logicalAscentForGridItem(gridItem, alignmentContextType, preference);
    // Looking up for a shared alignment context perpendicular to the
    // alignment axis.
    auto& baselineAlignmentStateMap = alignmentContextType == GridTrackSizingDirection::ForRows ? m_rowAlignmentContextStates : m_columnAlignmentContextStates;
    // Looking for a compatible baseline-sharing group.
    if (auto* baselineAlignmentStateSearch = baselineAlignmentStateMap.get(sharedContext))
        baselineAlignmentStateSearch->updateSharedGroup(gridItem, preference, ascent);
    else
        baselineAlignmentStateMap.add(sharedContext, makeUnique<BaselineAlignmentState>(gridItem, preference, ascent));
}

LayoutUnit GridBaselineAlignment::baselineOffsetForGridItem(ItemPosition preference, unsigned sharedContext, const RenderBox& gridItem, GridTrackSizingDirection alignmentContextType) const
{
    ASSERT(isBaselinePosition(preference));
    auto& group = baselineGroupForGridItem(preference, sharedContext, gridItem, alignmentContextType);
    if (group.computeSize() > 1)
        return group.maxAscent() - logicalAscentForGridItem(gridItem, alignmentContextType, preference);
    return LayoutUnit();
}

void GridBaselineAlignment::clear(GridTrackSizingDirection alignmentContextType)
{
    if (alignmentContextType == GridTrackSizingDirection::ForRows)
        m_rowAlignmentContextStates.clear();
    else
        m_columnAlignmentContextStates.clear();
}

} // namespace WebCore
