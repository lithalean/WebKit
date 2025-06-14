/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
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

#include "BlockEllipsis.h"
#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "CalculationValue.h"
#include "ColorBlending.h"
#include "ContentData.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontCascade.h"
#include "FontSelectionAlgorithm.h"
#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "GridPositionsResolver.h"
#include "IdentityTransformOperation.h"
#include "LengthPoint.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PathOperation.h"
#include "QuotesData.h"
#include "RenderBox.h"
#include "RenderStyleSetters.h"
#include "RotateTransformOperation.h"
#include "SVGRenderStyle.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollbarGutter.h"
#include "Settings.h"
#include "StyleBoxShadow.h"
#include "StyleCachedImage.h"
#include "StyleCrossfadeImage.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleFilterImage.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StyleResolver.h"
#include "StyleTextEdge.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>

namespace WebCore::Style::Interpolation {

inline int blendFunc(int from, int to, const Context& context)
{
    return blend(from, to, context);
}

inline double blendFunc(double from, double to, const Context& context)
{
    return blend(from, to, context);
}

inline float blendFunc(float from, float to, const Context& context)
{
    if (context.iterationCompositeOperation == IterationCompositeOperation::Accumulate && context.currentIteration) {
        auto iterationIncrement = context.currentIteration * to;
        from += iterationIncrement;
        to += iterationIncrement;
    }

    if (context.compositeOperation == CompositeOperation::Replace)
        return narrowPrecisionToFloat(from + (to - from) * context.progress);
    return narrowPrecisionToFloat(from + from + (to - from) * context.progress);
}

inline WebCore::Color blendFunc(const WebCore::Color& from, const WebCore::Color& to, const Context& context)
{
    return blend(from, to, context);
}

inline WebCore::Length blendFunc(const WebCore::Length& from, const WebCore::Length& to, const Context& context, ValueRange valueRange = ValueRange::All)
{
    return blend(from, to, context, valueRange);
}

inline GapLength blendFunc(const GapLength& from, const GapLength& to, const Context& context)
{
    if (from.isNormal() || to.isNormal())
        return context.progress < 0.5 ? from : to;
    return blend(from.length(), to.length(), context, ValueRange::NonNegative);
}

inline bool canInterpolateLengthVariants(const GapLength& from, const GapLength& to)
{
    if (from.isNormal() || to.isNormal())
        return false;
    bool isLengthPercentage = true;
    return canInterpolateLengths(from.length(), to.length(), isLengthPercentage);
}

inline bool lengthVariantRequiresInterpolationForAccumulativeIteration(const GapLength& from, const GapLength& to)
{
    return from.isNormal() || to.isNormal() || lengthsRequireInterpolationForAccumulativeIteration(from.length(), to.length());
}

inline TabSize blendFunc(const TabSize& from, const TabSize& to, const Context& context)
{
    auto blendedValue = blend(from.value(), to.value(), context);
    return { blendedValue < 0 ? 0 : blendedValue, from.isSpaces() ? SpaceValueType : LengthValueType };
}

inline LengthSize blendFunc(const LengthSize& from, const LengthSize& to, const Context& context)
{
    return blend(from, to, context, ValueRange::NonNegative);
}

inline bool canInterpolateLengthVariants(const LengthSize& from, const LengthSize& to)
{
    bool isLengthPercentage = true;
    return canInterpolateLengths(from.width, to.width, isLengthPercentage)
        && canInterpolateLengths(from.height, to.height, isLengthPercentage);
}

inline bool lengthVariantRequiresInterpolationForAccumulativeIteration(const LengthSize& from, const LengthSize& to)
{
    return lengthsRequireInterpolationForAccumulativeIteration(from.width, to.width)
        || lengthsRequireInterpolationForAccumulativeIteration(from.height, to.height);
}

inline LengthPoint blendFunc(const LengthPoint& from, const LengthPoint& to, const Context& context)
{
    return blend(from, to, context);
}

inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, const Context& context)
{
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        Vector<Ref<TransformOperation>> operations;
        operations.reserveInitialCapacity(from.size() + to.size());

        operations.appendRange(from.begin(), from.end());
        operations.appendRange(to.begin(), to.end());

        return TransformOperations { WTFMove(operations) };
    }

    auto prefix = [&]() -> std::optional<unsigned> {
        // We cannot use the pre-computed prefix when dealing with accumulation
        // since the values used to accumulate may be different than those held
        // in the initial keyframe list. We must do the same with any property
        // other than "transform" since we only pre-compute the prefix for that
        // property.
        if (context.compositeOperation == CompositeOperation::Accumulate || std::holds_alternative<AtomString>(context.property) || std::get<CSSPropertyID>(context.property) != CSSPropertyTransform)
            return std::nullopt;
        return context.client.transformFunctionListPrefix();
    };

    auto* renderBox = dynamicDowncast<RenderBox>(context.client.renderer());
    auto boxSize = renderBox ? renderBox->borderBoxRect().size() : LayoutSize();
    return to.blend(from, context, boxSize, prefix());
}

