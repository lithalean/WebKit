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

#pragma once

#include "CSSValue.h"
#include "CSSValueAggregates.h"
#include "ComputedStyleDependencies.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class CSSValuePool;
using CSSValueListBuilder = Vector<Ref<CSSValue>, 4>;

namespace CSS {

// MARK: - Serialization

// All leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::Serialize<CSSType> {
//        void operator()(StringBuilder&, const SerializationContext&, const CSSType&);
//    };

struct SerializationContext;

template<typename CSSType> struct Serialize;

// Serialization Invokers
template<typename CSSType> void serializationForCSS(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    Serialize<CSSType>{}(builder, context, value);
}

template<typename CSSType> [[nodiscard]] String serializationForCSS(const SerializationContext& context, const CSSType& value)
{
    StringBuilder builder;
    serializationForCSS(builder, context, value);
    return builder.toString();
}

template<typename CSSType> void serializationForCSSOnOptionalLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    if (!value)
        return;
    serializationForCSS(builder, context, *value);
}

template<typename CSSType> void serializationForCSSOnTupleLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(const std::optional<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, *element);
        },
        [&]<typename T>(const Markable<T>& element) {
            if (!element)
                return;
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, *element);
        },
        [&](const auto& element) {
            builder.append(std::exchange(swappedSeparator, separator));
            serializationForCSS(builder, context, element);
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename CSSType> void serializationForCSSOnRangeLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        builder.append(std::exchange(swappedSeparator, separator));
        serializationForCSS(builder, context, element);
    }
}

