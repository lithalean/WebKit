/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "IDBKeyData.h"

#include "KeyedCoding.h"
#include <algorithm>
#include <wtf/CrossThreadCopier.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IDBKeyData);

IDBKeyData::IDBKeyData(const IDBKey* key)
{
    if (!key)
        return;

    switch (key->type()) {
    case IndexedDB::KeyType::Invalid:
        m_value = Invalid { };
        break;
    case IndexedDB::KeyType::Array: {
        m_value = Vector<IDBKeyData>();
        auto& array = std::get<Vector<IDBKeyData>>(m_value);
        for (auto& key2 : key->array())
            array.append(IDBKeyData(key2.get()));
        break;
    }
    case IndexedDB::KeyType::Binary:
        m_value = key->binary();
        break;
    case IndexedDB::KeyType::String:
        m_value = key->string();
        break;
    case IndexedDB::KeyType::Date:
        m_value = Date { key->date() };
        break;
    case IndexedDB::KeyType::Number:
        m_value = key->number();
        break;
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        break;
    }
}

IDBKeyData::IDBKeyData(const IDBKeyData& data, IsolatedCopyTag)
    : m_isDeletedValue(data.m_isDeletedValue)
    , m_isPlaceholder(data.m_isPlaceholder)
    , m_value(crossThreadCopy(data.m_value))
{
}