inline RefPtr<ScaleTransformOperation> blendFunc(ScaleTransformOperation* from, ScaleTransformOperation* to, const Context& context)
{
    if (!from && !to)
        return nullptr;

    RefPtr<ScaleTransformOperation> identity;
    if (!from) {
        identity = ScaleTransformOperation::create(1, 1, 1, to->type());
        from = identity.get();
    } else if (!to) {
        identity = ScaleTransformOperation::create(1, 1, 1, from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<ScaleTransformOperation> normalizedFrom;
        RefPtr<ScaleTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::Type::Scale3D);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::Type::Scale3D);
        } else {
            normalizedFrom = ScaleTransformOperation::create(from->x(), from->y(), TransformOperation::Type::Scale);
            normalizedTo = ScaleTransformOperation::create(to->x(), to->y(), TransformOperation::Type::Scale);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (auto* scale = dynamicDowncast<ScaleTransformOperation>(blendedOperation.get()))
        return ScaleTransformOperation::create(scale->x(), scale->y(), scale->z(), scale->type());
    return nullptr;
}

inline RefPtr<RotateTransformOperation> blendFunc(RotateTransformOperation* from, RotateTransformOperation* to, const Context& context)
{
    if (!from && !to)
        return nullptr;

    RefPtr<RotateTransformOperation> identity;
    if (!from) {
        identity = RotateTransformOperation::create(0, to->type());
        from = identity.get();
    } else if (!to) {
        identity = RotateTransformOperation::create(0, from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<RotateTransformOperation> normalizedFrom;
        RefPtr<RotateTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = RotateTransformOperation::create(from->x(), from->y(), from->z(), from->angle(), TransformOperation::Type::Rotate3D);
            normalizedTo = RotateTransformOperation::create(to->x(), to->y(), to->z(), to->angle(), TransformOperation::Type::Rotate3D);
        } else {
            normalizedFrom = RotateTransformOperation::create(from->angle(), TransformOperation::Type::Rotate);
            normalizedTo = RotateTransformOperation::create(to->angle(), TransformOperation::Type::Rotate);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (auto* rotate = dynamicDowncast<RotateTransformOperation>(blendedOperation.get()))
        return RotateTransformOperation::create(rotate->x(), rotate->y(), rotate->z(), rotate->angle(), rotate->type());
    return nullptr;
}

inline RefPtr<TranslateTransformOperation> blendFunc(TranslateTransformOperation* from, TranslateTransformOperation* to, const Context& context)
{
    if (!from && !to)
        return nullptr;

    RefPtr<TranslateTransformOperation> identity;
    if (!from) {
        identity = TranslateTransformOperation::create(WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), to->type());
        from = identity.get();
    } else if (!to) {
        identity = TranslateTransformOperation::create(WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), WebCore::Length(0, LengthType::Fixed), from->type());
        to = identity.get();
    }

    // Ensure the two transforms have the same type.
    if (!from->isSameType(*to)) {
        RefPtr<TranslateTransformOperation> normalizedFrom;
        RefPtr<TranslateTransformOperation> normalizedTo;
        if (from->is3DOperation() || to->is3DOperation()) {
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), from->z(), TransformOperation::Type::Translate3D);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), to->z(), TransformOperation::Type::Translate3D);
        } else {
            normalizedFrom = TranslateTransformOperation::create(from->x(), from->y(), TransformOperation::Type::Translate);
            normalizedTo = TranslateTransformOperation::create(to->x(), to->y(), TransformOperation::Type::Translate);
        }
        return blendFunc(normalizedFrom.get(), normalizedTo.get(), context);
    }

    auto blendedOperation = to->blend(from, context);
    if (auto* translate = dynamicDowncast<TranslateTransformOperation>(blendedOperation.get()))
        return TranslateTransformOperation::create(translate->x(), translate->y(), translate->z(), translate->type());
    return nullptr;
}

