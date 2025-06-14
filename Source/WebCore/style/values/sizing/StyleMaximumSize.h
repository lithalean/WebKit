/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "Length.h"
#include "LengthFunctions.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleValueTypes.h"

namespace WebCore {

class CSSValue;
class RenderStyle;

namespace Style {

class BuilderState;

// <'max-width'>/<'max-height'> = none | <length-percentage [0,∞]> | min-content | max-content | fit-content(<length-percentage [0,∞]>) | <calc-size()> | stretch | fit-content | contain
//
// What is actually implemented is:
//
// <'width'>/<'height'> = none | <length-percentage [0,∞]> | min-content | max-content | fit-content | intrinsic | min-intrinsic | -webkit-fill-available
//
// MISSING:
//    fit-content(<length-percentage [0,∞]>)
//    <calc-size()>
//    stretch
//    contain

// NON-STANDARD:
//    intrinsic
//    min-intrinsic
//    -webkit-fill-available
//
// https://drafts.csswg.org/css-sizing-3/#max-size-properties
// https://drafts.csswg.org/css-sizing-4/#sizing-values (additional values added)
struct MaximumSize {
    using Specified = LengthPercentage<CSS::Nonnegative>;
    using Fixed = typename Specified::Dimension;
    using Percentage = typename Specified::Percentage;
    using Calc = typename Specified::Calc;

    MaximumSize(CSS::Keyword::None) : m_value(WebCore::LengthType::Undefined) { }
    MaximumSize(CSS::Keyword::MinContent) : m_value(WebCore::LengthType::MinContent) { }
    MaximumSize(CSS::Keyword::MaxContent) : m_value(WebCore::LengthType::MaxContent) { }
    MaximumSize(CSS::Keyword::FitContent) : m_value(WebCore::LengthType::FitContent) { }
    MaximumSize(CSS::Keyword::WebkitFillAvailable) : m_value(WebCore::LengthType::FillAvailable) { }
    MaximumSize(CSS::Keyword::Intrinsic) : m_value(WebCore::LengthType::Intrinsic) { }
    MaximumSize(CSS::Keyword::MinIntrinsic) : m_value(WebCore::LengthType::MinIntrinsic) { }