IndexedDB::KeyType IDBKeyData::type() const
{
    switch (m_value.index()) {
    case WTF::alternativeIndexV<std::nullptr_t, ValueVariant>:
    case WTF::alternativeIndexV<Invalid, ValueVariant>:
        return IndexedDB::KeyType::Invalid;
    case WTF::alternativeIndexV<Vector<IDBKeyData>, ValueVariant>:
        return IndexedDB::KeyType::Array;
    case WTF::alternativeIndexV<String, ValueVariant>:
        return IndexedDB::KeyType::String;
    case WTF::alternativeIndexV<double, ValueVariant>:
        return IndexedDB::KeyType::Number;
    case WTF::alternativeIndexV<Date, ValueVariant>:
        return IndexedDB::KeyType::Date;
    case WTF::alternativeIndexV<ThreadSafeDataBuffer, ValueVariant>:
        return IndexedDB::KeyType::Binary;
    case WTF::alternativeIndexV<Min, ValueVariant>:
        return IndexedDB::KeyType::Min;
    case WTF::alternativeIndexV<Max, ValueVariant>:
        return IndexedDB::KeyType::Max;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

RefPtr<IDBKey> IDBKeyData::maybeCreateIDBKey() const
{
    if (isNull())
        return nullptr;

    switch (type()) {
    case IndexedDB::KeyType::Invalid:
        return IDBKey::createInvalid();
    case IndexedDB::KeyType::Array: {
        Vector<RefPtr<IDBKey>> array;
        for (auto& keyData : std::get<Vector<IDBKeyData>>(m_value)) {
            array.append(keyData.maybeCreateIDBKey());
            ASSERT(array.last());
        }
        return IDBKey::createArray(array);
    }
    case IndexedDB::KeyType::Binary:
        return IDBKey::createBinary(std::get<ThreadSafeDataBuffer>(m_value));
    case IndexedDB::KeyType::String:
        return IDBKey::createString(std::get<String>(m_value));
    case IndexedDB::KeyType::Date:
        return IDBKey::createDate(std::get<Date>(m_value).value);
    case IndexedDB::KeyType::Number:
        return IDBKey::createNumber(std::get<double>(m_value));
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

IDBKeyData IDBKeyData::isolatedCopy() const
{
    return { *this, IsolatedCopy };
}

void IDBKeyData::encode(KeyedEncoder& encoder) const
{
    encoder.encodeBool("null"_s, isNull());
    if (isNull())
        return;

    encoder.encodeEnum("type"_s, type());

    switch (type()) {
    case IndexedDB::KeyType::Invalid:
        return;
    case IndexedDB::KeyType::Array: {
        auto& array = std::get<Vector<IDBKeyData>>(m_value);
        encoder.encodeObjects("array"_s, array, [](KeyedEncoder& encoder, const IDBKeyData& key) {
            key.encode(encoder);
        });
        return;
    }
    case IndexedDB::KeyType::Binary: {
        auto* data = std::get<ThreadSafeDataBuffer>(m_value).data();
        encoder.encodeBool("hasBinary"_s, !!data);
        if (data)
            encoder.encodeBytes("binary"_s, data->span());
        return;
    }
    case IndexedDB::KeyType::String:
        encoder.encodeString("string"_s, std::get<String>(m_value));
        return;
    case IndexedDB::KeyType::Date:
    case IndexedDB::KeyType::Number:
        encoder.encodeDouble("number"_s, std::get<double>(m_value));
        return;
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        return;
    }

    ASSERT_NOT_REACHED();
}

bool IDBKeyData::decode(KeyedDecoder& decoder, IDBKeyData& result)
{
    bool isNull;
    if (!decoder.decodeBool("null"_s, isNull))
        return false;

    if (isNull)
        return true;

    auto enumFunction = [](IndexedDB::KeyType value) {
        return value == IndexedDB::KeyType::Max
            || value == IndexedDB::KeyType::Invalid
            || value == IndexedDB::KeyType::Array
            || value == IndexedDB::KeyType::Binary
            || value == IndexedDB::KeyType::String
            || value == IndexedDB::KeyType::Date
            || value == IndexedDB::KeyType::Number
            || value == IndexedDB::KeyType::Min;
    };
    IndexedDB::KeyType type;
    if (!decoder.decodeEnum("type"_s, type, enumFunction))
        return false;

    switch (type) {
    case IndexedDB::KeyType::Invalid:
        return true;
    case IndexedDB::KeyType::Max:
        result.m_value = Max { };
        return true;
    case IndexedDB::KeyType::Min:
        result.m_value = Min { };
        return true;
    case IndexedDB::KeyType::String:
        result.m_value = String();
        return decoder.decodeString("string"_s, std::get<String>(result.m_value));
    case IndexedDB::KeyType::Number:
        result.m_value = 0.0;
        return decoder.decodeDouble("number"_s, std::get<double>(result.m_value));
    case IndexedDB::KeyType::Date:
        result.m_value = Date { };
        return decoder.decodeDouble("number"_s, std::get<Date>(result.m_value).value);
    case IndexedDB::KeyType::Binary: {
        result.m_value = ThreadSafeDataBuffer();

        bool hasBinaryData;
        if (!decoder.decodeBool("hasBinary"_s, hasBinaryData))
            return false;

        if (!hasBinaryData)
            return true;

        Vector<uint8_t> bytes;
        if (!decoder.decodeBytes("binary"_s, bytes))
            return false;

        result.m_value = ThreadSafeDataBuffer::create(WTFMove(bytes));
        return true;
    }
    case IndexedDB::KeyType::Array:
        auto arrayFunction = [](KeyedDecoder& decoder, IDBKeyData& result) {
            return decode(decoder, result);
        };

        result.m_value = Vector<IDBKeyData>();
        return decoder.decodeObjects("array"_s, std::get<Vector<IDBKeyData>>(result.m_value), arrayFunction);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

std::weak_ordering operator<=>(const IDBKeyData& a, const IDBKeyData& b)
{
    auto aType = a.type();
    auto bType = b.type();

    if (aType == IndexedDB::KeyType::Invalid) {
        if (bType != IndexedDB::KeyType::Invalid)
            return std::weak_ordering::less;
        if (bType == IndexedDB::KeyType::Invalid)
            return std::weak_ordering::equivalent;
    } else if (bType == IndexedDB::KeyType::Invalid)
        return std::weak_ordering::greater;

    // The IDBKey::type() enum is in reverse sort order.
    if (aType != bType)
        return bType <=> aType;

    // The types are the same, so handle actual value comparison.
    switch (aType) {
    case IndexedDB::KeyType::Invalid:
        // Invalid type should have been fully handled above
        ASSERT_NOT_REACHED();
        return std::weak_ordering::equivalent;
    case IndexedDB::KeyType::Array: {
        auto& aArray = std::get<Vector<IDBKeyData>>(a.m_value);
        auto& bArray = std::get<Vector<IDBKeyData>>(b.m_value);
        for (size_t i = 0; i < aArray.size() && i < bArray.size(); ++i) {
            if (auto result = aArray[i] <=> bArray[i]; is_neq(result))
                return result;
        }
        return aArray.size() <=> bArray.size();
    }
    case IndexedDB::KeyType::Binary:
        return compareBinaryKeyData(std::get<ThreadSafeDataBuffer>(a.m_value), std::get<ThreadSafeDataBuffer>(b.m_value));
    case IndexedDB::KeyType::String:
        return codePointCompare(std::get<String>(a.m_value), std::get<String>(b.m_value));
    case IndexedDB::KeyType::Date:
        return weakOrderingCast(std::get<IDBKeyData::Date>(a.m_value).value <=> std::get<IDBKeyData::Date>(b.m_value).value);
    case IndexedDB::KeyType::Number:
        return weakOrderingCast(std::get<double>(a.m_value) <=> std::get<double>(b.m_value));
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        return std::weak_ordering::equivalent;
    }

    ASSERT_NOT_REACHED();
    return std::weak_ordering::equivalent;
}

String IDBKeyData::loggingString() const
{
    if (isNull())
        return "<null>"_s;

    String result;

    switch (type()) {
    case IndexedDB::KeyType::Invalid:
        return "<invalid>"_s;
    case IndexedDB::KeyType::Array:
        result = makeString("<array> - { "_s, interleave(std::get<Vector<IDBKeyData>>(m_value), [](auto& builder, auto& item) { builder.append(item.loggingString()); }, ", "_s), " }"_s);
        break;

    case IndexedDB::KeyType::Binary: {
        StringBuilder builder;
        builder.append("<binary> - "_s);

        auto* data = std::get<ThreadSafeDataBuffer>(m_value).data();
        if (!data) {
            builder.append("(null)"_s);
            result = builder.toString();
            break;
        }

        size_t i = 0;
        for (; i < 8 && i < data->size(); ++i) {
            uint8_t byte = data->at(i);
            builder.append(upperNibbleToLowercaseASCIIHexDigit(byte),
                lowerNibbleToLowercaseASCIIHexDigit(byte));
        }

        if (data->size() > 8)
            builder.append("..."_s);

        result = builder.toString();
        break;
    }
    case IndexedDB::KeyType::String:
        result = makeString("<string> - "_s, std::get<String>(m_value));
        break;
    case IndexedDB::KeyType::Date:
        return makeString("<date> - "_s, std::get<Date>(m_value).value);
    case IndexedDB::KeyType::Number:
        return makeString("<number> - "_s, std::get<double>(m_value));
    case IndexedDB::KeyType::Max:
        return "<maximum>"_s;
    case IndexedDB::KeyType::Min:
        return "<minimum>"_s;
    }

    if (result.length() > 150)
        result = makeString(StringView(result).left(147), "..."_s);

    return result;
}

void IDBKeyData::setArrayValue(const Vector<IDBKeyData>& value)
{
    m_value = value;
}

void IDBKeyData::setBinaryValue(const ThreadSafeDataBuffer& value)
{
    m_value = value;
}

void IDBKeyData::setStringValue(const String& value)
{
    m_value = value;
}

void IDBKeyData::setDateValue(double value)
{
    m_value = Date { value };
}

void IDBKeyData::setNumberValue(double value)
{
    m_value = value;
}

bool IDBKeyData::isValid() const
{
    if (type() == IndexedDB::KeyType::Invalid)
        return false;
    
    if (type() == IndexedDB::KeyType::Array) {
        for (auto& key : array()) {
            if (!key.isValid())
                return false;
        }
    }

    return true;
}

// See NOTE in https://www.w3.org/TR/IndexedDB/#key-construct.
bool IDBKeyData::isValidValue(const ValueVariant& variant)
{
    return WTF::switchOn(variant, [&](double value) {
        return !std::isnan(value);
    }, [&](const Date& date) {
        return !std::isnan(date.value);
    }, [&](const Vector<IDBKeyData>& keys) {
        return std::ranges::all_of(keys, IDBKeyData::isValidValue, &IDBKeyData::value);
    }, [&](const auto&) {
        return true;
    });
}

bool IDBKeyData::operator==(const IDBKeyData& other) const
{
    if (type() != other.type() || isNull() != other.isNull() || m_isDeletedValue != other.m_isDeletedValue)
        return false;
    switch (type()) {
    case IndexedDB::KeyType::Invalid:
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        return true;
    case IndexedDB::KeyType::Number:
        return std::get<double>(m_value) == std::get<double>(other.m_value);
    case IndexedDB::KeyType::Date:
        return std::get<Date>(m_value).value == std::get<Date>(other.m_value).value;
    case IndexedDB::KeyType::String:
        return std::get<String>(m_value) == std::get<String>(other.m_value);
    case IndexedDB::KeyType::Binary:
        return std::get<ThreadSafeDataBuffer>(m_value) == std::get<ThreadSafeDataBuffer>(other.m_value);
    case IndexedDB::KeyType::Array:
        return std::get<Vector<IDBKeyData>>(m_value) == std::get<Vector<IDBKeyData>>(other.m_value);
    }
    RELEASE_ASSERT_NOT_REACHED();
}

size_t IDBKeyData::size() const
{
    if (isNull())
        return 0;

    switch (type()) {
    case IndexedDB::KeyType::Invalid:
        return 0;
    case IndexedDB::KeyType::Array: {
        Vector<RefPtr<IDBKey>> array;
        size_t totalSize = 0;
        for (auto& keyData : std::get<Vector<IDBKeyData>>(m_value))
            totalSize += keyData.size();
        return totalSize;
    }
    case IndexedDB::KeyType::Binary:
        return std::get<ThreadSafeDataBuffer>(m_value).size();
    case IndexedDB::KeyType::String:
        return std::get<String>(m_value).sizeInBytes();
    case IndexedDB::KeyType::Date:
        return sizeof(Date);
    case IndexedDB::KeyType::Number:
        return sizeof(double);
    case IndexedDB::KeyType::Max:
    case IndexedDB::KeyType::Min:
        return 0;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

} // namespace WebCore
