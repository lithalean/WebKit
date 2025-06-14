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

#include "CSSDynamicRangeLimitMix.h"
#include "CSSMarkup.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSValueTypes.h"
#include "ColorSerialization.h"
#include "StyleExtractorConverter.h"
#include "StylePrimitiveNumericTypes+Serialization.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace Style {

template<typename T> requires std::is_enum_v<T> struct Serialize<T> {
    void operator()(StringBuilder& builder, const CSS::SerializationContext&, const RenderStyle&, T value)
    {
        builder.append(nameLiteralForSerialization(toCSSValueID(value)));
    }
};

class ExtractorSerializer {
public:
    // MARK: Strong value conversions

    template<typename T> static void serializeStyleType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const T&);

    // MARK: Primitive serializations

    template<typename ConvertibleType>
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ConvertibleType&);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, int);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, unsigned short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, short);
    static void serialize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&);

    static void serializeLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLength(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLengthAllowingNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLengthOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeLengthWithoutApplyingZoom(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);

    template<typename T> static void serializeNumber(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeNumberAsPixels(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeComputedLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T, CSSValueID> static void serializeNumberOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T);
    template<typename T> static void serializeLineWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, T lineWidth);

    template<CSSValueID> static void serializeStringOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const String&);
    template<CSSValueID> static void serializeCustomIdentOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const String&);
    template<CSSValueID> static void serializeStringAtomOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);
    template<CSSValueID> static void serializeCustomIdentAtomOrKeyword(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);

    // MARK: SVG serializations

    static void serializeSVGURIReference(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const URL&);
    static void serializeSVGPaint(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, SVGPaintType, const URL&, const Color&);

    // MARK: Transform serializations

    static void serializeTransformationMatrix(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformationMatrix(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformationMatrix&);
    static void serializeTransformOperation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TransformOperation&);
    static void serializeTransformOperation(const RenderStyle&, StringBuilder&, const CSS::SerializationContext&, const TransformOperation&);

    // MARK: Shared serializations

    static void serializeOpacity(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serializeImageOrNone(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleImage*);
    static void serializeGlyphOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeGlyphOrientationOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GlyphOrientation);
    static void serializeListStyleType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ListStyleType&);
    static void serializeMarginTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<MarginTrimType>);
    static void serializeBasicShape(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const BasicShape&, PathConversion = PathConversion::None);
    static void serializeShapeValue(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ShapeValue*);
    static void serializePathOperation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const PathOperation*, PathConversion = PathConversion::None);
    static void serializePathOperationForceAbsolute(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const PathOperation*);
    static void serializeDPath(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StylePathData*);
    static void serializeStrokeDashArray(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<WebCore::Length>&);
    static void serializeTextStrokeWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, float);
    static void serializeFilterOperations(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FilterOperations&);
    static void serializeAppleColorFilterOperations(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FilterOperations&);
    static void serializeWebkitTextCombine(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextCombine);
    static void serializeImageOrientation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, ImageOrientation);
    static void serializeLineClamp(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LineClampValue&);
    static void serializeContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Containment>);
    static void serializeMaxLines(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, size_t);
    static void serializeSmoothScrolling(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool);
    static void serializeInitialLetter(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, IntSize);
    static void serializeTextSpacingTrim(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextSpacingTrim);
    static void serializeTextAutospace(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, TextAutospace);
    static void serializeReflection(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleReflection*);
    static void serializeLineFitEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializeTextBoxEdge(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextEdge&);
    static void serializeQuotes(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const QuotesData*);
    static void serializeBorderRadiusCorner(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthSize&);
    static void serializeContainerNames(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<ScopedName>&);
    static void serializeViewTransitionClasses(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<ScopedName>&);
    static void serializeViewTransitionName(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ViewTransitionName&);
    static void serializeBoxShadow(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<BoxShadow>&);
    static void serializeTextShadow(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<TextShadow>&);
    static void serializePositionTryFallbacks(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<PositionTryFallback>&);
    static void serializeWillChange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WillChangeData*);
    static void serializeBlockEllipsis(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const BlockEllipsis&);
    static void serializeBlockStepSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, std::optional<WebCore::Length>);
    static void serializeGapLength(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const GapLength&);
    static void serializeTabSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TabSize&);
    static void serializeScrollSnapType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapType&);
    static void serializeScrollSnapAlign(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollSnapAlign&);
    static void serializeScrollbarColor(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, std::optional<ScrollbarColor>);
    static void serializeScrollbarGutter(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScrollbarGutter&);
    static void serializeLineBoxContain(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<Style::LineBoxContain>);
    static void serializeWebkitRubyPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, RubyPosition);
    static void serializePosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthPoint&);
    static void serializePositionOrAuto(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthPoint&);
    static void serializePositionOrAutoOrNormal(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthPoint&);
    static void serializeContainIntrinsicSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ContainIntrinsicSizeType&, const std::optional<WebCore::Length>&);
    static void serializeTouchAction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TouchAction>);
    static void serializeTextTransform(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextTransform>);
    static void serializeTextDecorationLine(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextDecorationLine>);
    static void serializeTextUnderlineOffset(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextUnderlineOffset&);
    static void serializeTextUnderlinePosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextUnderlinePosition>);
    static void serializeTextDecorationThickness(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextDecorationThickness&);
    static void serializeTextEmphasisPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition>);
    static void serializeSpeakAs(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<SpeakAs>);
    static void serializeHangingPunctuation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<HangingPunctuation>);
    static void serializePageBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakBetween);
    static void serializePageBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakInside);
    static void serializeWebkitColumnBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakBetween);
    static void serializeWebkitColumnBreak(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BreakInside);
    static void serializeSelfOrDefaultAlignmentData(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleSelfAlignmentData&);
    static void serializeContentAlignmentData(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleContentAlignmentData&);
    static void serializeOffsetRotate(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const OffsetRotation&);
    static void serializePaintOrder(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, PaintOrder);
    static void serializeScrollTimelineAxes(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<ScrollAxis>&);
    static void serializeScrollTimelineNames(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<AtomString>&);
    static void serializeAnchorNames(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<ScopedName>&);
    static void serializePositionAnchor(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<ScopedName>&);
    static void serializePositionArea(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const std::optional<PositionArea>&);
    static void serializeNameScope(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NameScope&);
    static void serializeSingleViewTimelineInsets(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ViewTimelineInsets&);
    static void serializeViewTimelineInsets(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FixedVector<ViewTimelineInsets>&);
    static void serializePositionVisibility(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, OptionSet<PositionVisibility>);
#if ENABLE(TEXT_AUTOSIZING)
    static void serializeWebkitTextSizeAdjust(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TextSizeAdjustment&);
#endif
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    static void serializeOverflowScrolling(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool);
#endif
#if PLATFORM(IOS_FAMILY)
    static void serializeTouchCallout(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool);
#endif

    // MARK: FillLayer serializations

    static void serializeFillLayerAttachment(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillAttachment);
    static void serializeFillLayerBlendMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, BlendMode);
    static void serializeFillLayerClip(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillBox);
    static void serializeFillLayerOrigin(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillBox);
    static void serializeFillLayerXPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeFillLayerYPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const WebCore::Length&);
    static void serializeFillLayerRepeat(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillRepeatXY);
    static void serializeFillLayerBackgroundSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillSize);
    static void serializeFillLayerMaskSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FillSize);
    static void serializeFillLayerMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerWebkitMaskComposite(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperator);
    static void serializeFillLayerMaskMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeFillLayerWebkitMaskSourceType(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MaskMode);
    static void serializeFillLayerImage(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const StyleImage*);

    // MARK: Font serializations

    static void serializeFontFamily(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const AtomString&);
    static void serializeFontSizeAdjust(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontSizeAdjust&);
    static void serializeFontPalette(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontPalette&);
    static void serializeFontWeight(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontWidth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, FontSelectionValue);
    static void serializeFontFeatureSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontFeatureSettings&);
    static void serializeFontVariationSettings(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const FontVariationSettings&);

    // MARK: NinePieceImage serializations

    static void serializeNinePieceImageQuad(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const LengthBox&);
    static void serializeNinePieceImageSlices(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NinePieceImage&);
    static void serializeNinePieceImageRepeat(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NinePieceImage&);
    static void serializeNinePieceImage(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const NinePieceImage&);

    // MARK: Animation/Transition serializations

    static void serializeAnimationName(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const ScopedName&, const Animation*, const AnimationList*);
    static void serializeAnimationProperty(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation::TransitionProperty&, const Animation*, const AnimationList*);
    static void serializeAnimationAllowsDiscreteTransitions(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, bool, const Animation*, const AnimationList*);
    static void serializeAnimationDuration(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, MarkableDouble, const Animation*, const AnimationList*);
    static void serializeAnimationDelay(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double, const Animation*, const AnimationList*);
    static void serializeAnimationIterationCount(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, double, const Animation*, const AnimationList*);
    static void serializeAnimationDirection(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, Animation::Direction, const Animation*, const AnimationList*);
    static void serializeAnimationFillMode(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, AnimationFillMode, const Animation*, const AnimationList*);
    static void serializeAnimationCompositeOperation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, CompositeOperation, const Animation*, const AnimationList*);
    static void serializeAnimationPlayState(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, AnimationPlayState, const Animation*, const AnimationList*);
    static void serializeAnimationTimeline(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation::Timeline&, const Animation*, const AnimationList*);
    static void serializeAnimationTimingFunction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimingFunction&, const Animation*, const AnimationList*);
    static void serializeAnimationTimingFunction(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimingFunction*, const Animation*, const AnimationList*);
    static void serializeAnimationSingleRange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, SingleTimelineRange::Type);
    static void serializeAnimationRangeStart(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static void serializeAnimationRangeEnd(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const SingleTimelineRange&, const Animation*, const AnimationList*);
    static void serializeAnimationRange(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const TimelineRange&, const Animation*, const AnimationList*);
    static void serializeSingleAnimation(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation&);
    static void serializeSingleTransition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Animation&);

    // MARK: Grid serializations

    static void serializeGridAutoFlow(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, GridAutoFlow);
    static void serializeGridPosition(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const GridPosition&);
    static void serializeGridTrackBreadth(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const GridLength&);
    static void serializeGridTrackSize(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const GridTrackSize&);
    static void serializeGridTrackSizeList(ExtractorState&, StringBuilder&, const CSS::SerializationContext&, const Vector<GridTrackSize>&);
};

// MARK: - Strong value serializations

template<typename T> void ExtractorSerializer::serializeStyleType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const T& value)
{
    serializationForCSS(builder, context, state.style, value);
}

// MARK: - Primitive serializations