inline Ref<TransformOperation> blendFunc(TransformOperation& from, TransformOperation& to, const Context& context)
{
    return to.blend(&from, context);
}

inline RefPtr<PathOperation> blendFunc(PathOperation* from, PathOperation* to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

    ASSERT(from);
    ASSERT(to);
    return from->blend(to, context);
}

inline RefPtr<ShapeValue> blendFunc(ShapeValue* from, ShapeValue* to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1);
        return context.progress ? to : from;
    }

    ASSERT(from);
    ASSERT(to);
    return from->blend(*to, context);
}

inline FilterOperations blendFunc(const FilterOperations& from, const FilterOperations& to, const Context& context)
{
    return from.blend(to, context);
}

inline RefPtr<StyleImage> blendFilter(RefPtr<StyleImage> inputImage, const FilterOperations& from, const FilterOperations& to, const Context& context)
{
    auto filterResult = from.blend(to, context);
    return StyleFilterImage::create(WTFMove(inputImage), WTFMove(filterResult));
}

inline ContentVisibility blendFunc(ContentVisibility from, ContentVisibility to, const Context& context)
{
    // https://drafts.csswg.org/css-contain-3/#content-visibility-animation
    // In general, the content-visibility property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between hidden and any other content-visibility value, p values between 0 and 1
    // map to the non-hidden value.
    if (from != ContentVisibility::Hidden && to != ContentVisibility::Hidden)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == ContentVisibility::Hidden ? to : from;
}

inline Visibility blendFunc(Visibility from, Visibility to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Visibility::Collapse) depends on the specified from/to values.
    double fromVal = from == Visibility::Visible ? 1. : 0.;
    double toVal = to == Visibility::Visible ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    // The composite operation here is irrelevant.
    double result = blendFunc(fromVal, toVal, { context.property, context.progress, false, CompositeOperation::Replace, IterationCompositeOperation::Replace, 0, { }, { }, context.client });
    return result > 0. ? Visibility::Visible : (to != Visibility::Visible ? to : from);
}

inline DisplayType blendFunc(DisplayType from, DisplayType to, const Context& context)
{
    // https://drafts.csswg.org/css-display-4/#display-animation
    // In general, the display property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between none and any other display value, p values between 0 and 1
    // map to the non-none value. Additionally, the element is inert as long as its display value would
    // compute to none when ignoring the Transitions and Animations cascade origins.
    if (from != DisplayType::None && to != DisplayType::None)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == DisplayType::None ? to : from;
}

inline LengthBox blendFunc(const LengthBox& from, const LengthBox& to, const Context& context, ValueRange valueRange = ValueRange::NonNegative)
{
    return LengthBox {
        blendFunc(from.top(), to.top(), context, valueRange),
        blendFunc(from.right(), to.right(), context, valueRange),
        blendFunc(from.bottom(), to.bottom(), context, valueRange),
        blendFunc(from.left(), to.left(), context, valueRange),
    };
}

