/*
 * Copyright (C) 2025 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "JSDisposableStack.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSDisposableStack::s_info = { "DisposableStack"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDisposableStack) };

JSDisposableStack* JSDisposableStack::create(VM& vm, Structure* structure)
{
    JSDisposableStack* disposableStack = new (NotNull, allocateCell<JSDisposableStack>(vm)) JSDisposableStack(vm, structure);
    disposableStack->finishCreation(vm);
    return disposableStack;
}

void JSDisposableStack::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    auto values = initialValues();
    ASSERT(values.size() == numberOfInternalFields);
    internalField(Field::State).set(vm, this, values[0]);
    internalField(Field::Capability).set(vm, this, values[1]);
}

template<typename Visitor>
void JSDisposableStack::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSDisposableStack*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

bool JSDisposableStack::disposed()
{
    return internalField(Field::State).get().asInt32() == static_cast<int32_t>(State::Disposed);
}

DEFINE_VISIT_CHILDREN(JSDisposableStack);

} // namespace JSC