template<typename ConvertibleType>
void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ConvertibleType& value)
{
    serializationForCSS(builder, context, state.style, value);
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, float value)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, int value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, int> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, unsigned short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, unsigned short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, short value)
{
    CSS::serializationForCSS(builder, context, CSS::IntegerRaw<CSS::All, short> { value });
}

inline void ExtractorSerializer::serialize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        serializationForCSS(builder, context, state.style, CustomIdentifier { scopedName.name });
    else
        serializationForCSS(builder, context, state.style, scopedName.name);
}

inline void ExtractorSerializer::serializeLength(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state.style, builder, context, length);
}

inline void ExtractorSerializer::serializeLength(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        serializationForCSS(builder, context, style, CSS::Keyword::Auto { });
        return;
    case LengthType::Content:
        serializationForCSS(builder, context, style, CSS::Keyword::Content { });
        return;
    case LengthType::FillAvailable:
        serializationForCSS(builder, context, style, CSS::Keyword::WebkitFillAvailable { });
        return;
    case LengthType::FitContent:
        serializationForCSS(builder, context, style, CSS::Keyword::FitContent { });
        return;
    case LengthType::Intrinsic:
        serializationForCSS(builder, context, style, CSS::Keyword::Intrinsic { });
        return;
    case LengthType::MinIntrinsic:
        serializationForCSS(builder, context, style, CSS::Keyword::MinIntrinsic { });
        return;
    case LengthType::MinContent:
        serializationForCSS(builder, context, style, CSS::Keyword::MinContent { });
        return;
    case LengthType::MaxContent:
        serializationForCSS(builder, context, style, CSS::Keyword::MaxContent { });
        return;
    case LengthType::Normal:
        serializationForCSS(builder, context, style, CSS::Keyword::Normal { });
        return;
    case LengthType::Fixed:
        CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(length.value(), style) });
        return;
    case LengthType::Percent:
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { length.value() });
        return;
    case LengthType::Calculated:
        builder.append(CSSCalcValue::create(length.protectedCalculationValue(), style)->customCSSText(context));
        return;
    case LengthType::Relative:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeLengthAllowingNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state, builder, context, length);
}

inline void ExtractorSerializer::serializeLengthOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    serializeLength(state, builder, context, length);
}

inline void ExtractorSerializer::serializeLengthWithoutApplyingZoom(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& length)
{
    switch (length.type()) {
    case LengthType::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    case LengthType::Content:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Content { });
        return;
    case LengthType::FillAvailable:
        serializationForCSS(builder, context, state.style, CSS::Keyword::WebkitFillAvailable { });
        return;
    case LengthType::FitContent:
        serializationForCSS(builder, context, state.style, CSS::Keyword::FitContent { });
        return;
    case LengthType::Intrinsic:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Intrinsic { });
        return;
    case LengthType::MinIntrinsic:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MinIntrinsic { });
        return;
    case LengthType::MinContent:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MinContent { });
        return;
    case LengthType::MaxContent:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MaxContent { });
        return;
    case LengthType::Normal:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    case LengthType::Fixed:
        CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, length.value() });
        return;
    case LengthType::Percent:
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { length.value() });
        return;
    case LengthType::Calculated:
        builder.append(CSSCalcValue::create(length.protectedCalculationValue(), state.style)->customCSSText(context));
        return;
    case LengthType::Relative:
    case LengthType::Undefined:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

template<typename T> void ExtractorSerializer::serializeNumber(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serialize(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeNumberAsPixels(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, adjustFloatForAbsoluteZoom(number, state.style) });
}

template<typename T> void ExtractorSerializer::serializeComputedLength(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    serializeNumberAsPixels(state, builder, context, number);
}

template<typename T, CSSValueID keyword> void ExtractorSerializer::serializeNumberOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T number)
{
    if (number < 0) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serialize(state, builder, context, number);
}

template<typename T> void ExtractorSerializer::serializeLineWidth(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, T lineWidth)
{
    serializeNumberAsPixels(state, builder, context, lineWidth);
}

template<CSSValueID keyword> void ExtractorSerializer::serializeStringOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const String& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serializationForCSS(builder, context, state.style, string);
}

template<CSSValueID keyword> void ExtractorSerializer::serializeCustomIdentOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const String& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serializationForCSS(builder, context, state.style, CustomIdentifier { AtomString { string } });
}

template<CSSValueID keyword> void ExtractorSerializer::serializeStringAtomOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AtomString& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serializationForCSS(builder, context, state.style, string);
}

template<CSSValueID keyword> void ExtractorSerializer::serializeCustomIdentAtomOrKeyword(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const AtomString& string)
{
    if (string.isNull()) {
        serializationForCSS(builder, context, state.style, Constant<keyword> { });
        return;
    }

    serializationForCSS(builder, context, state.style, CustomIdentifier { string });
}

// MARK: - SVG serializations

inline void ExtractorSerializer::serializeSVGURIReference(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const URL& marker)
{
    if (marker.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    serializationForCSS(builder, context, state.style, marker);
}

inline void ExtractorSerializer::serializeSVGPaint(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, SVGPaintType paintType, const URL& url, const Color& color)
{
    switch (paintType) {
    case SVGPaintType::URI:
        CSS::serializationForCSS(builder, context, toCSS(url, state.style));
        return;

    case SVGPaintType::URINone:
        CSS::serializationForCSS(builder, context, toCSS(url, state.style));
        builder.append(' ');
        [[fallthrough]];
    case SVGPaintType::None:
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;

    case SVGPaintType::URICurrentColor:
    case SVGPaintType::URIRGBColor:
        CSS::serializationForCSS(builder, context, toCSS(url, state.style));
        builder.append(' ');
        [[fallthrough]];
    case SVGPaintType::RGBColor:
    case SVGPaintType::CurrentColor:
        serializeStyleType(state, builder, context, color);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Transform serializations

inline void ExtractorSerializer::serializeTransformationMatrix(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    serializeTransformationMatrix(state.style, builder, context, transform);
}

inline void ExtractorSerializer::serializeTransformationMatrix(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformationMatrix& transform)
{
    auto zoom = style.usedZoom();
    if (transform.isAffine()) {
        std::array values { transform.a(), transform.b(), transform.c(), transform.d(), transform.e() / zoom, transform.f() / zoom };
        builder.append(nameLiteral(CSSValueMatrix), '(', interleave(values, [&](auto& builder, auto& value) {
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
        }, ", "_s), ')');
        return;
    }

    std::array values {
        transform.m11(), transform.m12(), transform.m13(), transform.m14() * zoom,
        transform.m21(), transform.m22(), transform.m23(), transform.m24() * zoom,
        transform.m31(), transform.m32(), transform.m33(), transform.m34() * zoom,
        transform.m41() / zoom, transform.m42() / zoom, transform.m43() / zoom, transform.m44()
    };
    builder.append(nameLiteral(CSSValueMatrix3d), '(', interleave(values, [&](auto& builder, auto& value) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    }, ", "_s), ')');
}

inline void ExtractorSerializer::serializeTransformOperation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TransformOperation& operation)
{
    serializeTransformOperation(state.style, builder, context, operation);
}

inline void ExtractorSerializer::serializeTransformOperation(const RenderStyle& style, StringBuilder& builder, const CSS::SerializationContext& context, const TransformOperation& operation)
{
    auto translateLength = [&](const auto& length) {
        if (length.isZero()) {
            builder.append("0px"_s);
            return;
        }
        serializeLength(style, builder, context, length);
    };

    auto translateAngle = [&](auto angle) {
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { CSS::AngleUnit::Deg, angle });
    };

    auto translateNumber = [&](auto number) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { number });
    };


    auto includeLength = [](const auto& length) -> bool {
        return !length.isZero() || length.isPercent();
    };

    switch (operation.type()) {
    case TransformOperation::Type::TranslateX:
        builder.append(nameLiteral(CSSValueTranslateX), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateY:
        builder.append(nameLiteral(CSSValueTranslateY), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::TranslateZ:
        builder.append(nameLiteral(CSSValueTranslateZ), '(');
        translateLength(uncheckedDowncast<TranslateTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Translate:
    case TransformOperation::Type::Translate3D: {
        auto& translate = uncheckedDowncast<TranslateTransformOperation>(operation);
        if (!translate.is3DOperation()) {
            if (!includeLength(translate.y())) {
                builder.append(nameLiteral(CSSValueTranslate), '(');
                translateLength(translate.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueTranslate), '(');
            translateLength(translate.x());
            builder.append(", "_s);
            translateLength(translate.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueTranslate3d), '(');
        translateLength(translate.x());
        builder.append(", "_s);
        translateLength(translate.y());
        builder.append(", "_s);
        translateLength(translate.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::ScaleX:
        builder.append(nameLiteral(CSSValueScaleX), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).x());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleY:
        builder.append(nameLiteral(CSSValueScaleY), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).y());
        builder.append(')');
        return;
    case TransformOperation::Type::ScaleZ:
        builder.append(nameLiteral(CSSValueScaleZ), '(');
        translateNumber(uncheckedDowncast<ScaleTransformOperation>(operation).z());
        builder.append(')');
        return;
    case TransformOperation::Type::Scale:
    case TransformOperation::Type::Scale3D: {
        auto& scale = uncheckedDowncast<ScaleTransformOperation>(operation);
        if (!scale.is3DOperation()) {
            if (scale.x() == scale.y()) {
                builder.append(nameLiteral(CSSValueScale), '(');
                translateNumber(scale.x());
                builder.append(')');
                return;
            }
            builder.append(nameLiteral(CSSValueScale), '(');
            translateNumber(scale.x());
            builder.append(", "_s);
            translateNumber(scale.y());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueScale3d), '(');
        translateNumber(scale.x());
        builder.append(", "_s);
        translateNumber(scale.y());
        builder.append(", "_s);
        translateNumber(scale.z());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::RotateX:
        builder.append(nameLiteral(CSSValueRotateX), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateY:
        builder.append(nameLiteral(CSSValueRotateY), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::RotateZ:
        builder.append(nameLiteral(CSSValueRotateZ), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate:
        builder.append(nameLiteral(CSSValueRotate), '(');
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    case TransformOperation::Type::Rotate3D: {
        auto& rotate = uncheckedDowncast<RotateTransformOperation>(operation);
        builder.append(nameLiteral(CSSValueRotate3d), '(');
        translateNumber(rotate.x());
        builder.append(", "_s);
        translateNumber(rotate.y());
        builder.append(", "_s);
        translateNumber(rotate.z());
        builder.append(", "_s);
        translateAngle(uncheckedDowncast<RotateTransformOperation>(operation).angle());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::SkewX:
        builder.append(nameLiteral(CSSValueSkewX), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleX());
        builder.append(')');
        return;
    case TransformOperation::Type::SkewY:
        builder.append(nameLiteral(CSSValueSkewY), '(');
        translateAngle(uncheckedDowncast<SkewTransformOperation>(operation).angleY());
        builder.append(')');
        return;
    case TransformOperation::Type::Skew: {
        auto& skew = uncheckedDowncast<SkewTransformOperation>(operation);
        if (!skew.angleY()) {
            builder.append(nameLiteral(CSSValueSkew), '(');
            translateAngle(skew.angleX());
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValueSkew), '(');
        translateAngle(skew.angleX());
        builder.append(", "_s);
        translateAngle(skew.angleY());
        builder.append(')');
        return;
    }
    case TransformOperation::Type::Perspective:
        if (auto perspective = uncheckedDowncast<PerspectiveTransformOperation>(operation).perspective()) {
            builder.append(nameLiteral(CSSValuePerspective), '(');
            serializeLength(style, builder, context, *perspective);
            builder.append(')');
            return;
        }
        builder.append(nameLiteral(CSSValuePerspective), '(', nameLiteralForSerialization(CSSValueNone), ')');
        return;
    case TransformOperation::Type::Matrix:
    case TransformOperation::Type::Matrix3D: {
        TransformationMatrix transform;
        operation.apply(transform, { });
        serializeTransformationMatrix(style, builder, context, transform);
        return;
    }
    case TransformOperation::Type::Identity:
    case TransformOperation::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Shared serializations

inline void ExtractorSerializer::serializeOpacity(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, float opacity)
{
    serialize(state, builder, context, opacity);
}

inline void ExtractorSerializer::serializeImageOrNone(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleImage* image)
{
    if (!image) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(image->computedStyleValue(state.style)->cssText(context));
}

inline void ExtractorSerializer::serializeGlyphOrientation(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 270_css_deg });
        return;
    case GlyphOrientation::Auto:
        ASSERT_NOT_REACHED();
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeGlyphOrientationOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GlyphOrientation orientation)
{
    switch (orientation) {
    case GlyphOrientation::Degrees0:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 0_css_deg });
        return;
    case GlyphOrientation::Degrees90:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 90_css_deg });
        return;
    case GlyphOrientation::Degrees180:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 180_css_deg });
        return;
    case GlyphOrientation::Degrees270:
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { 270_css_deg });
        return;
    case GlyphOrientation::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeListStyleType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ListStyleType& listStyleType)
{
    if (listStyleType.type == ListStyleType::Type::String) {
        serializationForCSS(builder, context, state.style, listStyleType.identifier);
        return;
    }
    if (listStyleType.type == ListStyleType::Type::CounterStyle) {
        serializationForCSS(builder, context, state.style, CustomIdentifier { listStyleType.identifier });
        return;
    }

    serialize(state, builder, context, listStyleType.type);
}

