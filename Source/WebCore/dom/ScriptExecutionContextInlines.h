/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include "DOMTimer.h"
#include "EventLoop.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerIdentifier.h"
#include <wtf/CheckedRef.h>
#include <wtf/CrossThreadTask.h>
#include <wtf/NativePromise.h>

namespace WebCore {

inline bool ScriptExecutionContext::addTimeout(int timeoutId, DOMTimer& timer)
{
    return m_timeouts.add(timeoutId, &timer).isNewEntry;
}

inline RefPtr<DOMTimer> ScriptExecutionContext::takeTimeout(int timeoutId)
{
    return m_timeouts.take(timeoutId);
}

inline DOMTimer* ScriptExecutionContext::findTimeout(int timeoutId)
{
    return m_timeouts.get(timeoutId);
}

inline ServiceWorker* ScriptExecutionContext::serviceWorker(ServiceWorkerIdentifier identifier)
{
    return m_serviceWorkers.get(identifier);
}

inline CheckedRef<EventLoopTaskGroup> ScriptExecutionContext::checkedEventLoop()
{
    return eventLoop();
}

template<typename... Arguments>
inline void ScriptExecutionContext::postCrossThreadTask(Arguments&&... arguments)
{
    postTask([crossThreadTask = createCrossThreadTask(arguments...)](ScriptExecutionContext&) mutable {
        crossThreadTask.performTask();
    });
}

template<typename Promise, typename TaskType>
void ScriptExecutionContext::enqueueTaskWhenSettled(Ref<Promise>&& promise, TaskSource taskSource, TaskType&& task)
{
    auto request = makeUnique<NativePromiseRequest>();
    WeakPtr weakRequest { *request };
    auto command = promise->whenSettled(nativePromiseDispatcher(), [weakThis = WeakPtr { *this }, taskSource, task = WTFMove(task), request = WTFMove(request)] (auto&& result) mutable {
        request->complete();
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        protectedThis->eventLoop().queueTask(taskSource, [task = WTFMove(task), result = WTFMove(result)] () mutable {
            task(WTFMove(result));
        });
    });
    if (weakRequest) {
        m_nativePromiseRequests.add(*weakRequest);
        command->track(*weakRequest);
    }
}

template<typename Promise, typename TaskType, typename Finalizer>
void ScriptExecutionContext::enqueueTaskWhenSettled(Ref<Promise>&& promise, TaskSource taskSource, TaskType&& task, Finalizer&& finalizer)
{
    enqueueTaskWhenSettled(WTFMove(promise), taskSource, CompletionHandlerWithFinalizer<void(typename Promise::Result&&)>(WTFMove(task), WTFMove(finalizer)));
}

inline ScriptExecutionContext::AddConsoleMessageTask::AddConsoleMessageTask(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
    : Task([&consoleMessage](ScriptExecutionContext& context) {
        context.addConsoleMessage(WTFMove(consoleMessage));
    })
{
}

inline ScriptExecutionContext::AddConsoleMessageTask::AddConsoleMessageTask(MessageSource source, MessageLevel level, const String& message)
    : Task([source, level, message = message.isolatedCopy()](ScriptExecutionContext& context) {
        context.addConsoleMessage(source, level, message);
    })
{
}

} // namespace WebCore