inline FixedVector<WebCore::Length> blendFunc(const FixedVector<WebCore::Length>& from, const FixedVector<WebCore::Length>& to, const Context& context)
{
    size_t fromLength = from.size();
    size_t toLength = to.size();
    if (!fromLength || !toLength)
        return context.progress < 0.5 ? from : to;

    size_t resultLength = fromLength;
    if (fromLength != toLength) {
        if (!remainder(std::max(fromLength, toLength), std::min(fromLength, toLength)))
            resultLength = std::max(fromLength, toLength);
        else
            resultLength = fromLength * toLength;
    }
    return FixedVector<WebCore::Length>::createWithSizeFromGenerator(resultLength, [&](auto i) {
        return blendFunc(from[i % fromLength], to[i % toLength], context);
    });
}

inline RefPtr<StyleImage> crossfadeBlend(StyleCachedImage& fromStyleImage, StyleCachedImage& toStyleImage, const Context& context)
{
    // If progress is at one of the extremes, we want getComputedStyle to show the image,
    // not a completed cross-fade, so we hand back one of the existing images.
    if (!context.progress)
        return &fromStyleImage;
    if (context.progress == 1)
        return &toStyleImage;
    if (!fromStyleImage.cachedImage() || !toStyleImage.cachedImage())
        return &toStyleImage;
    return StyleCrossfadeImage::create(&fromStyleImage, &toStyleImage, context.progress, false);
}

inline RefPtr<StyleImage> blendFunc(StyleImage* from, StyleImage* to, const Context& context)
{
    if (!context.progress)
        return from;

    if (context.progress == 1.0)
        return to;

    ASSERT(from);
    ASSERT(to);

    from = from->selectedImage();
    to = to->selectedImage();

    if (!from || !to)
        return to;

    // Animation between two generated images. Cross fade for all other cases.
    if (auto [fromFilter, toFilter] = std::tuple { dynamicDowncast<StyleFilterImage>(*from), dynamicDowncast<StyleFilterImage>(*to) }; fromFilter && toFilter) {
        // Animation of generated images just possible if input images are equal.
        // Otherwise fall back to cross fade animation.
        if (fromFilter->equalInputImages(*toFilter) && is<StyleCachedImage>(fromFilter->inputImage()))
            return blendFilter(fromFilter->inputImage(), fromFilter->filterOperations(), toFilter->filterOperations(), context);
    } else if (auto [fromCrossfade, toCrossfade] = std::tuple { dynamicDowncast<StyleCrossfadeImage>(*from), dynamicDowncast<StyleCrossfadeImage>(*to) }; fromCrossfade && toCrossfade) {
        if (fromCrossfade->equalInputImages(*toCrossfade)) {
            if (auto crossfadeBlend = toCrossfade->blend(*fromCrossfade, context))
                return crossfadeBlend;
        }
    } else if (auto [fromFilter, toCachedImage] = std::tuple { dynamicDowncast<StyleFilterImage>(*from), dynamicDowncast<StyleCachedImage>(*to) }; fromFilter && toCachedImage) {
        RefPtr fromFilterInputImage = dynamicDowncast<StyleCachedImage>(fromFilter->inputImage());

        if (fromFilterInputImage && toCachedImage->equals(*fromFilterInputImage))
            return blendFilter(WTFMove(fromFilterInputImage), fromFilter->filterOperations(), FilterOperations(), context);
    } else if (auto [fromCachedImage, toFilter] = std::tuple { dynamicDowncast<StyleCachedImage>(*from), dynamicDowncast<StyleFilterImage>(*to) }; fromCachedImage && toFilter) {
        RefPtr toFilterInputImage = dynamicDowncast<StyleCachedImage>(toFilter->inputImage());

        if (toFilterInputImage && fromCachedImage->equals(*toFilterInputImage))
            return blendFilter(WTFMove(toFilterInputImage), FilterOperations(), toFilter->filterOperations(), context);
    }

    auto* fromCachedImage = dynamicDowncast<StyleCachedImage>(*from);
    auto* toCachedImage = dynamicDowncast<StyleCachedImage>(*to);
    if (fromCachedImage && toCachedImage)
        return crossfadeBlend(*fromCachedImage, *toCachedImage, context);

    // FIXME: Add support for animation between two *gradient() functions.
    // https://bugs.webkit.org/show_bug.cgi?id=119956

    // FIXME: Add support cross fade between cached and generated images.
    // https://bugs.webkit.org/show_bug.cgi?id=78293

    return to;
}