template<typename CSSType> void serializationForCSSOnVariantLike(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { serializationForCSS(builder, context, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder&, const SerializationContext&, const CSSType&)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnOptionalLike(builder, context, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnTupleLike(builder, context, value, SerializationSeparatorString<CSSType>);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnRangeLike(builder, context, value, SerializationSeparatorString<CSSType>);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct Serialize<CSSType> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const CSSType& value)
    {
        serializationForCSSOnVariantLike(builder, context, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct Serialize<Constant<C>> {
    void operator()(StringBuilder& builder, const SerializationContext&, const Constant<C>& value)
    {
        builder.append(nameLiteralForSerialization(value.value));
    }
};

// Specialization for `CustomIdentifier`.
template<> struct Serialize<CustomIdentifier> {
    void operator()(StringBuilder&, const SerializationContext&, const CustomIdentifier&);
};

// Specialization for `WTF::AtomString`.
template<> struct Serialize<WTF::AtomString> {
    void operator()(StringBuilder&, const SerializationContext&, const WTF::AtomString&);
};

// Specialization for `WTF::String`.
template<> struct Serialize<WTF::String> {
    void operator()(StringBuilder&, const SerializationContext&, const WTF::String&);
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct Serialize<FunctionNotation<Name, CSSType>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const FunctionNotation<Name, CSSType>& value)
    {
        builder.append(nameLiteralForSerialization(value.name), '(');
        serializationForCSS(builder, context, value.parameters);
        builder.append(')');
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename CSSType> struct Serialize<MinimallySerializingSpaceSeparatedSize<CSSType>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const MinimallySerializingSpaceSeparatedSize<CSSType>& value)
    {
        constexpr auto separator = SerializationSeparatorString<MinimallySerializingSpaceSeparatedSize<CSSType>>;

        if (get<0>(value) != get<1>(value)) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { get<0>(value), get<1>(value) }, separator);
            return;
        }
        serializationForCSS(builder, context, get<0>(value));
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedRectEdges`.
template<typename CSSType> struct Serialize<MinimallySerializingSpaceSeparatedRectEdges<CSSType>> {
    void operator()(StringBuilder& builder, const SerializationContext& context, const MinimallySerializingSpaceSeparatedRectEdges<CSSType>& value)
    {
        constexpr auto separator = SerializationSeparatorString<MinimallySerializingSpaceSeparatedRectEdges<CSSType>>;

        if (value.left() != value.right()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right(), value.bottom(), value.left() }, separator);
            return;
        }
        if (value.bottom() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right(), value.bottom() }, separator);
            return;
        }
        if (value.right() != value.top()) {
            serializationForCSSOnTupleLike(builder, context, std::tuple { value.top(), value.right() }, separator);
            return;
        }
        serializationForCSS(builder, context, value.top());
    }
};

// MARK: - Computed Style Dependencies

// What properties does this value rely on (eg, font-size for em units)?

// All non-tuple-like leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::ComputedStyleDependenciesCollector<CSSType> {
//        void operator()(ComputedStyleDependencies&, const CSSType&);
//    };

template<typename CSSType> struct ComputedStyleDependenciesCollector;

// ComputedStyleDependencies Invoker
template<typename CSSType> void collectComputedStyleDependencies(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    ComputedStyleDependenciesCollector<CSSType>{}(dependencies, value);
}

template<typename CSSType> [[nodiscard]] ComputedStyleDependencies collectComputedStyleDependencies(const CSSType& value)
{
    ComputedStyleDependencies dependencies;
    collectComputedStyleDependencies(dependencies, value);
    return dependencies;
}

template<typename CSSType> auto collectComputedStyleDependenciesOnOptionalLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    if (!value)
        return;
    collectComputedStyleDependencies(dependencies, *value);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnTupleLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    WTF::apply([&](const auto& ...x) { (..., collectComputedStyleDependencies(dependencies, x)); }, value);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnRangeLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    for (const auto& element : value)
        collectComputedStyleDependencies(dependencies, element);
}

template<typename CSSType> auto collectComputedStyleDependenciesOnVariantLike(ComputedStyleDependencies& dependencies, const CSSType& value)
{
    WTF::switchOn(value, [&](const auto& alternative) { collectComputedStyleDependencies(dependencies, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies&, const CSSType&)
    {
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnOptionalLike(dependencies, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnTupleLike(dependencies, value);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnRangeLike(dependencies, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct ComputedStyleDependenciesCollector<CSSType> {
    void operator()(ComputedStyleDependencies& dependencies, const CSSType& value)
    {
        collectComputedStyleDependenciesOnVariantLike(dependencies, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct ComputedStyleDependenciesCollector<Constant<C>> {
    constexpr void operator()(ComputedStyleDependencies&, const Constant<C>&)
    {
        // Nothing to do.
    }
};

// Specialization for `CustomIdentifier`.
template<> struct ComputedStyleDependenciesCollector<CustomIdentifier> {
    constexpr void operator()(ComputedStyleDependencies&, const CustomIdentifier&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::AtomString`.
template<> struct ComputedStyleDependenciesCollector<WTF::AtomString> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::AtomString&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::String`.
template<> struct ComputedStyleDependenciesCollector<WTF::String> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::String&)
    {
        // Nothing to do.
    }
};

// Specialization for `WTF::URL`.
template<> struct ComputedStyleDependenciesCollector<WTF::URL> {
    constexpr void operator()(ComputedStyleDependencies&, const WTF::URL&)
    {
        // Nothing to do.
    }
};

// MARK: - CSSValue Visitation

// All non-tuple-like leaf types must implement the following conversions:
//
//    template<> struct WebCore::CSS::CSSValueChildrenVisitor<CSSType> {
//        IterationStatus operator()(const Function<IterationStatus(CSSValue&)>&, const CSSType&);
//    };

template<typename CSSType> struct CSSValueChildrenVisitor;

// CSSValueVisitor Invoker
template<typename CSSType> IterationStatus visitCSSValueChildren(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return CSSValueChildrenVisitor<CSSType>{}(func, value);
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnOptionalLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return value ? visitCSSValueChildren(func, *value) : IterationStatus::Continue;
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnTupleLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    // Process a single element of the tuple-like, updating result, and return true if result == IterationStatus::Done to
    // short circuit the fold in the apply lambda.
    auto process = [&](const auto& x, IterationStatus& result) -> bool {
        result = visitCSSValueChildren(func, x);
        return result == IterationStatus::Done;
    };

    return WTF::apply([&](const auto& ...x) {
        auto result = IterationStatus::Continue;
        (process(x, result) || ...);
        return result;
    }, value);
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnRangeLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    for (const auto& element : value) {
        if (visitCSSValueChildren(func, element) == IterationStatus::Done)
            return IterationStatus::Done;
    }
    return IterationStatus::Continue;
}

template<typename CSSType> IterationStatus visitCSSValueChildrenOnVariantLike(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
{
    return WTF::switchOn(value, [&](const auto& alternative) { return visitCSSValueChildren(func, alternative); });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CSSType&)
    {
        return IterationStatus::Continue;
    }
};

// Constrained for `TreatAsOptionalLike`.
template<OptionalLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnOptionalLike(func, value);
    }
};

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnTupleLike(func, value);
    }
};

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnRangeLike(func, value);
    }
};

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct CSSValueChildrenVisitor<CSSType> {
    IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>& func, const CSSType& value)
    {
        return visitCSSValueChildrenOnVariantLike(func, value);
    }
};

// Specialization for `Constant`.
template<CSSValueID C> struct CSSValueChildrenVisitor<Constant<C>> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const Constant<C>&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueChildrenVisitor<CustomIdentifier> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const CustomIdentifier&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::AtomString`.
template<> struct CSSValueChildrenVisitor<WTF::AtomString> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::AtomString&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::String`.
template<> struct CSSValueChildrenVisitor<WTF::String> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::String&)
    {
        return IterationStatus::Continue;
    }
};

// Specialization for `WTF::URL`.
template<> struct CSSValueChildrenVisitor<WTF::URL> {
    constexpr IterationStatus operator()(NOESCAPE const Function<IterationStatus(CSSValue&)>&, const WTF::URL&)
    {
        return IterationStatus::Continue;
    }
};

// MARK: - CSSValue Creation

template<typename CSSType> struct CSSValueCreation;

template<typename CSSType> Ref<CSSValue> createCSSValue(CSSValuePool& pool, const CSSType& value)
{
    return CSSValueCreation<CSSType>{}(pool, value);
}

Ref<CSSValue> makePrimitiveCSSValue(CSSValueID);
Ref<CSSValue> makePrimitiveCSSValue(const CustomIdentifier&);
Ref<CSSValue> makePrimitiveCSSValue(const WTF::AtomString&);
Ref<CSSValue> makePrimitiveCSSValue(const WTF::String&);
Ref<CSSValue> makeFunctionCSSValue(CSSValueID, Ref<CSSValue>&&);
Ref<CSSValue> makeSpaceSeparatedCoalescingPairCSSValue(Ref<CSSValue>&&, Ref<CSSValue>&&);
template<SerializationSeparatorType> Ref<CSSValue> makeListCSSValue(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Space>(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Comma>(CSSValueListBuilder&&);
template<> Ref<CSSValue> makeListCSSValue<SerializationSeparatorType::Slash>(CSSValueListBuilder&&);

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> struct CSSValueCreation<CSSType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value)
    {
        return WTF::switchOn(value, [&](const auto& alternative) { return createCSSValue(pool, alternative); });
    }
};

// Constrained for `TreatAsTupleLike`
template<TupleLike CSSType> struct CSSValueCreation<CSSType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value)
    {
        if constexpr (std::tuple_size_v<CSSType> == 1 && SerializationSeparator<CSSType> == SerializationSeparatorType::None) {
            return createCSSValue(pool, get<0>(value));
        } else {
            CSSValueListBuilder list;

            auto caller = WTF::makeVisitor(
                [&]<typename T>(const std::optional<T>& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, *element));
                },
                [&]<typename T>(const Markable<T>& element) {
                    if (!element)
                        return;
                    list.append(createCSSValue(pool, *element));
                },
                [&](const auto& element) {
                    list.append(createCSSValue(pool, element));
                }
            );
            WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);

            return makeListCSSValue<SerializationSeparator<CSSType>>(WTFMove(list));
        }
    }
};

// Constrained for `TreatAsRangeLike`
template<RangeLike CSSType> struct CSSValueCreation<CSSType> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const CSSType& value)
    {
        CSSValueListBuilder list;
        for (const auto& element : value)
            list.append(createCSSValue(pool, element));

        return makeListCSSValue<SerializationSeparator<CSSType>>(WTFMove(list));
    }
};

// Specialization for `Constant`.
template<CSSValueID Id> struct CSSValueCreation<Constant<Id>> {
    Ref<CSSValue> operator()(CSSValuePool&, const Constant<Id>&)
    {
        return makePrimitiveCSSValue(Id);
    }
};

// Specialization for `CustomIdentifier`.
template<> struct CSSValueCreation<CustomIdentifier> {
    Ref<CSSValue> operator()(CSSValuePool&, const CustomIdentifier& customIdentifier)
    {
        return makePrimitiveCSSValue(customIdentifier);
    }
};

// Specialization for `WTF::AtomString`.
template<> struct CSSValueCreation<WTF::AtomString> {
    Ref<CSSValue> operator()(CSSValuePool&, const WTF::AtomString& string)
    {
        return makePrimitiveCSSValue(string);
    }
};

// Specialization for `WTF::String`.
template<> struct CSSValueCreation<WTF::String> {
    Ref<CSSValue> operator()(CSSValuePool&, const WTF::String& string)
    {
        return makePrimitiveCSSValue(string);
    }
};

// Specialization for `FunctionNotation`.
template<CSSValueID Name, typename CSSType> struct CSSValueCreation<FunctionNotation<Name, CSSType>> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const FunctionNotation<Name, CSSType>& value)
    {
        return makeFunctionCSSValue(value.name, createCSSValue(pool, value.parameters));
    }
};

// Specialization for `MinimallySerializingSpaceSeparatedSize`.
template<typename CSSType> struct CSSValueCreation<MinimallySerializingSpaceSeparatedSize<CSSType>> {
    Ref<CSSValue> operator()(CSSValuePool& pool, const MinimallySerializingSpaceSeparatedSize<CSSType>& value)
    {
        return makeSpaceSeparatedCoalescingPairCSSValue(createCSSValue(pool, get<0>(value)), createCSSValue(pool, get<1>(value)));
    }
};

// MARK: - Logging

template<typename CSSType> void logForCSSOnTupleLike(TextStream& ts, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    auto caller = WTF::makeVisitor(
        [&]<typename T>(const std::optional<T>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&]<typename T>(const Markable<T>& element) {
            if (!element)
                return;
            ts << std::exchange(swappedSeparator, separator);
            ts << *element;
        },
        [&](const auto& element) {
            ts << std::exchange(swappedSeparator, separator);
            ts << element;
        }
    );

    WTF::apply([&](const auto& ...x) { (..., caller(x)); }, value);
}

template<typename CSSType> void logForCSSOnRangeLike(TextStream& ts, const CSSType& value, ASCIILiteral separator)
{
    auto swappedSeparator = ""_s;
    for (const auto& element : value) {
        ts << std::exchange(swappedSeparator, separator);
        ts << element;
    }
}

template<typename CSSType> void logForCSSOnVariantLike(TextStream& ts, const CSSType& value)
{
    WTF::switchOn(value, [&](const auto& value) { ts << value; });
}

// Constrained for `TreatAsEmptyLike`.
template<EmptyLike CSSType> TextStream& operator<<(TextStream& ts, const CSSType&)
{
    return ts;
}

// Constrained for `TreatAsTupleLike`.
template<TupleLike CSSType> TextStream& operator<<(TextStream& ts, const CSSType& value)
{
    logForCSSOnTupleLike(ts, value, SerializationSeparator<CSSType>);
    return ts;
}

// Constrained for `TreatAsRangeLike`.
template<RangeLike CSSType> TextStream& operator<<(TextStream& ts, const CSSType& value)
{
    logForCSSOnRangeLike(ts, value, SerializationSeparator<CSSType>);
    return ts;
}

// Constrained for `TreatAsVariantLike`.
template<VariantLike CSSType> TextStream& operator<<(TextStream& ts, const CSSType& value)
{
    logForCSSOnVariantLike(ts, value);
    return ts;
}

} // namespace CSS
} // namespace WebCore
