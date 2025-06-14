/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "BorderValue.h"
#include "LengthSize.h"
#include "NinePieceImage.h"
#include "RectCorners.h"
#include "RectEdges.h"
#include "StyleCornerShapeValue.h"

namespace WebCore {

class OutlineValue;

class BorderData {
    friend class RenderStyle;
public:
    using Radii = RectCorners<LengthSize>;

    bool hasBorder() const
    {
        return m_edges.anyOf([](const auto& edge) { return edge.nonZero(); });
    }

    bool hasVisibleBorder() const
    {
        return m_edges.anyOf([](const auto& edge) { return edge.isVisible(); });
    }

    bool hasBorderImage() const
    {
        return m_image.hasImage();
    }

    bool hasBorderRadius() const
    {
        return m_radii.anyOf([](auto& corner) { return !corner.isEmpty(); });
    }

    template<BoxSide side>
    float borderEdgeWidth() const
    {
        if (m_edges[side].style() == BorderStyle::None || m_edges[side].style() == BorderStyle::Hidden)
            return 0;
        if (m_image.overridesBorderWidths() && m_image.borderSlices()[side].isFixed())
            return m_image.borderSlices()[side].value();
        return m_edges[side].width();
    }

    float borderLeftWidth() const { return borderEdgeWidth<BoxSide::Left>(); }
    float borderRightWidth() const { return borderEdgeWidth<BoxSide::Right>(); }
    float borderTopWidth() const { return borderEdgeWidth<BoxSide::Top>(); }
    float borderBottomWidth() const { return borderEdgeWidth<BoxSide::Bottom>(); }

    FloatBoxExtent borderWidth() const
    {
        return FloatBoxExtent(borderTopWidth(), borderRightWidth(), borderBottomWidth(), borderLeftWidth());
    }

    bool isEquivalentForPainting(const BorderData& other, bool currentColorDiffers) const;

    friend bool operator==(const BorderData&, const BorderData&) = default;

    const BorderValue& left() const { return m_edges.left(); }
    const BorderValue& right() const { return m_edges.right(); }
    const BorderValue& top() const { return m_edges.top(); }
    const BorderValue& bottom() const { return m_edges.bottom(); }

    const NinePieceImage& image() const { return m_image; }

    const LengthSize& topLeftRadius() const { return m_radii.topLeft(); }
    const LengthSize& topRightRadius() const { return m_radii.topRight(); }
    const LengthSize& bottomLeftRadius() const { return m_radii.bottomLeft(); }
    const LengthSize& bottomRightRadius() const { return m_radii.bottomRight(); }

    const Style::CornerShapeValue& topLeftCornerShape() const { return m_cornerShapes.topLeft(); }
    const Style::CornerShapeValue& topRightCornerShape() const { return m_cornerShapes.topRight(); }
    const Style::CornerShapeValue& bottomLeftCornerShape() const { return m_cornerShapes.bottomLeft(); }
    const Style::CornerShapeValue& bottomRightCornerShape() const { return m_cornerShapes.bottomRight(); }

    void dump(TextStream&, DumpStyleValues = DumpStyleValues::All) const;

private:
    bool containsCurrentColor() const;

    RectEdges<BorderValue> m_edges;
    NinePieceImage m_image;
    RectCorners<LengthSize> m_radii { LengthSize { LengthType::Fixed, LengthType::Fixed } };
    RectCorners<Style::CornerShapeValue> m_cornerShapes { Style::CornerShapeValue::round() };
};

WTF::TextStream& operator<<(WTF::TextStream&, const BorderValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const OutlineValue&);
WTF::TextStream& operator<<(WTF::TextStream&, const BorderData&);

} // namespace WebCore
