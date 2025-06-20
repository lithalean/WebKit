/*
 * Copyright (C) 2023 Apple, Inc. All rights reserved.
 * Copyright (C) 2010-2014 Google, Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HTMLEntitySearch.h"

#include "HTMLEntityTable.h"
#include <numeric>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

static const HTMLEntityTableEntry* midpoint(const HTMLEntityTableEntry* left, const HTMLEntityTableEntry* right)
{
    return std::midpoint(left, right);
}

HTMLEntitySearch::HTMLEntitySearch()
    : m_first(HTMLEntityTable::firstEntry())
    , m_last(HTMLEntityTable::lastEntry())
{
}

HTMLEntitySearch::CompareResult HTMLEntitySearch::compare(const HTMLEntityTableEntry* entry, UChar nextCharacter) const
{
    UChar entryNextCharacter;
    if (entry->nameLengthExcludingSemicolon < m_currentLength + 1) {
        if (!entry->nameIncludesTrailingSemicolon || entry->nameLengthExcludingSemicolon < m_currentLength)
            return Before;
        entryNextCharacter = ';';
    } else
        entryNextCharacter = entry->nameCharacters()[m_currentLength];
    if (entryNextCharacter == nextCharacter)
        return Prefix;
    return entryNextCharacter < nextCharacter ? Before : After;
}

const HTMLEntityTableEntry* HTMLEntitySearch::findFirst(UChar nextCharacter) const
{
    auto* left = m_first;
    auto* right = m_last;
    if (left == right)
        return left;
    CompareResult result = compare(left, nextCharacter);
    if (result == Prefix)
        return left;
    if (result == After)
        return right;
    while (left + 1 < right) {
        auto* probe = midpoint(left, right);
        result = compare(probe, nextCharacter);
        if (result == Before)
            left = probe;
        else {
            ASSERT(result == After || result == Prefix);
            right = probe;
        }
    }
    ASSERT(left + 1 == right);
    return right;
}

const HTMLEntityTableEntry* HTMLEntitySearch::findLast(UChar nextCharacter) const
{
    auto* left = m_first;
    auto* right = m_last;
    if (left == right)
        return right;
    CompareResult result = compare(right, nextCharacter);
    if (result == Prefix)
        return right;
    if (result == Before)
        return left;
    while (left + 1 < right) {
        auto* probe = midpoint(left, right);
        result = compare(probe, nextCharacter);
        if (result == After)
            right = probe;
        else {
            ASSERT(result == Before || result == Prefix);
            left = probe;
        }
    }
    ASSERT(left + 1 == right);
    return left;
}

void HTMLEntitySearch::advance(UChar nextCharacter)
{
    ASSERT(isEntityPrefix());
    if (!m_currentLength) {
        m_first = HTMLEntityTable::firstEntryStartingWith(nextCharacter);
        m_last = HTMLEntityTable::lastEntryStartingWith(nextCharacter);
        if (!m_first || !m_last)
            return fail();
    } else {
        m_first = findFirst(nextCharacter);
        m_last = findLast(nextCharacter);
        if (m_first == m_last && compare(m_first, nextCharacter) != Prefix)
            return fail();
    }
    ++m_currentLength;
    if (m_first->nameLength() != m_currentLength)
        return;
    m_mostRecentMatch = m_first;
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
