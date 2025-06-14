/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StylePosition.h"

#include "CalculationCategory.h"
#include "CalculationTree.h"
#include "LengthPoint.h"
#include "RenderStyle.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

// MARK: Core Keyword Resolution

static auto resolveKeyword(CSS::Keyword::Top, const BuilderState&) -> LengthPercentage<>
{
    return 0_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Top, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return toStyle(length, state);
}

static auto resolveKeyword(CSS::Keyword::Right, const BuilderState&) -> LengthPercentage<>
{
    return 100_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Right, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return reflect(toStyle(length, state));
}

static auto resolveKeyword(CSS::Keyword::Bottom, const BuilderState&) -> LengthPercentage<>
{
    return 100_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Bottom, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return reflect(toStyle(length, state));
}

static auto resolveKeyword(CSS::Keyword::Left, const BuilderState&) -> LengthPercentage<>
{
    return 0_css_percentage;
}

static auto resolveKeyword(CSS::Keyword::Left, const BuilderState& state, const CSS::LengthPercentage<>& length) -> LengthPercentage<>
{
    return toStyle(length, state);
}

static auto resolveKeyword(CSS::Keyword::Center, const BuilderState&) -> LengthPercentage<>
{
    return 50_css_percentage;
}

// MARK: Mapped value resolution

template<typename... Args> static auto resolveKeyword(CSS::Keyword::XStart, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyLeftToRight()
        ? resolveKeyword(CSS::Keyword::Left { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Right { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::XEnd, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyLeftToRight()
        ? resolveKeyword(CSS::Keyword::Right { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Left { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::YStart, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyTopToBottom()
        ? resolveKeyword(CSS::Keyword::Top { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Bottom { }, state, std::forward<Args>(args)...);
}

template<typename... Args> static auto resolveKeyword(CSS::Keyword::YEnd, const BuilderState& state, Args&&... args) -> LengthPercentage<>
{
    return state.style().writingMode().isAnyTopToBottom()
        ? resolveKeyword(CSS::Keyword::Bottom { }, state, std::forward<Args>(args)...)
        : resolveKeyword(CSS::Keyword::Top { }, state, std::forward<Args>(args)...);
}

// MARK: Horizontal/Vertical

static auto resolve(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        },
        [&](const CSS::LengthPercentage<>& value) {
            return toStyle(value, state);
        }
    );
}

static auto resolve(const CSS::TwoComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        },
        [&](const CSS::LengthPercentage<>& value) {
            return toStyle(value, state);
        }
    );
}

static auto resolve(const CSS::ThreeComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        }
    );
}

static auto resolve(const CSS::ThreeComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(value.offset,
        [&](auto keyword) {
            return resolveKeyword(keyword, state);
        }
    );
}

static auto resolve(const CSS::FourComponentPositionHorizontal& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(get<0>(value.offset),
        [&](auto keyword) {
            return resolveKeyword(keyword, state, get<1>(value.offset));
        }
    );
}

static auto resolve(const CSS::FourComponentPositionVertical& value, const BuilderState& state) -> LengthPercentage<>
{
    return WTF::switchOn(get<0>(value.offset),
        [&](auto keyword) {
            return resolveKeyword(keyword, state, get<1>(value.offset));
        }
    );
}

auto ToStyle<CSS::TwoComponentPositionHorizontal>::operator()(const CSS::TwoComponentPositionHorizontal& value, const BuilderState& state) -> TwoComponentPositionHorizontal
{
    return { resolve(value, state) };
}

auto ToStyle<CSS::TwoComponentPositionVertical>::operator()(const CSS::TwoComponentPositionVertical& value, const BuilderState& state) -> TwoComponentPositionVertical
{
    return { resolve(value, state) };
}

// MARK: <position> conversion

auto ToCSS<Position>::operator()(const Position& value, const RenderStyle& style) -> CSS::Position
{
    return CSS::TwoComponentPositionHorizontalVertical { { toCSS(value.x(), style) }, { toCSS(value.y(), style) } };
}

auto ToStyle<CSS::Position>::operator()(const CSS::Position& position, const BuilderState& state) -> Position
{
    return WTF::switchOn(position,
        [&](const auto& components) {
            return Position {
                resolve(get<0>(components), state),
                resolve(get<1>(components), state),
            };
        }
    );
}

// MARK: <position-x> conversion

auto ToCSS<PositionX>::operator()(const PositionX& value, const RenderStyle& style) -> CSS::PositionX
{
    return CSS::TwoComponentPositionHorizontal { toCSS(value.value, style) };
}

auto ToStyle<CSS::PositionX>::operator()(const CSS::PositionX& positionX, const BuilderState& state) -> PositionX
{
    return WTF::switchOn(positionX,
        [&](const auto& value) {
            return PositionX { resolve(value, state) };
        }
    );
}

// MARK: <position-y> conversion

auto ToCSS<PositionY>::operator()(const PositionY& value, const RenderStyle& style) -> CSS::PositionY
{
    return CSS::TwoComponentPositionVertical { toCSS(value.value, style) };
}

auto ToStyle<CSS::PositionY>::operator()(const CSS::PositionY& positionY, const BuilderState& state) -> PositionY
{
    return WTF::switchOn(positionY,
        [&](const auto& value) {
            return PositionY { resolve(value, state) };
        }
    );
}

// MARK: - Evaluation

auto Evaluation<Position>::operator()(const Position& position, FloatSize referenceBox) -> FloatPoint
{
    return evaluate(position.value, referenceBox);
}

// MARK: - Platform

static auto toPlatform(const LengthPercentage<>& length) -> WebCore::Length
{
    return WTF::switchOn(length,
        [](const LengthPercentage<>::Dimension& dimension) {
            return WebCore::Length { dimension.value, WebCore::LengthType::Fixed };
        },
        [](const LengthPercentage<>::Percentage& percentage) {
            return WebCore::Length { percentage.value, WebCore::LengthType::Percent };
        },
        [](const LengthPercentage<>::Calc& calc) {
            return WebCore::Length { calc.protectedCalculation() };
        }
    );
}

auto toPlatform(const Position& position) -> WebCore::LengthPoint
{
    return { toPlatform(position.x()), toPlatform(position.y()) };
}

auto toPlatform(const PositionX& positionX) -> WebCore::Length
{
    return toPlatform(positionX.value);
}

auto toPlatform(const PositionY& positionY) -> WebCore::Length
{
    return toPlatform(positionY.value);
}

} // namespace CSS
} // namespace WebCore