inline void ExtractorSerializer::serializeMarginTrim(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<MarginTrimType> marginTrim)
{
    if (marginTrim.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // Try to serialize into one of the "block" or "inline" shorthands
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd }) && !marginTrim.containsAny({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Block { });
        return;
    }
    if (marginTrim.containsAll({ MarginTrimType::InlineStart, MarginTrimType::InlineEnd }) && !marginTrim.containsAny({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Inline { });
        return;
    }
    if (marginTrim.containsAll({ MarginTrimType::BlockStart, MarginTrimType::BlockEnd, MarginTrimType::InlineStart, MarginTrimType::InlineEnd })) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Block { });
        builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::Inline { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](MarginTrimType test, CSSValueID value) {
        if (marginTrim.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(MarginTrimType::BlockStart, CSSValueBlockStart);
    appendOption(MarginTrimType::InlineStart, CSSValueInlineStart);
    appendOption(MarginTrimType::BlockEnd, CSSValueBlockEnd);
    appendOption(MarginTrimType::InlineEnd, CSSValueInlineEnd);
}

inline void ExtractorSerializer::serializeBasicShape(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const BasicShape& basicShape, PathConversion pathConversion)
{
    WTF::switchOn(basicShape,
        [&](const auto& shape) {
            CSS::serializationForCSS(builder, context, CSS::BasicShape { toCSS(shape, state.style) });
        },
        [&](const PathFunction& path)  {
            CSS::serializationForCSS(builder, context, CSS::BasicShape { overrideToCSS(path, state.style, pathConversion) });
        }
    );
}

inline void ExtractorSerializer::serializeShapeValue(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ShapeValue* shapeValue)
{
    if (!shapeValue) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (shapeValue->type() == ShapeValue::Type::Box) {
        serialize(state, builder, context, shapeValue->cssBox());
        return;
    }

    if (shapeValue->type() == ShapeValue::Type::Image) {
        serializeImageOrNone(state, builder, context, shapeValue->image());
        return;
    }

    ASSERT(shapeValue->type() == ShapeValue::Type::Shape);
    if (shapeValue->cssBox() == CSSBoxType::BoxMissing) {
        serializeBasicShape(state, builder, context, *shapeValue->shape());
        return;
    }

    serializeBasicShape(state, builder, context, *shapeValue->shape());
    builder.append(' ');
    serialize(state, builder, context, shapeValue->cssBox());
}

inline void ExtractorSerializer::serializePathOperation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const PathOperation* operation, PathConversion conversion)
{
    if (!operation) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    switch (operation->type()) {
    case PathOperation::Type::Reference: {
        auto& reference = uncheckedDowncast<ReferencePathOperation>(*operation);
        CSS::serializationForCSS(builder, context, toCSS(reference.url(), state.style));
        return;
    }

    case PathOperation::Type::Shape: {
        auto& shape = uncheckedDowncast<ShapePathOperation>(*operation);
        if (shape.referenceBox() == CSSBoxType::BoxMissing) {
            serializeBasicShape(state, builder, context, shape.shape(), conversion);
            return;
        }

        serializeBasicShape(state, builder, context, shape.shape(), conversion);
        builder.append(' ');
        serialize(state, builder, context, shape.referenceBox());
        return;
    }

    case PathOperation::Type::Box: {
        auto& box = uncheckedDowncast<BoxPathOperation>(*operation);
        serialize(state, builder, context, box.referenceBox());
        return;
    }

    case PathOperation::Type::Ray: {
        auto& ray = uncheckedDowncast<RayPathOperation>(*operation);
        if (ray.referenceBox() == CSSBoxType::BoxMissing) {
            CSS::serializationForCSS(builder, context, toCSS(ray.ray(), state.style));
            return;
        }
        CSS::serializationForCSS(builder, context, toCSS(ray.ray(), state.style));
        builder.append(' ');
        serialize(state, builder, context, ray.referenceBox());
        return;
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializePathOperationForceAbsolute(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const PathOperation* operation)
{
    serializePathOperation(state, builder, context, operation, PathConversion::ForceAbsolute);
}

inline void ExtractorSerializer::serializeDPath(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StylePathData* path)
{
    if (!path) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSS::serializationForCSS(builder, context, overrideToCSS(Ref { *path }->path(), state.style, PathConversion::ForceAbsolute));
}

inline void ExtractorSerializer::serializeStrokeDashArray(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<WebCore::Length>& dashes)
{
    if (dashes.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(dashes, [&](auto& builder, auto& dash) {
        serializeLength(state, builder, context, dash);
    }, ", "_s));
}

inline void ExtractorSerializer::serializeTextStrokeWidth(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, float textStrokeWidth)
{
    serializeNumberAsPixels(state, builder, context, textStrokeWidth);
}

inline void ExtractorSerializer::serializeFilterOperations(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FilterOperations& filterOperations)
{
    CSS::serializationForCSS(builder, context, toCSSFilterProperty(filterOperations, state.style));
}

inline void ExtractorSerializer::serializeAppleColorFilterOperations(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FilterOperations& filterOperations)
{
    CSS::serializationForCSS(builder, context, toCSSAppleColorFilterProperty(filterOperations, state.style));
}

inline void ExtractorSerializer::serializeWebkitTextCombine(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextCombine textCombine)
{
    if (textCombine == TextCombine::All) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Horizontal { });
        return;
    }
    serialize(state, builder, context, textCombine);
}

inline void ExtractorSerializer::serializeImageOrientation(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, ImageOrientation imageOrientation)
{
    builder.append(nameLiteralForSerialization(imageOrientation == ImageOrientation::Orientation::FromImage ? CSSValueFromImage : CSSValueNone));
}

inline void ExtractorSerializer::serializeLineClamp(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LineClampValue& lineClamp)
{
    if (lineClamp.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (lineClamp.isPercentage()) {
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { lineClamp.value() });
        return;
    }
    serialize(state, builder, context, lineClamp.value());
}

inline void ExtractorSerializer::serializeContain(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<Containment> containment)
{
    if (!containment) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (containment == RenderStyle::strictContainment()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Strict { });
        return;
    }
    if (containment == RenderStyle::contentContainment()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Content { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](Containment test, CSSValueID value) {
        if (containment & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(Containment::Size, CSSValueSize);
    appendOption(Containment::InlineSize, CSSValueInlineSize);
    appendOption(Containment::Layout, CSSValueLayout);
    appendOption(Containment::Style, CSSValueStyle);
    appendOption(Containment::Paint, CSSValuePaint);
}

inline void ExtractorSerializer::serializeMaxLines(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, size_t maxLines)
{
    if (!maxLines) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { maxLines });
}

inline void ExtractorSerializer::serializeSmoothScrolling(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, bool useSmoothScrolling)
{
    builder.append(nameLiteralForSerialization(useSmoothScrolling ? CSSValueSmooth : CSSValueAuto));
}

inline void ExtractorSerializer::serializeInitialLetter(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, IntSize initialLetter)
{
    auto append = [&](auto axis) {
        if (!axis)
            serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        else
            CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { axis });
    };

    if (initialLetter.width() == initialLetter.height()) {
        append(initialLetter.width());
        return;
    }

    append(initialLetter.width());
    builder.append(' ');
    append(initialLetter.height());
}

inline void ExtractorSerializer::serializeTextSpacingTrim(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextSpacingTrim textSpacingTrim)
{
    switch (textSpacingTrim.type()) {
    case TextSpacingTrim::TrimType::SpaceAll:
        serializationForCSS(builder, context, state.style, CSS::Keyword::SpaceAll { });
        return;
    case TextSpacingTrim::TrimType::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    case TextSpacingTrim::TrimType::TrimAll:
        serializationForCSS(builder, context, state.style, CSS::Keyword::TrimAll { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeTextAutospace(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, TextAutospace textAutospace)
{
    if (textAutospace.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    if (textAutospace.isNoAutospace()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::NoAutospace { });
        return;
    }

    if (textAutospace.isNormal()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    if (textAutospace.hasIdeographAlpha() && textAutospace.hasIdeographNumeric()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographAlpha { });
        builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographNumeric { });
        return;
    }

    if (textAutospace.hasIdeographAlpha()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographAlpha { });
        return;
    }

    if (textAutospace.hasIdeographNumeric()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::IdeographNumeric { });
        return;
    }
}

inline void ExtractorSerializer::serializeReflection(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleReflection* reflection)
{
    if (!reflection) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // FIXME: Consider omitting 0px when the mask is null.

    auto offset = [&] -> Ref<CSSPrimitiveValue> {
        auto& reflectionOffset = reflection->offset();
        if (reflectionOffset.isPercentOrCalculated())
            return CSSPrimitiveValue::create(reflectionOffset.percent(), CSSUnitType::CSS_PERCENTAGE);
        return ExtractorConverter::convertNumberAsPixels(state, reflectionOffset.value());
    }();

    auto mask = [&] -> RefPtr<CSSValue> {
        auto& reflectionMask = reflection->mask();
        RefPtr reflectionMaskImageSource = reflectionMask.image();
        if (!reflectionMaskImageSource)
            return CSSPrimitiveValue::create(CSSValueNone);
        if (reflectionMask.overridesBorderWidths())
            return nullptr;
        return ExtractorConverter::convertNinePieceImage(state, reflectionMask);
    }();

    builder.append(CSSReflectValue::create(
        toCSSValueID(reflection->direction()),
        WTFMove(offset),
        WTFMove(mask)
    )->cssText(context));
}

inline void ExtractorSerializer::serializeLineFitEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Leading && textEdge.under == TextEdgeType::Leading) {
        serialize(state, builder, context, textEdge.over);
        return;
    }

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge) {
        serialize(state, builder, context, textEdge.over);
        return;
    }

    serialize(state, builder, context, textEdge.over);
    builder.append(' ');
    serialize(state, builder, context, textEdge.under);
}