    MaximumSize(Fixed&& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    MaximumSize(const Fixed& fixed) : m_value(fixed.value, WebCore::LengthType::Fixed) { }
    MaximumSize(Percentage&& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }
    MaximumSize(const Percentage& percent) : m_value(percent.value, WebCore::LengthType::Percent) { }

    MaximumSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Fixed) { }
    MaximumSize(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : m_value(static_cast<float>(literal.value), WebCore::LengthType::Percent) { }

    explicit MaximumSize(WebCore::Length&& other) : m_value(WTFMove(other)) { RELEASE_ASSERT(isValid(m_value)); }
    explicit MaximumSize(const WebCore::Length& other) : m_value(other) { RELEASE_ASSERT(isValid(m_value)); }

    ALWAYS_INLINE bool isFixed() const { return m_value.isFixed(); }
    ALWAYS_INLINE bool isPercent() const { return m_value.isPercent(); }
    ALWAYS_INLINE bool isCalculated() const { return m_value.isCalculated(); }
    ALWAYS_INLINE bool isPercentOrCalculated() const { return m_value.isPercentOrCalculated(); }
    ALWAYS_INLINE bool isSpecified() const { return m_value.isSpecified(); }

    ALWAYS_INLINE bool isNone() const { return m_value.isUndefined(); }
    ALWAYS_INLINE bool isMinContent() const { return m_value.isMinContent(); }
    ALWAYS_INLINE bool isMaxContent() const { return m_value.isMaxContent(); }
    ALWAYS_INLINE bool isFitContent() const { return m_value.isFitContent(); }
    ALWAYS_INLINE bool isFillAvailable() const { return m_value.isFillAvailable(); }
    ALWAYS_INLINE bool isMinIntrinsic() const { return m_value.isMinIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicKeyword() const { return m_value.type() == LengthType::Intrinsic; }

    // FIXME: This is misleadingly named. One would expect this function checks `type == LengthType::Intrinsic` but instead it checks `type = LengthType::MinContent || type == LengthType::MaxContent || type == LengthType::FillAvailable || type == LengthType::FitContent`.
    ALWAYS_INLINE bool isIntrinsic() const { return m_value.isIntrinsic(); }
    ALWAYS_INLINE bool isLegacyIntrinsic() const { return m_value.isLegacyIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsic() const { return isIntrinsic() || isLegacyIntrinsic(); }
    ALWAYS_INLINE bool isIntrinsicOrLegacyIntrinsicOrAuto() const { return isIntrinsic() || isLegacyIntrinsic(); } // NOTE: it is never `auto` for MaximumSize, but this function is implemented for use in generic contexts.
    ALWAYS_INLINE bool isSpecifiedOrIntrinsic() const { return m_value.isSpecifiedOrIntrinsic(); }

    ALWAYS_INLINE bool isZero() const { return m_value.isZero(); }
    ALWAYS_INLINE bool isPositive() const { return m_value.isPositive(); }
    ALWAYS_INLINE bool isNegative() const { return m_value.isNegative(); }

    // FIXME: Remove this when RenderBox's adjust*Box functions no longer need it.
    ALWAYS_INLINE WebCore::LengthType type() const { return m_value.type(); }

    std::optional<Fixed> tryFixed() const { return isFixed() ? std::make_optional(Fixed { m_value.value() }) : std::nullopt; }
    std::optional<Percentage> tryPercentage() const { return isPercent() ? std::make_optional(Percentage { m_value.value() }) : std::nullopt; }
    std::optional<Calc> tryCalc() const { return isCalculated() ? std::make_optional(Calc { m_value.calculationValue() }) : std::nullopt; }

    template<typename T> bool holdsAlternative() const
    {
             if constexpr (std::same_as<T, Fixed>)                              return isFixed();
        else if constexpr (std::same_as<T, Percentage>)                         return isPercent();
        else if constexpr (std::same_as<T, Calc>)                               return isCalculated();
        else if constexpr (std::same_as<T, CSS::Keyword::None>)                 return isNone();
        else if constexpr (std::same_as<T, CSS::Keyword::Intrinsic>)            return isIntrinsicKeyword();
        else if constexpr (std::same_as<T, CSS::Keyword::MinIntrinsic>)         return isMinIntrinsic();
        else if constexpr (std::same_as<T, CSS::Keyword::MinContent>)           return isMinContent();
        else if constexpr (std::same_as<T, CSS::Keyword::MaxContent>)           return isMaxContent();
        else if constexpr (std::same_as<T, CSS::Keyword::WebkitFillAvailable>)  return isFillAvailable();
        else if constexpr (std::same_as<T, CSS::Keyword::FitContent>)           return isFitContent();
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_value.type()) {
        case WebCore::LengthType::Fixed:            return visitor(Fixed { m_value.value() });
        case WebCore::LengthType::Percent:          return visitor(Percentage { m_value.value() });
        case WebCore::LengthType::Calculated:       return visitor(Calc { m_value.calculationValue() });
        case WebCore::LengthType::Undefined:        return visitor(CSS::Keyword::None { });
        case WebCore::LengthType::Intrinsic:        return visitor(CSS::Keyword::Intrinsic { });
        case WebCore::LengthType::MinIntrinsic:     return visitor(CSS::Keyword::MinIntrinsic { });
        case WebCore::LengthType::MinContent:       return visitor(CSS::Keyword::MinContent { });
        case WebCore::LengthType::MaxContent:       return visitor(CSS::Keyword::MaxContent { });
        case WebCore::LengthType::FillAvailable:    return visitor(CSS::Keyword::WebkitFillAvailable { });
        case WebCore::LengthType::FitContent:       return visitor(CSS::Keyword::FitContent { });

        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Content:
        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
            break;
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    bool hasSameType(const MaximumSize& other) const { return m_value.type() == other.m_value.type(); }

    bool operator==(const MaximumSize&) const = default;

private:
    friend struct Blending<MaximumSize>;
    friend struct Evaluation<MaximumSize>;
    friend LayoutUnit evaluateMinimum(const MaximumSize&, NOESCAPE const Invocable<LayoutUnit()> auto&);
    friend LayoutUnit evaluateMinimum(const MaximumSize&, LayoutUnit);
    friend WTF::TextStream& operator<<(WTF::TextStream&, const MaximumSize&);

    static bool isValid(const WebCore::Length& length)
    {
        switch (length.type()) {
        case WebCore::LengthType::Fixed:
            return CSS::isWithinRange<Fixed::range>(length.value());
        case WebCore::LengthType::Percent:
            return CSS::isWithinRange<Percentage::range>(length.value());
        case WebCore::LengthType::Undefined:
        case WebCore::LengthType::Intrinsic:
        case WebCore::LengthType::MinIntrinsic:
        case WebCore::LengthType::MinContent:
        case WebCore::LengthType::MaxContent:
        case WebCore::LengthType::FillAvailable:
        case WebCore::LengthType::FitContent:
        case WebCore::LengthType::Calculated:
            return true;
        case WebCore::LengthType::Auto:
        case WebCore::LengthType::Content:
        case WebCore::LengthType::Normal:
        case WebCore::LengthType::Relative:
            break;
        }
        return false;
    }

    WebCore::Length m_value;
};

using MaximumSizePair = SpaceSeparatedSize<MaximumSize>;

// MARK: - Conversion

template<> struct CSSValueConversion<MaximumSize> { auto operator()(BuilderState&, const CSSValue&) -> MaximumSize; };

// MARK: - Evaluation

template<> struct Evaluation<MaximumSize> {
    auto operator()(const MaximumSize& edge, LayoutUnit referenceLength) -> LayoutUnit
    {
        return valueForLength(edge.m_value, referenceLength);
    }

    auto operator()(const MaximumSize& edge, float referenceLength) -> float
    {
        return floatValueForLength(edge.m_value, referenceLength);
    }
};

inline LayoutUnit evaluateMinimum(const MaximumSize& edge, NOESCAPE const Invocable<LayoutUnit()> auto& lazyMaximumValueFunctor)
{
    return minimumValueForLengthWithLazyMaximum<LayoutUnit, LayoutUnit>(edge.m_value, lazyMaximumValueFunctor);
}

inline LayoutUnit evaluateMinimum(const MaximumSize& edge, LayoutUnit maximumValue)
{
    return minimumValueForLength(edge.m_value, maximumValue);
}

// MARK: - Blending

template<> struct Blending<MaximumSize> {
    auto canBlend(const MaximumSize&, const MaximumSize&) -> bool;
    auto requiresInterpolationForAccumulativeIteration(const MaximumSize&, const MaximumSize&) -> bool;
    auto blend(const MaximumSize&, const MaximumSize&, const BlendingContext&) -> MaximumSize;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const MaximumSize&);

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::MaximumSize> = true;
