/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <yusukesuzuki@slowstart.org>.
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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmOps.h"
#include "WasmParser.h"
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/MakeString.h>

namespace JSC { namespace Wasm {

class SectionParser final : public Parser<void> {
public:
    SectionParser(std::span<const uint8_t> data, size_t offsetInSource, ModuleInformation& info)
        : Parser(data)
        , m_offsetInSource(offsetInSource)
        , m_info(info)
    {
    }

#define WASM_SECTION_DECLARE_PARSER(NAME, ID, ORDERING, DESCRIPTION) PartialResult WARN_UNUSED_RETURN parse ## NAME();
    FOR_EACH_KNOWN_WASM_SECTION(WASM_SECTION_DECLARE_PARSER)
#undef WASM_SECTION_DECLARE_PARSER

    PartialResult WARN_UNUSED_RETURN parseCustom();

private:
    template <typename ...Args>
    NEVER_INLINE UnexpectedResult WARN_UNUSED_RETURN fail(Args... args) const
    {
        using namespace FailureHelper; // See ADL comment in namespace above.
        if (ASSERT_ENABLED && Options::crashOnFailedWasmValidate()) [[unlikely]]
            CRASH();

        return UnexpectedResult(makeString("WebAssembly.Module doesn't parse at byte "_s, String::number(m_offset + m_offsetInSource), ": "_s, makeString(args)...));
    }

    PartialResult WARN_UNUSED_RETURN parseGlobalType(GlobalInformation&);
    PartialResult WARN_UNUSED_RETURN parseMemoryHelper(bool isImport);
    PartialResult WARN_UNUSED_RETURN parseTableHelper(bool isImport);
    enum class LimitsType { Memory, Table };
    PartialResult WARN_UNUSED_RETURN parseResizableLimits(uint32_t& initial, std::optional<uint32_t>& maximum, bool& isShared, LimitsType);
    PartialResult WARN_UNUSED_RETURN parseInitExpr(uint8_t&, bool&, uint64_t&, v128_t&, Type, Type& initExprType);
    PartialResult WARN_UNUSED_RETURN parseI32InitExpr(std::optional<I32InitExpr>&, ASCIILiteral failMessage);

    PartialResult WARN_UNUSED_RETURN parseFunctionType(uint32_t position, RefPtr<TypeDefinition>&);
    PartialResult WARN_UNUSED_RETURN parsePackedType(PackedType&);
    PartialResult WARN_UNUSED_RETURN parseStorageType(StorageType&);
    PartialResult WARN_UNUSED_RETURN parseStructType(uint32_t position, RefPtr<TypeDefinition>&);
    PartialResult WARN_UNUSED_RETURN parseArrayType(uint32_t position, RefPtr<TypeDefinition>&);
    PartialResult WARN_UNUSED_RETURN parseRecursionGroup(uint32_t position, RefPtr<TypeDefinition>&);
    PartialResult WARN_UNUSED_RETURN parseSubtype(uint32_t position, RefPtr<TypeDefinition>&, Vector<TypeIndex>&, bool);

    PartialResult WARN_UNUSED_RETURN validateElementTableIdx(uint32_t, Type);
    PartialResult WARN_UNUSED_RETURN parseI32InitExprForElementSection(std::optional<I32InitExpr>&);
    PartialResult WARN_UNUSED_RETURN parseElementKind(uint8_t& elementKind);
    PartialResult WARN_UNUSED_RETURN parseIndexCountForElementSection(uint32_t&, const unsigned);
    PartialResult WARN_UNUSED_RETURN parseElementSegmentVectorOfExpressions(Type, Vector<Element::InitializationType>&, Vector<uint64_t>&, const unsigned, const unsigned);
    PartialResult WARN_UNUSED_RETURN parseElementSegmentVectorOfIndexes(Vector<Element::InitializationType>&, Vector<uint64_t>&, const unsigned, const unsigned);

    PartialResult WARN_UNUSED_RETURN parseI32InitExprForDataSection(std::optional<I32InitExpr>&);

    static bool checkStructuralSubtype(const TypeDefinition&, const TypeDefinition&);
    PartialResult WARN_UNUSED_RETURN checkSubtypeValidity(const TypeDefinition&);

    size_t m_offsetInSource;
    const Ref<ModuleInformation> m_info;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