inline void ExtractorSerializer::serializeTextBoxEdge(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextEdge& textEdge)
{
    if (textEdge.over == TextEdgeType::Auto && textEdge.under == TextEdgeType::Auto) {
        serialize(state, builder, context, textEdge.over);
        return;
    }

    // https://www.w3.org/TR/css-inline-3/#text-edges
    // "If only one value is specified, both edges are assigned that same keyword if possible; else text is assumed as the missing value."
    auto shouldSerializeUnderEdge = [&] {
        if (textEdge.over == TextEdgeType::CapHeight || textEdge.over == TextEdgeType::ExHeight)
            return textEdge.under != TextEdgeType::Text;
        return textEdge.over != textEdge.under;
    }();

    if (!shouldSerializeUnderEdge) {
        serialize(state, builder, context, textEdge.over);
        return;
    }

    serialize(state, builder, context, textEdge.over);
    builder.append(' ');
    serialize(state, builder, context, textEdge.under);
}

inline void ExtractorSerializer::serializeQuotes(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const QuotesData* quotes)
{
    if (!quotes) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    unsigned size = quotes->size();
    if (!size) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSSValueListBuilder list;
    for (unsigned i = 0; i < size; ++i) {
        list.append(CSSPrimitiveValue::create(quotes->openQuote(i)));
        list.append(CSSPrimitiveValue::create(quotes->closeQuote(i)));
    }
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeBorderRadiusCorner(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthSize& radius)
{
    if (radius.width == radius.height) {
        serializeLength(state, builder, context, radius.width);
        return;
    }

    serializeLength(state, builder, context, radius.width);
    builder.append(' ');
    serializeLength(state, builder, context, radius.height);
}

inline void ExtractorSerializer::serializeContainerNames(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<ScopedName>& containerNames)
{
    if (containerNames.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(containerNames, [&](auto& builder, auto& containerName) {
        serialize(state, builder, context, containerName);
    }, ' '));
}

inline void ExtractorSerializer::serializeViewTransitionClasses(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<ScopedName>& classList)
{
    if (classList.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(classList, [&](auto& builder, auto& className) {
        serialize(state, builder, context, className);
    }, ' '));
}

inline void ExtractorSerializer::serializeViewTransitionName(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ViewTransitionName& viewTransitionName)
{
    if (viewTransitionName.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (viewTransitionName.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializationForCSS(builder, context, state.style, CustomIdentifier { viewTransitionName.customIdent() });
}

inline void ExtractorSerializer::serializeBoxShadow(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<BoxShadow>& shadows)
{
    if (shadows.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSS::BoxShadowProperty::List list;

    for (const auto& shadow : makeReversedRange(shadows))
        list.value.append(toCSS(shadow, state.style));

    CSS::serializationForCSS(builder, context, CSS::BoxShadowProperty { WTFMove(list) });
}

inline void ExtractorSerializer::serializeTextShadow(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<TextShadow>& shadows)
{
    if (shadows.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSS::TextShadowProperty::List list;

    for (const auto& shadow : makeReversedRange(shadows))
        list.value.append(toCSS(shadow, state.style));

    CSS::serializationForCSS(builder, context, CSS::TextShadowProperty { WTFMove(list) });
}

inline void ExtractorSerializer::serializePositionTryFallbacks(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<PositionTryFallback>& fallbacks)
{
    if (fallbacks.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSSValueListBuilder list;
    for (auto& fallback : fallbacks) {
        if (fallback.positionAreaProperties) {
            auto areaValue = fallback.positionAreaProperties->getPropertyCSSValue(CSSPropertyPositionArea);
            if (areaValue)
                list.append(*areaValue);
            continue;
        }

        CSSValueListBuilder singleFallbackList;
        if (fallback.positionTryRuleName)
            singleFallbackList.append(ExtractorConverter::convert(state, *fallback.positionTryRuleName));
        for (auto& tactic : fallback.tactics)
            singleFallbackList.append(ExtractorConverter::convert(state, tactic));
        list.append(CSSValueList::createSpaceSeparated(singleFallbackList));
    }

    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeWillChange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WillChangeData* willChangeData)
{
    if (!willChangeData || !willChangeData->numFeatures()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    CSSValueListBuilder list;
    for (size_t i = 0; i < willChangeData->numFeatures(); ++i) {
        auto feature = willChangeData->featureAt(i);
        switch (feature.first) {
        case WillChangeData::Feature::ScrollPosition:
            list.append(CSSPrimitiveValue::create(CSSValueScrollPosition));
            break;
        case WillChangeData::Feature::Contents:
            list.append(CSSPrimitiveValue::create(CSSValueContents));
            break;
        case WillChangeData::Feature::Property:
            list.append(CSSPrimitiveValue::create(feature.second));
            break;
        case WillChangeData::Feature::Invalid:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeBlockEllipsis(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const BlockEllipsis& blockEllipsis)
{
    switch (blockEllipsis.type) {
    case BlockEllipsis::Type::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case BlockEllipsis::Type::Auto:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    case BlockEllipsis::Type::String:
        serializationForCSS(builder, context, state.style, blockEllipsis.string);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeBlockStepSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, std::optional<WebCore::Length> blockStepSize)
{
    if (!blockStepSize) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    serializeLength(state, builder, context, *blockStepSize);
}

inline void ExtractorSerializer::serializeGapLength(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const GapLength& gapLength)
{
    if (gapLength.isNormal()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    serializeLength(state, builder, context, gapLength.length());
}

inline void ExtractorSerializer::serializeTabSize(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, const TabSize& tabSize)
{
    auto value = tabSize.widthInPixels(1.0);
    if (tabSize.isSpaces())
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { value });
    else
        CSS::serializationForCSS(builder, context, CSS::LengthRaw<> { CSS::LengthUnit::Px, value });
}

inline void ExtractorSerializer::serializeScrollSnapType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapType& type)
{
    if (type.strictness == ScrollSnapStrictness::None) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (type.strictness == ScrollSnapStrictness::Proximity) {
        serialize(state, builder, context, type.axis);
        return;
    }

    serialize(state, builder, context, type.axis);
    builder.append(' ');
    serialize(state, builder, context, type.strictness);
}

inline void ExtractorSerializer::serializeScrollSnapAlign(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollSnapAlign& alignment)
{
    if (alignment.blockAlign == alignment.inlineAlign) {
        serialize(state, builder, context, alignment.blockAlign);
        return;
    }

    serialize(state, builder, context, alignment.blockAlign);
    builder.append(' ');
    serialize(state, builder, context, alignment.inlineAlign);
}

inline void ExtractorSerializer::serializeScrollbarColor(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, std::optional<ScrollbarColor> scrollbarColor)
{
    if (!scrollbarColor) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializeStyleType(state, builder, context, scrollbarColor->thumbColor);
    builder.append(' ');
    serializeStyleType(state, builder, context, scrollbarColor->trackColor);
}

inline void ExtractorSerializer::serializeScrollbarGutter(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScrollbarGutter& gutter)
{
    if (!gutter.bothEdges) {
        if (gutter.isAuto)
            serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        else
            serializationForCSS(builder, context, state.style, CSS::Keyword::Stable { });
        return;
    }

    serializationForCSS(builder, context, state.style, CSS::Keyword::Stable { });
    builder.append(' ');
    serializationForCSS(builder, context, state.style, CSS::Keyword::BothEdges { });
}

inline void ExtractorSerializer::serializeLineBoxContain(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<Style::LineBoxContain> lineBoxContain)
{
    if (!lineBoxContain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](LineBoxContain test, CSSValueID value) {
        if (lineBoxContain.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(LineBoxContain::Block, CSSValueBlock);
    appendOption(LineBoxContain::Inline, CSSValueInline);
    appendOption(LineBoxContain::Font, CSSValueFont);
    appendOption(LineBoxContain::Glyphs, CSSValueGlyphs);
    appendOption(LineBoxContain::Replaced, CSSValueReplaced);
    appendOption(LineBoxContain::InlineBox, CSSValueInlineBox);
    appendOption(LineBoxContain::InitialLetter, CSSValueInitialLetter);
}

inline void ExtractorSerializer::serializeWebkitRubyPosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, RubyPosition position)
{
    switch (position) {
    case RubyPosition::Over:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Before { });
        return;
    case RubyPosition::Under:
        serializationForCSS(builder, context, state.style, CSS::Keyword::After { });
        return;
    case RubyPosition::InterCharacter:
    case RubyPosition::LegacyInterCharacter:
        serializationForCSS(builder, context, state.style, CSS::Keyword::InterCharacter { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializePosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthPoint& position)
{
    serializeLength(state, builder, context, position.x);
    builder.append(' ');
    serializeLength(state, builder, context, position.y);
}

inline void ExtractorSerializer::serializePositionOrAuto(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthPoint& position)
{
    if (position.x.isAuto() && position.y.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializePosition(state, builder, context, position);
}

inline void ExtractorSerializer::serializePositionOrAutoOrNormal(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthPoint& position)
{
    if (position.x.isAuto() && position.y.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    if (position.x.isNormal()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    serializePosition(state, builder, context, position);
}

inline void ExtractorSerializer::serializeContainIntrinsicSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ContainIntrinsicSizeType& type, const std::optional<WebCore::Length>& containIntrinsicLength)
{
    switch (type) {
    case ContainIntrinsicSizeType::None:
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    case ContainIntrinsicSizeType::Length:
        serializeLength(state, builder, context, *containIntrinsicLength);
        return;
    case ContainIntrinsicSizeType::AutoAndLength:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        builder.append(' ');
        serializeLength(state, builder, context, *containIntrinsicLength);
        return;
    case ContainIntrinsicSizeType::AutoAndNone:
        CSS::serializationForCSS(builder, context, CSS::Keyword::Auto { });
        builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::Keyword::None { });
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeTouchAction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TouchAction> touchActions)
{
    if (touchActions & TouchAction::Auto) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }
    if (touchActions & TouchAction::None) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }
    if (touchActions & TouchAction::Manipulation) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Manipulation { });
        return;
    }

    bool listEmpty = true;
    auto appendOption = [&](TouchAction test, CSSValueID value) {
        if (touchActions & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TouchAction::PanX, CSSValuePanX);
    appendOption(TouchAction::PanY, CSSValuePanY);
    appendOption(TouchAction::PinchZoom, CSSValuePinchZoom);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeTextTransform(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextTransform> textTransform)
{
    bool listEmpty = true;

    if (textTransform.contains(TextTransform::Capitalize)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Capitalize { });
        listEmpty = false;
    } else if (textTransform.contains(TextTransform::Uppercase)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Uppercase { });
        listEmpty = false;
    } else if (textTransform.contains(TextTransform::Lowercase)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Lowercase { });
        listEmpty = false;
    }

    auto appendOption = [&](TextTransform test, CSSValueID value) {
        if (textTransform.contains(test)) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextTransform::FullWidth, CSSValueFullWidth);
    appendOption(TextTransform::FullSizeKana, CSSValueFullSizeKana);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializeTextDecorationLine(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextDecorationLine> textDecorationLine)
{
    // Blink value is ignored.
    bool listEmpty = true;
    auto appendOption = [&](TextDecorationLine test, CSSValueID value) {
        if (textDecorationLine & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextDecorationLine::Underline, CSSValueUnderline);
    appendOption(TextDecorationLine::Overline, CSSValueOverline);
    appendOption(TextDecorationLine::LineThrough, CSSValueLineThrough);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializeTextUnderlineOffset(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextUnderlineOffset& textUnderlineOffset)
{
    if (textUnderlineOffset.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    ASSERT(textUnderlineOffset.isLength());
    auto& length = textUnderlineOffset.length();
    if (length.isPercent()) {
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { length.percent() });
        return;
    }

    serializeLength(state, builder, context, length);
}

inline void ExtractorSerializer::serializeTextUnderlinePosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<TextUnderlinePosition> textUnderlinePosition)
{
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::FromFont) && (textUnderlinePosition & TextUnderlinePosition::Under)));
    ASSERT(!((textUnderlinePosition & TextUnderlinePosition::Left) && (textUnderlinePosition & TextUnderlinePosition::Right)));

    if (textUnderlinePosition.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    bool isFromFont = textUnderlinePosition.contains(TextUnderlinePosition::FromFont);
    bool isUnder = textUnderlinePosition.contains(TextUnderlinePosition::Under);
    bool isLeft = textUnderlinePosition.contains(TextUnderlinePosition::Left);
    bool isRight = textUnderlinePosition.contains(TextUnderlinePosition::Right);

    auto metric = isUnder ? CSSValueUnder : CSSValueFromFont;
    auto side = isLeft ? CSSValueLeft : CSSValueRight;
    if (!isFromFont && !isUnder) {
        builder.append(nameLiteralForSerialization(side));
        return;
    }
    if (!isLeft && !isRight) {
        builder.append(nameLiteralForSerialization(metric));
        return;
    }

    builder.append(nameLiteralForSerialization(metric), ' ', nameLiteralForSerialization(side));
}

inline void ExtractorSerializer::serializeTextDecorationThickness(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextDecorationThickness& textDecorationThickness)
{
    if (textDecorationThickness.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }
    if (textDecorationThickness.isFromFont()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::FromFont { });
        return;
    }

    ASSERT(textDecorationThickness.isLength());
    auto& length = textDecorationThickness.length();
    if (length.isPercent()) {
        CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { length.percent() });
        return;
    }
    return serializeLength(state, builder, context, length);
}

inline void ExtractorSerializer::serializeTextEmphasisPosition(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, OptionSet<TextEmphasisPosition> textEmphasisPosition)
{
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Over) && (textEmphasisPosition & TextEmphasisPosition::Under)));
    ASSERT(!((textEmphasisPosition & TextEmphasisPosition::Left) && (textEmphasisPosition & TextEmphasisPosition::Right)));
    ASSERT((textEmphasisPosition & TextEmphasisPosition::Over) || (textEmphasisPosition & TextEmphasisPosition::Under));

    bool listEmpty = true;
    auto appendOption = [&](TextEmphasisPosition test, CSSValueID value) {
        if (textEmphasisPosition &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(TextEmphasisPosition::Over, CSSValueOver);
    appendOption(TextEmphasisPosition::Under, CSSValueUnder);
    appendOption(TextEmphasisPosition::Left, CSSValueLeft);
}

inline void ExtractorSerializer::serializeSpeakAs(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<SpeakAs> speakAs)
{
    bool listEmpty = true;
    auto appendOption = [&](SpeakAs test, CSSValueID value) {
        if (speakAs &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(SpeakAs::SpellOut, CSSValueSpellOut);
    appendOption(SpeakAs::Digits, CSSValueDigits);
    appendOption(SpeakAs::LiteralPunctuation, CSSValueLiteralPunctuation);
    appendOption(SpeakAs::NoPunctuation, CSSValueNoPunctuation);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
}

inline void ExtractorSerializer::serializeHangingPunctuation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<HangingPunctuation> hangingPunctuation)
{
    bool listEmpty = true;
    auto appendOption = [&](HangingPunctuation test, CSSValueID value) {
        if (hangingPunctuation &  test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(HangingPunctuation::First, CSSValueFirst);
    appendOption(HangingPunctuation::AllowEnd, CSSValueAllowEnd);
    appendOption(HangingPunctuation::ForceEnd, CSSValueForceEnd);
    appendOption(HangingPunctuation::Last, CSSValueLast);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializePageBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakBetween value)
{
    if (value == BreakBetween::Page || value == BreakBetween::LeftPage || value == BreakBetween::RightPage
        || value == BreakBetween::RectoPage || value == BreakBetween::VersoPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { }); // CSS 2.1 allows us to map these to always.
        return;
    }
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializePageBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidPage) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeWebkitColumnBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakBetween value)
{
    if (value == BreakBetween::Column) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { });
        return;
    }
    if (value == BreakBetween::Avoid || value == BreakBetween::AvoidColumn) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeWebkitColumnBreak(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BreakInside value)
{
    if (value == BreakInside::Avoid || value == BreakInside::AvoidColumn) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Avoid { });
        return;
    }
    serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
}

inline void ExtractorSerializer::serializeSelfOrDefaultAlignmentData(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleSelfAlignmentData& data)
{
    CSSValueListBuilder list;
    if (data.positionType() == ItemPositionType::Legacy)
        list.append(CSSPrimitiveValue::create(CSSValueLegacy));
    if (data.position() == ItemPosition::Baseline)
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    else if (data.position() == ItemPosition::LastBaseline) {
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
    } else {
        if (data.position() >= ItemPosition::Center && data.overflow() != OverflowAlignment::Default)
            list.append(ExtractorConverter::convert(state, data.overflow()));
        if (data.position() == ItemPosition::Legacy)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        else
            list.append(ExtractorConverter::convert(state, data.position()));
    }
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeContentAlignmentData(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleContentAlignmentData& data)
{
    CSSValueListBuilder list;

    // Handle content-distribution values
    if (data.distribution() != ContentDistribution::Default)
        list.append(ExtractorConverter::convert(state, data.distribution()));

    // Handle content-position values (either as fallback or actual value)
    switch (data.position()) {
    case ContentPosition::Normal:
        // Handle 'normal' value, not valid as content-distribution fallback.
        if (data.distribution() == ContentDistribution::Default)
            list.append(CSSPrimitiveValue::create(CSSValueNormal));
        break;
    case ContentPosition::LastBaseline:
        list.append(CSSPrimitiveValue::create(CSSValueLast));
        list.append(CSSPrimitiveValue::create(CSSValueBaseline));
        break;
    default:
        // Handle overflow-alignment (only allowed for content-position values)
        if ((data.position() >= ContentPosition::Center || data.distribution() != ContentDistribution::Default) && data.overflow() != OverflowAlignment::Default)
            list.append(ExtractorConverter::convert(state, data.overflow()));
        list.append(ExtractorConverter::convert(state, data.position()));
    }

    ASSERT(list.size() > 0);
    ASSERT(list.size() <= 3);
    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeOffsetRotate(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const OffsetRotation& rotation)
{
    if (rotation.hasAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        builder.append(' ');
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { CSS::AngleUnit::Deg, rotation.angle() });
    } else
        CSS::serializationForCSS(builder, context, CSS::AngleRaw<> { CSS::AngleUnit::Deg, rotation.angle() });
}

inline void ExtractorSerializer::serializePaintOrder(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, PaintOrder paintOrder)
{
    if (paintOrder == PaintOrder::Normal) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    auto appendOne = [&](auto a) {
        builder.append(nameLiteralForSerialization(a));
    };

    auto appendTwo = [&](auto a, auto b) {
        builder.append(nameLiteralForSerialization(a), ' ', nameLiteralForSerialization(b));
    };

    switch (paintOrder) {
    case PaintOrder::Normal:
        ASSERT_NOT_REACHED();
        return;
    case PaintOrder::Fill:
        appendOne(CSSValueFill);
        return;
    case PaintOrder::FillMarkers:
        appendTwo(CSSValueFill, CSSValueMarkers);
        return;
    case PaintOrder::Stroke:
        appendOne(CSSValueStroke);
        return;
    case PaintOrder::StrokeMarkers:
        appendTwo(CSSValueStroke, CSSValueMarkers);
        return;
    case PaintOrder::Markers:
        appendOne(CSSValueMarkers);
        return;
    case PaintOrder::MarkersStroke:
        appendTwo(CSSValueMarkers, CSSValueStroke);
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeScrollTimelineAxes(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<ScrollAxis>& axes)
{
    if (axes.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Block { });
        return;
    }

    builder.append(interleave(axes, [&](auto& builder, auto& axis) {
        builder.append(nameLiteralForSerialization(toCSSValueID(axis)));
    }, ", "_s));
}

inline void ExtractorSerializer::serializeScrollTimelineNames(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<AtomString>& names)
{
    if (names.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(names, [&](auto& builder, auto& name) {
        if (name.isNull())
            serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        else
            serializationForCSS(builder, context, state.style, CustomIdentifier { name });
    }, ", "_s));
}

inline void ExtractorSerializer::serializeAnchorNames(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<ScopedName>& anchorNames)
{
    if (anchorNames.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    builder.append(interleave(anchorNames, [&](auto& builder, auto& anchorName) {
        serialize(state, builder, context, anchorName);
    }, ", "_s));
}

inline void ExtractorSerializer::serializePositionAnchor(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<ScopedName>& positionAnchor)
{
    if (!positionAnchor) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serialize(state, builder, context, *positionAnchor);
}

inline void ExtractorSerializer::serializePositionArea(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const std::optional<PositionArea>& positionArea)
{
    if (!positionArea) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(ExtractorConverter::convertPositionArea(state, *positionArea)->cssText(context));
}

inline void ExtractorSerializer::serializeNameScope(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const NameScope& scope)
{
    switch (scope.type) {
    case NameScope::Type::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case NameScope::Type::All:
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    case NameScope::Type::Ident:
        if (scope.names.isEmpty()) {
            serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
            return;
        }

        builder.append(interleave(scope.names, [&](auto& builder, auto& name) {
            serializationForCSS(builder, context, state.style, CustomIdentifier { name });
        }, ", "_s));
        return;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeSingleViewTimelineInsets(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ViewTimelineInsets& insets)
{
    ASSERT(insets.start);

    if (!insets.end || insets.start == insets.end) {
        serializeLength(state, builder, context, *insets.start);
        return;
    }

    serializeLength(state, builder, context, *insets.start);
    builder.append(' ');
    serializeLength(state, builder, context, *insets.end);
}

inline void ExtractorSerializer::serializeViewTimelineInsets(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FixedVector<ViewTimelineInsets>& insets)
{
    if (insets.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    builder.append(interleave(insets, [&](auto& builder, auto& singleInsets) {
        serializeSingleViewTimelineInsets(state, builder, context, singleInsets);
    }, ", "_s));
}

inline void ExtractorSerializer::serializePositionVisibility(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, OptionSet<PositionVisibility> positionVisibility)
{
    bool listEmpty = true;
    auto appendOption = [&](PositionVisibility test, CSSValueID value) {
        if (positionVisibility & test) {
            if (!listEmpty)
                builder.append(' ');
            builder.append(nameLiteralForSerialization(value));
            listEmpty = false;
        }
    };
    appendOption(PositionVisibility::AnchorsValid, CSSValueAnchorsValid);
    appendOption(PositionVisibility::AnchorsVisible, CSSValueAnchorsVisible);
    appendOption(PositionVisibility::NoOverflow, CSSValueNoOverflow);

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Always { });
}

#if ENABLE(TEXT_AUTOSIZING)
inline void ExtractorSerializer::serializeWebkitTextSizeAdjust(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TextSizeAdjustment& textSizeAdjust)
{
    if (textSizeAdjust.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }
    if (textSizeAdjust.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { textSizeAdjust.percentage() });
}
#endif

#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
inline void ExtractorSerializer::serializeOverflowScrolling(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, bool useTouchOverflowScrolling)
{
    builder.append(nameLiteralForSerialization(useTouchOverflowScrolling ? CSSValueTouch : CSSValueAuto));
}
#endif

#if PLATFORM(IOS_FAMILY)
inline void ExtractorSerializer::serializeTouchCallout(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, bool touchCalloutEnabled)
{
    builder.append(nameLiteralForSerialization(touchCalloutEnabled ? CSSValueDefault : CSSValueNone));
}
#endif

// MARK: - FillLayer serializations

inline void ExtractorSerializer::serializeFillLayerAttachment(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillAttachment attachment)
{
    serialize(state, builder, context, attachment);
}

inline void ExtractorSerializer::serializeFillLayerBlendMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, BlendMode blendMode)
{
    serialize(state, builder, context, blendMode);
}

inline void ExtractorSerializer::serializeFillLayerClip(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillBox clip)
{
    serialize(state, builder, context, clip);
}

inline void ExtractorSerializer::serializeFillLayerOrigin(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillBox origin)
{
    serialize(state, builder, context, origin);
}

inline void ExtractorSerializer::serializeFillLayerXPosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& xPosition)
{
    serializeLength(state, builder, context, xPosition);
}

inline void ExtractorSerializer::serializeFillLayerYPosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const WebCore::Length& yPosition)
{
    serializeLength(state, builder, context, yPosition);
}

inline void ExtractorSerializer::serializeFillLayerRepeat(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillRepeatXY repeat)
{
    if (repeat.x == repeat.y) {
        serialize(state, builder, context, repeat.x);
        return;
    }

    if (repeat.x == FillRepeat::Repeat && repeat.y == FillRepeat::NoRepeat) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::RepeatX { });
        return;
    }

    if (repeat.x == FillRepeat::NoRepeat && repeat.y == FillRepeat::Repeat) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::RepeatY { });
        return;
    }

    serialize(state, builder, context, repeat.x);
    builder.append(' ');
    serialize(state, builder, context, repeat.y);
}

inline void ExtractorSerializer::serializeFillLayerBackgroundSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillSize size)
{
    if (size.type == FillSizeType::Contain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Contain { });
        return;
    }

    if (size.type == FillSizeType::Cover) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Cover { });
        return;
    }

    if (size.size.height.isAuto() && size.size.width.isAuto()) {
        serializeLength(state, builder, context, size.size.width);
        return;
    }

    serializeLength(state, builder, context, size.size.width);
    builder.append(' ');
    serializeLength(state, builder, context, size.size.height);
}