inline NinePieceImage blendFunc(const NinePieceImage& from, const NinePieceImage& to, const Context& context)
{
    if (!from.hasImage() || !to.hasImage())
        return to;

    // FIXME (74112): Support transitioning between NinePieceImages that differ by more than image content.

    if (from.imageSlices() != to.imageSlices() || from.borderSlices() != to.borderSlices() || from.outset() != to.outset() || from.fill() != to.fill() || from.overridesBorderWidths() != to.overridesBorderWidths() || from.horizontalRule() != to.horizontalRule() || from.verticalRule() != to.verticalRule())
        return to;

    if (auto* renderer = context.client.renderer()) {
        if (from.image()->imageSize(renderer, 1.0) != to.image()->imageSize(renderer, 1.0))
            return to;
    }

    return NinePieceImage(blendFunc(from.image(), to.image(), context),
        from.imageSlices(), from.fill(), from.borderSlices(), from.overridesBorderWidths(), from.outset(), from.horizontalRule(), from.verticalRule());
}

#if ENABLE(VARIATION_FONTS)

inline FontVariationSettings blendFunc(const FontVariationSettings& from, const FontVariationSettings& to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    ASSERT(from.size() == to.size());
    FontVariationSettings result;
    size_t size = from.size();
    for (size_t i = 0; i < size; ++i) {
        auto& fromItem = from.at(i);
        auto& toItem = to.at(i);
        ASSERT(fromItem.tag() == toItem.tag());
        result.insert({ fromItem.tag(), blendFunc(fromItem.value(), toItem.value(), context) });
    }
    return result;
}

#endif

inline FontSelectionValue blendFunc(FontSelectionValue from, FontSelectionValue to, const Context& context)
{
    return FontSelectionValue(std::max(0.0f, blendFunc(static_cast<float>(from), static_cast<float>(to), context)));
}

inline std::optional<FontSelectionValue> blendFunc(std::optional<FontSelectionValue> from, std::optional<FontSelectionValue> to, const Context& context)
{
    if (!from && !to)
        return std::nullopt;

    auto valueOrDefault = [](std::optional<FontSelectionValue> fontSelectionValue) {
        if (!fontSelectionValue)
            return 0.0f;
        return static_cast<float>(fontSelectionValue.value());
    };

    return normalizedFontItalicValue(blendFunc(valueOrDefault(from), valueOrDefault(to), context));
}

inline bool canInterpolate(const GridTrackList& from, const GridTrackList& to)
{
    if (from.list.size() != to.list.size())
        return false;

    size_t i = 0;
    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize&) {
            return std::holds_alternative<GridTrackSize>(to.list[i]);
        },
        [&](const Vector<String>&) {
            return std::holds_alternative<Vector<String>>(to.list[i]);
        },
        [&](const GridTrackEntryRepeat& repeat) {
            if (!std::holds_alternative<GridTrackEntryRepeat>(to.list[i]))
                return false;
            auto& toEntry = std::get<GridTrackEntryRepeat>(to.list[i]);
            return repeat.repeats == toEntry.repeats && repeat.list.size() == toEntry.list.size();
        },
        [](const GridTrackEntryAutoRepeat&) {
            return false;
        },
        [](const GridTrackEntrySubgrid&) {
            return false;
        },
        [](const GridTrackEntryMasonry&) {
            return false;
        }
    );

    for (i = 0; i < from.list.size(); i++) {
        if (!WTF::visit(visitor, from.list[i]))
            return false;
    }

    return true;
}

inline GridLength blendFunc(const GridLength& from, const GridLength& to, const Context& context)
{
    if (from.isFlex() != to.isFlex())
        return context.progress < 0.5 ? from : to;

    if (from.isFlex())
        return GridLength(blend(from.flex(), to.flex(), context));

    return GridLength(blendFunc(from.length(), to.length(), context));
}

inline GridTrackSize blendFunc(const GridTrackSize& from, const GridTrackSize& to, const Context& context)
{
    if (from.type() != to.type())
        return context.progress < 0.5 ? from : to;

    if (from.type() == LengthTrackSizing) {
        auto length = blendFunc(from.minTrackBreadth(), to.minTrackBreadth(), context);
        return GridTrackSize(length, LengthTrackSizing);
    }
    if (from.type() == MinMaxTrackSizing) {
        auto minTrackBreadth = blendFunc(from.minTrackBreadth(), to.minTrackBreadth(), context);
        auto maxTrackBreadth = blendFunc(from.maxTrackBreadth(), to.maxTrackBreadth(), context);
        return GridTrackSize(minTrackBreadth, maxTrackBreadth);
    }

    auto fitContentBreadth = blendFunc(from.fitContentTrackBreadth(), to.fitContentTrackBreadth(), context);
    return GridTrackSize(fitContentBreadth, FitContentTrackSizing);
}

inline RepeatTrackList blendFunc(const RepeatTrackList& from, const RepeatTrackList& to, const Context& context)
{
    RepeatTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize& size) {
            result.append(blendFunc(size, std::get<GridTrackSize>(to[i]), context));
        },
        [&](const Vector<String>& names) {
            if (context.progress < 0.5)
                result.append(names);
            else
                result.append(std::get<Vector<String>>(to[i]));
        }
    );

    for (i = 0; i < from.size(); i++)
        WTF::visit(visitor, from[i]);

    return result;
}

inline GridTrackList blendFunc(const GridTrackList& from, const GridTrackList& to, const Context& context)
{
    if (!canInterpolate(from, to))
        return context.progress < 0.5 ? from : to;

    GridTrackList result;
    size_t i = 0;

    auto visitor = WTF::makeVisitor(
        [&](const GridTrackSize& size) {
            result.list.append(blendFunc(size, std::get<GridTrackSize>(to.list[i]), context));
        },
        [&](const Vector<String>& names) {
            if (context.progress < 0.5)
                result.list.append(names);
            else
                result.list.append(std::get<Vector<String>>(to.list[i]));
        },
        [&](const GridTrackEntryRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryRepeat>(to.list[i]);
            GridTrackEntryRepeat repeatResult;
            repeatResult.repeats = repeatFrom.repeats;
            repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
            result.list.append(WTFMove(repeatResult));
        },
        [&](const GridTrackEntryAutoRepeat& repeatFrom) {
            auto& repeatTo = std::get<GridTrackEntryAutoRepeat>(to.list[i]);
            GridTrackEntryAutoRepeat repeatResult;
            repeatResult.type = repeatFrom.type;
            repeatResult.list = blendFunc(repeatFrom.list, repeatTo.list, context);
            result.list.append(WTFMove(repeatResult));
        },
        [](const GridTrackEntrySubgrid&) {
        },
        [](const GridTrackEntryMasonry&) {
        }
    );


    for (i = 0; i < from.list.size(); i++)
        WTF::visit(visitor, from.list[i]);

    return result;
}

inline RefPtr<StylePathData> blendFunc(StylePathData* from, StylePathData* to, const Context& context)
{
    if (context.isDiscrete)
        return context.progress < 0.5 ? from : to;
    ASSERT(from);
    ASSERT(to);
    return from->blend(*to, context);
}

} // namespace WebCore::Style::Interpolation