inline void ExtractorSerializer::serializeFillLayerMaskSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, FillSize size)
{
    if (size.type == FillSizeType::Contain) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Contain { });
        return;
    }

    if (size.type == FillSizeType::Cover) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Cover { });
        return;
    }

    if (size.size.height.isAuto()) {
        serializeLength(state, builder, context, size.size.width);
        return;
    }

    serializeLength(state, builder, context, size.size.width);
    builder.append(' ');
    serializeLength(state, builder, context, size.size.height);
}

inline void ExtractorSerializer::serializeFillLayerMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyMaskComposite)));
}

inline void ExtractorSerializer::serializeFillLayerWebkitMaskComposite(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, CompositeOperator composite)
{
    builder.append(nameLiteralForSerialization(toCSSValueID(composite, CSSPropertyWebkitMaskComposite)));
}

inline void ExtractorSerializer::serializeFillLayerMaskMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        serializationForCSS(builder, context, state.style, CSS::Keyword::MatchSource { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeFillLayerWebkitMaskSourceType(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MaskMode maskMode)
{
    switch (maskMode) {
    case MaskMode::Alpha:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    case MaskMode::Luminance:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Luminance { });
        return;
    case MaskMode::MatchSource:
        // MatchSource is only available in the mask-mode property.
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alpha { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeFillLayerImage(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const StyleImage* image)
{
    serializeImageOrNone(state, builder, context, image);
}

// MARK: - Font serializations

inline void ExtractorSerializer::serializeFontFamily(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, const AtomString& family)
{
    auto identifierForFamily = [](const auto& family) {
        if (family == cursiveFamily)
            return CSSValueCursive;
        if (family == fantasyFamily)
            return CSSValueFantasy;
        if (family == monospaceFamily)
            return CSSValueMonospace;
        if (family == pictographFamily)
            return CSSValueWebkitPictograph;
        if (family == sansSerifFamily)
            return CSSValueSansSerif;
        if (family == serifFamily)
            return CSSValueSerif;
        if (family == systemUiFamily)
            return CSSValueSystemUi;
        return CSSValueInvalid;
    };

    if (auto familyIdentifier = identifierForFamily(family))
        builder.append(nameLiteralForSerialization(familyIdentifier));
    else
        builder.append(WebCore::serializeFontFamily(family));
}

inline void ExtractorSerializer::serializeFontSizeAdjust(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontSizeAdjust& fontSizeAdjust)
{
    if (fontSizeAdjust.isNone()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    auto metric = fontSizeAdjust.metric;
    auto value = fontSizeAdjust.shouldResolveFromFont() ? fontSizeAdjust.resolve(state.style.computedFontSize(), state.style.metricsOfPrimaryFont()) : fontSizeAdjust.value.asOptional();

    if (!value) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    }

    if (metric == FontSizeAdjust::Metric::ExHeight) {
        CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { *value });
        return;
    }

    serialize(state, builder, context, metric);
    builder.append(' ');
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { *value });
}

inline void ExtractorSerializer::serializeFontPalette(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontPalette& fontPalette)
{
    switch (fontPalette.type) {
    case FontPalette::Type::Normal:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    case FontPalette::Type::Light:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Light { });
        return;
    case FontPalette::Type::Dark:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Dark { });
        return;
    case FontPalette::Type::Custom:
        serializationForCSS(builder, context, state.style, CustomIdentifier { fontPalette.identifier });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeFontWeight(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWeight)
{
    CSS::serializationForCSS(builder, context, CSS::NumberRaw<> { static_cast<float>(fontWeight) });
}

inline void ExtractorSerializer::serializeFontWidth(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, FontSelectionValue fontWidth)
{
    CSS::serializationForCSS(builder, context, CSS::PercentageRaw<> { static_cast<float>(fontWidth) });
}

inline void ExtractorSerializer::serializeFontFeatureSettings(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontFeatureSettings& fontFeatureSettings)
{
    if (!fontFeatureSettings.size()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    for (auto& feature : fontFeatureSettings)
        list.append(CSSFontFeatureValue::create(FontTag(feature.tag()), ExtractorConverter::convert(state, feature.value())));
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeFontVariationSettings(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const FontVariationSettings& fontVariationSettings)
{
    if (fontVariationSettings.isEmpty()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    }

    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    for (auto& feature : fontVariationSettings)
        list.append(CSSFontVariationValue::create(feature.tag(), ExtractorConverter::convert(state, feature.value())));
    builder.append(CSSValueList::createCommaSeparated(WTFMove(list))->cssText(context));
}

// MARK: - NinePieceImage serializations

inline void ExtractorSerializer::serializeNinePieceImageQuad(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const LengthBox& box)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    RefPtr<CSSPrimitiveValue> top;
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;

    if (box.top().isRelative())
        top = CSSPrimitiveValue::create(box.top().value());
    else
        top = CSSPrimitiveValue::create(box.top(), state.style);

    if (box.right() == box.top() && box.bottom() == box.top() && box.left() == box.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        if (box.right().isRelative())
            right = CSSPrimitiveValue::create(box.right().value());
        else
            right = CSSPrimitiveValue::create(box.right(), state.style);

        if (box.bottom() == box.top() && box.right() == box.left()) {
            bottom = top;
            left = right;
        } else {
            if (box.bottom().isRelative())
                bottom = CSSPrimitiveValue::create(box.bottom().value());
            else
                bottom = CSSPrimitiveValue::create(box.bottom(), state.style);

            if (box.left() == box.right())
                left = right;
            else {
                if (box.left().isRelative())
                    left = CSSPrimitiveValue::create(box.left().value());
                else
                    left = CSSPrimitiveValue::create(box.left(), state.style);
            }
        }
    }

    builder.append(CSSQuadValue::create({
        top.releaseNonNull(),
        right.releaseNonNull(),
        bottom.releaseNonNull(),
        left.releaseNonNull()
    })->cssText(context));
}

inline void ExtractorSerializer::serializeNinePieceImageSlices(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, const NinePieceImage& image)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    auto sliceSide = [](const WebCore::Length& length) -> Ref<CSSPrimitiveValue> {
        // These values can be percentages or numbers.
        if (length.isPercent())
            return CSSPrimitiveValue::create(length.percent(), CSSUnitType::CSS_PERCENTAGE);
        ASSERT(length.isFixed());
        return CSSPrimitiveValue::create(length.value());
    };

    auto& slices = image.imageSlices();

    RefPtr<CSSPrimitiveValue> top = sliceSide(slices.top());
    RefPtr<CSSPrimitiveValue> right;
    RefPtr<CSSPrimitiveValue> bottom;
    RefPtr<CSSPrimitiveValue> left;
    if (slices.right() == slices.top() && slices.bottom() == slices.top() && slices.left() == slices.top()) {
        right = top;
        bottom = top;
        left = top;
    } else {
        right = sliceSide(slices.right());
        if (slices.bottom() == slices.top() && slices.right() == slices.left()) {
            bottom = top;
            left = right;
        } else {
            bottom = sliceSide(slices.bottom());
            if (slices.left() == slices.right())
                left = right;
            else
                left = sliceSide(slices.left());
        }
    }

    builder.append(CSSBorderImageSliceValue::create({
        top.releaseNonNull(),
        right.releaseNonNull(),
        bottom.releaseNonNull(),
        left.releaseNonNull()
    }, image.fill())->cssText(context));
}

inline void ExtractorSerializer::serializeNinePieceImageRepeat(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, const NinePieceImage& image)
{
    auto valueID = [](NinePieceImageRule rule) -> CSSValueID {
        switch (rule) {
        case NinePieceImageRule::Repeat:
            return CSSValueRepeat;
        case NinePieceImageRule::Round:
            return CSSValueRound;
        case NinePieceImageRule::Space:
            return CSSValueSpace;
        default:
            return CSSValueStretch;
        }
    };

    if (image.horizontalRule() == image.verticalRule())
        builder.append(nameLiteralForSerialization(valueID(image.horizontalRule())));
    else
        builder.append(nameLiteralForSerialization(valueID(image.horizontalRule())), ' ', nameLiteralForSerialization(valueID(image.verticalRule())));
}

inline void ExtractorSerializer::serializeNinePieceImage(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const NinePieceImage& image)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.
    builder.append(createBorderImageValue({
        .source = image.image()->computedStyleValue(state.style),
        .slice = ExtractorConverter::convertNinePieceImageSlices(state, image),
        .width = ExtractorConverter::convertNinePieceImageQuad(state, image.borderSlices()),
        .outset = ExtractorConverter::convertNinePieceImageQuad(state, image.outset()),
        .repeat = ExtractorConverter::convertNinePieceImageRepeat(state, image),
    })->customCSSText(context));
}

// MARK: - Animation/Transition serializations

inline void ExtractorSerializer::serializeAnimationName(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const ScopedName& name, const Animation*, const AnimationList*)
{
    serialize(state, builder, context, name);
}

inline void ExtractorSerializer::serializeAnimationProperty(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation::TransitionProperty& property, const Animation*, const AnimationList*)
{
    switch (property.mode) {
    case Animation::TransitionMode::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case Animation::TransitionMode::All:
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
        return;
    case Animation::TransitionMode::SingleProperty:
    case Animation::TransitionMode::UnknownProperty:
        serializationForCSS(builder, context, state.style, CustomIdentifier { animatablePropertyAsString(property.animatableProperty) });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationAllowsDiscreteTransitions(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext&, bool allowsDiscreteTransitions, const Animation*, const AnimationList*)
{
    builder.append(nameLiteralForSerialization(allowsDiscreteTransitions ? CSSValueAllowDiscrete : CSSValueNormal));
}

inline void ExtractorSerializer::serializeAnimationDuration(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, MarkableDouble duration, const Animation* animation, const AnimationList* animationList)
{
    auto animationListHasMultipleExplicitTimelines = [&] {
        if (!animationList || animationList->size() <= 1)
            return false;
        auto explicitTimelines = 0;
        for (auto& animation : *animationList) {
            if (animation->isTimelineSet())
                ++explicitTimelines;
            if (explicitTimelines > 1)
                return true;
        }
        return false;
    };

    auto animationHasExplicitNonAutoTimeline = [&] {
        if (!animation || !animation->isTimelineSet())
            return false;
        auto* timelineKeyword = std::get_if<Animation::TimelineKeyword>(&animation->timeline());
        return !timelineKeyword || *timelineKeyword != Animation::TimelineKeyword::Auto;
    };

    // https://drafts.csswg.org/css-animations-2/#animation-duration
    // For backwards-compatibility with Level 1, when the computed value of animation-timeline is auto
    // (i.e. only one list value, and that value being auto), the resolved value of auto for
    // animation-duration is 0s whenever its used value would also be 0s.
    if (!duration && (animationListHasMultipleExplicitTimelines() || animationHasExplicitNonAutoTimeline())) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    CSS::serializationForCSS(builder, context, CSS::TimeRaw<> { CSS::TimeUnit::S, duration.value_or(0) });
}

inline void ExtractorSerializer::serializeAnimationDelay(ExtractorState&, StringBuilder& builder, const CSS::SerializationContext& context, double delay, const Animation*, const AnimationList*)
{
    CSS::serializationForCSS(builder, context, CSS::TimeRaw<> { CSS::TimeUnit::S, delay });
}

inline void ExtractorSerializer::serializeAnimationIterationCount(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, double iterationCount, const Animation*, const AnimationList*)
{
    if (iterationCount == Animation::IterationCountInfinite)
        serializationForCSS(builder, context, state.style, CSS::Keyword::Infinite { });
    else
        serialize(state, builder, context, iterationCount);
}

inline void ExtractorSerializer::serializeAnimationDirection(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, Animation::Direction direction, const Animation*, const AnimationList*)
{
    switch (direction) {
    case Animation::Direction::Normal:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Normal { });
        return;
    case Animation::Direction::Alternate:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Alternate { });
        return;
    case Animation::Direction::Reverse:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Reverse { });
        return;
    case Animation::Direction::AlternateReverse:
        serializationForCSS(builder, context, state.style, CSS::Keyword::AlternateReverse { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationFillMode(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, AnimationFillMode fillMode, const Animation*, const AnimationList*)
{
    switch (fillMode) {
    case AnimationFillMode::None:
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
        return;
    case AnimationFillMode::Forwards:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Forwards { });
        return;
    case AnimationFillMode::Backwards:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Backwards { });
        return;
    case AnimationFillMode::Both:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Both { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationCompositeOperation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, CompositeOperation operation, const Animation*, const AnimationList*)
{
    switch (operation) {
    case CompositeOperation::Add:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Add { });
        return;
    case CompositeOperation::Accumulate:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Accumulate { });
        return;
    case CompositeOperation::Replace:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Replace { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationPlayState(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, AnimationPlayState playState, const Animation*, const AnimationList*)
{
    switch (playState) {
    case AnimationPlayState::Playing:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Running { });
        return;
    case AnimationPlayState::Paused:
        serializationForCSS(builder, context, state.style, CSS::Keyword::Paused { });
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeAnimationTimeline(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation::Timeline& timeline, const Animation*, const AnimationList*)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    auto valueForAnonymousScrollTimeline = [&](auto& anonymousScrollTimeline) {
        auto scroller = [&] {
            switch (anonymousScrollTimeline.scroller) {
            case Scroller::Nearest:
                return CSSValueNearest;
            case Scroller::Root:
                return CSSValueRoot;
            case Scroller::Self:
                return CSSValueSelf;
            default:
                ASSERT_NOT_REACHED();
                return CSSValueNearest;
            }
        }();
        return CSSScrollValue::create(
            CSSPrimitiveValue::create(scroller),
            ExtractorConverter::convert(state, anonymousScrollTimeline.axis)
        );
    };

    auto valueForAnonymousViewTimeline = [&](auto& anonymousViewTimeline) {
        auto insetCSSValue = [&](auto& inset) -> RefPtr<CSSValue> {
            if (!inset)
                return nullptr;
            return CSSPrimitiveValue::create(*inset, state.style);
        };
        return CSSViewValue::create(
            ExtractorConverter::convert(state, anonymousViewTimeline.axis),
            insetCSSValue(anonymousViewTimeline.insets.start),
            insetCSSValue(anonymousViewTimeline.insets.end)
        );
    };

    WTF::switchOn(timeline,
        [&](Animation::TimelineKeyword keyword) {
            builder.append(nameLiteralForSerialization(keyword == Animation::TimelineKeyword::None ? CSSValueNone : CSSValueAuto));
        },
        [&](const AtomString& customIdent) {
            serializationForCSS(builder, context, state.style, CustomIdentifier { customIdent });
        },
        [&](const Animation::AnonymousScrollTimeline& anonymousScrollTimeline) {
            builder.append(valueForAnonymousScrollTimeline(anonymousScrollTimeline)->cssText(context));
        },
        [&](const Animation::AnonymousViewTimeline& anonymousViewTimeline) {
            builder.append(valueForAnonymousViewTimeline(anonymousViewTimeline)->cssText(context));
        }
    );
}

inline void ExtractorSerializer::serializeAnimationTimingFunction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimingFunction& timingFunction, const Animation*, const AnimationList*)
{
    // FIXME: Optimize by avoiding CSSEasingFunction conversion.
    CSS::serializationForCSS(builder, context, toCSSEasingFunction(timingFunction, state.style));
}

inline void ExtractorSerializer::serializeAnimationTimingFunction(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimingFunction* timingFunction, const Animation*, const AnimationList*)
{
    // FIXME: Optimize by avoiding CSSEasingFunction conversion.
    CSS::serializationForCSS(builder, context, toCSSEasingFunction(*timingFunction, state.style));
}

inline void ExtractorSerializer::serializeAnimationSingleRange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, SingleTimelineRange::Type type)
{
    bool listEmpty = true;

    if (range.name != SingleTimelineRange::Name::Omitted) {
        builder.append(nameLiteralForSerialization(SingleTimelineRange::valueID(range.name)));
        listEmpty = false;
    }
    if (!SingleTimelineRange::isDefault(range.offset, type)) {
        if (!listEmpty)
            builder.append(' ');
        serializeLength(state, builder, context, range.offset);
    }
}

inline void ExtractorSerializer::serializeAnimationRangeStart(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    serializeAnimationSingleRange(state, builder, context, range, SingleTimelineRange::Type::Start);
}

inline void ExtractorSerializer::serializeAnimationRangeEnd(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const SingleTimelineRange& range, const Animation*, const AnimationList*)
{
    serializeAnimationSingleRange(state, builder, context, range, SingleTimelineRange::Type::End);
}

inline void ExtractorSerializer::serializeAnimationRange(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const TimelineRange& range, const Animation*, const AnimationList*)
{
    // FIXME: Do this more efficiently without creating and destroying a CSSValue object.

    CSSValueListBuilder list;
    auto rangeStart = range.start;
    auto rangeEnd = range.end;

    Ref startValue = ExtractorConverter::convertAnimationSingleRange(state, rangeStart, SingleTimelineRange::Type::Start);
    Ref endValue = ExtractorConverter::convertAnimationSingleRange(state, rangeEnd, SingleTimelineRange::Type::End);
    bool endValueEqualsStart = startValue->equals(endValue);

    if (startValue->length())
        list.append(WTFMove(startValue));

    bool isNormal = rangeEnd.name == SingleTimelineRange::Name::Normal;
    bool isDefaultAndSameNameAsStart = rangeStart.name == rangeEnd.name && SingleTimelineRange::isDefault(rangeEnd.offset, SingleTimelineRange::Type::End);
    if (endValue->length() && !endValueEqualsStart && !isNormal && !isDefaultAndSameNameAsStart)
        list.append(WTFMove(endValue));

    builder.append(CSSValueList::createSpaceSeparated(WTFMove(list))->cssText(context));
}

inline void ExtractorSerializer::serializeSingleAnimation(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation& animation)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());
    static NeverDestroyed<String> alternate { "alternate"_s };
    static NeverDestroyed<String> alternateReverse { "alternate-reverse"_s };
    static NeverDestroyed<String> backwards { "backwards"_s };
    static NeverDestroyed<String> both { "both"_s };
    static NeverDestroyed<String> ease { "ease"_s };
    static NeverDestroyed<String> easeIn { "ease-in"_s };
    static NeverDestroyed<String> easeInOut { "ease-in-out"_s };
    static NeverDestroyed<String> easeOut { "ease-out"_s };
    static NeverDestroyed<String> forwards { "forwards"_s };
    static NeverDestroyed<String> infinite { "infinite"_s };
    static NeverDestroyed<String> linear { "linear"_s };
    static NeverDestroyed<String> normal { "normal"_s };
    static NeverDestroyed<String> paused { "paused"_s };
    static NeverDestroyed<String> reverse { "reverse"_s };
    static NeverDestroyed<String> running { "running"_s };
    static NeverDestroyed<String> stepEnd { "step-end"_s };
    static NeverDestroyed<String> stepStart { "step-start"_s };

    // If we have an animation-delay but no animation-duration set, we must serialize
    // the animation-duration because they're both <time> values and animation-delay
    // comes first.
    auto showsDelay = animation.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || animation.duration() != Animation::initialDuration();

    auto showsTimingFunction = [&] {
        RefPtr timingFunction = animation.timingFunction();
        if (timingFunction && *timingFunction != initialTimingFunction.get())
            return true;
        auto& name = animation.name().name;
        return name == ease || name == easeIn || name == easeInOut || name == easeOut || name == linear || name == stepEnd || name == stepStart;
    };

    auto showsIterationCount = [&] {
        if (animation.iterationCount() != Animation::initialIterationCount())
            return true;
        return animation.name().name == infinite;
    };

    auto showsDirection = [&] {
        if (animation.direction() != Animation::initialDirection())
            return true;
        auto& name = animation.name().name;
        return name == normal || name == reverse || name == alternate || name == alternateReverse;
    };

    auto showsFillMode = [&] {
        if (animation.fillMode() != Animation::initialFillMode())
            return true;
        auto& name = animation.name().name;
        return name == forwards || name == backwards || name == both;
    };

    auto showsPlaysState = [&] {
        if (animation.playState() != Animation::initialPlayState())
            return true;
        auto& name = animation.name().name;
        return name == running || name == paused;
    };

    bool listEmpty = true;

    if (showsDuration) {
        serializeAnimationDuration(state, builder, context, animation.duration(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsTimingFunction()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimingFunction(state, builder, context, *animation.timingFunction(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDelay) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDelay(state, builder, context, animation.delay(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsIterationCount()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationIterationCount(state, builder, context, animation.iterationCount(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDirection()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDirection(state, builder, context, animation.direction(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsFillMode()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationFillMode(state, builder, context, animation.fillMode(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsPlaysState()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationPlayState(state, builder, context, animation.playState(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.name() != Animation::initialName()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationName(state, builder, context, animation.name(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.timeline() != Animation::initialTimeline()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimeline(state, builder, context, animation.timeline(), nullptr, nullptr);
        listEmpty = false;
    }
    if (animation.compositeOperation() != Animation::initialCompositeOperation()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationCompositeOperation(state, builder, context, animation.compositeOperation(), nullptr, nullptr);
        listEmpty = false;
    }
    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::None { });
}

inline void ExtractorSerializer::serializeSingleTransition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Animation& transition)
{
    static NeverDestroyed<Ref<TimingFunction>> initialTimingFunction(Animation::initialTimingFunction());

    // If we have a transition-delay but no transition-duration set, we must serialize
    // the transition-duration because they're both <time> values and transition-delay
    // comes first.
    auto showsDelay = transition.delay() != Animation::initialDelay();
    auto showsDuration = showsDelay || transition.duration() != Animation::initialDuration();

    bool listEmpty = true;

    if (transition.property() != Animation::initialProperty()) {
        serializeAnimationProperty(state, builder, context, transition.property(), nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDuration) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDuration(state, builder, context, transition.duration(), nullptr, nullptr);
        listEmpty = false;
    }
    if (RefPtr timingFunction = transition.timingFunction(); *timingFunction != initialTimingFunction.get()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationTimingFunction(state, builder, context, *timingFunction, nullptr, nullptr);
        listEmpty = false;
    }
    if (showsDelay) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationDelay(state, builder, context, transition.delay(), nullptr, nullptr);
        listEmpty = false;
    }
    if (transition.allowsDiscreteTransitions() != Animation::initialAllowsDiscreteTransitions()) {
        if (!listEmpty)
            builder.append(' ');
        serializeAnimationAllowsDiscreteTransitions(state, builder, context, transition.allowsDiscreteTransitions(), nullptr, nullptr);
        listEmpty = false;
    }

    if (listEmpty)
        serializationForCSS(builder, context, state.style, CSS::Keyword::All { });
}

// MARK: - Grid serializations

inline void ExtractorSerializer::serializeGridAutoFlow(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, GridAutoFlow gridAutoFlow)
{
    ASSERT(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionRow) || gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn));

    bool needsSpace = false;

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowDirectionColumn)) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Column { });
        needsSpace = true;
    } else if (!(gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense))) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Row { });
        needsSpace = true;
    }

    if (gridAutoFlow & static_cast<GridAutoFlow>(InternalAutoFlowAlgorithmDense)) {
        if (needsSpace)
            builder.append(' ');
        serializationForCSS(builder, context, state.style, CSS::Keyword::Dense { });
    }
}

inline void ExtractorSerializer::serializeGridPosition(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const GridPosition& position)
{
    if (position.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    if (position.isNamedGridArea()) {
        serializationForCSS(builder, context, state.style, CustomIdentifier { AtomString { position.namedGridLine() } });
        return;
    }

    bool hasNamedGridLine = !position.namedGridLine().isNull();

    if (position.isSpan()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Span { });

        if (!hasNamedGridLine || position.spanPosition() != 1) {
            builder.append(' ');
            serialize(state, builder, context, position.spanPosition());
        }
    } else
        serialize(state, builder, context, position.integerPosition());

    if (hasNamedGridLine) {
        builder.append(' ');
        serializationForCSS(builder, context, state.style, CustomIdentifier { AtomString { position.namedGridLine() } });
    }
}

inline void ExtractorSerializer::serializeGridTrackBreadth(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const GridLength& trackBreadth)
{
    if (!trackBreadth.isLength()) {
        CSS::serializationForCSS(builder, context, CSS::FlexRaw<> { CSS::FlexUnit::Fr, trackBreadth.flex() });
        return;
    }

    auto& trackBreadthLength = trackBreadth.length();
    if (trackBreadthLength.isAuto()) {
        serializationForCSS(builder, context, state.style, CSS::Keyword::Auto { });
        return;
    }

    serializeLength(state, builder, context, trackBreadthLength);
}

inline void ExtractorSerializer::serializeGridTrackSize(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const GridTrackSize& trackSize)
{
    switch (trackSize.type()) {
    case LengthTrackSizing:
        serializeGridTrackBreadth(state, builder, context, trackSize.minTrackBreadth());
        return;
    case FitContentTrackSizing:
        builder.append(nameLiteral(CSSValueFitContent), '(');
        serializeLength(state, builder, context, trackSize.fitContentTrackBreadth().length());
        builder.append(')');
        return;
    case MinMaxTrackSizing:
        if (trackSize.minTrackBreadth().isAuto() && trackSize.maxTrackBreadth().isFlex()) {
            CSS::serializationForCSS(builder, context, CSS::FlexRaw<> { CSS::FlexUnit::Fr, trackSize.maxTrackBreadth().flex() });
            return;
        }

        builder.append(nameLiteral(CSSValueMinmax), '(');
        serializeGridTrackBreadth(state, builder, context, trackSize.minTrackBreadth());
        builder.append(", "_s);
        serializeGridTrackBreadth(state, builder, context, trackSize.maxTrackBreadth());
        builder.append(')');
        return;
    }

    ASSERT_NOT_REACHED();
}

inline void ExtractorSerializer::serializeGridTrackSizeList(ExtractorState& state, StringBuilder& builder, const CSS::SerializationContext& context, const Vector<GridTrackSize>& gridTrackSizeList)
{
    builder.append(interleave(gridTrackSizeList, [&](auto& builder, auto& gridTrackSize) {
        serializeGridTrackSize(state, builder, context, gridTrackSize);
    }, ' '));
}

} // namespace Style
} // namespace WebCore
