/*
 * Copyright (C) 2010 Google, Inc. All rights reserved.
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

#include "NestingLevelIncrementer.h"
#include "Timer.h"
#include <wtf/CheckedPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThread.h"
#endif

namespace WebCore {

class Document;
class HTMLDocumentParser;
class ScriptElement;

class ActiveParserSession {
public:
    explicit ActiveParserSession(Document*);
    ~ActiveParserSession();

private:
    RefPtr<Document> m_document;
};

class PumpSession : public NestingLevelIncrementer, public ActiveParserSession {
public:
    PumpSession(unsigned& nestingLevel, Document*);
    ~PumpSession();

    unsigned processedTokens { 0 };
    unsigned processedTokensOnLastCheck { 0 };
    unsigned processedTokensOnLastYieldBeforeScript { 0 };
    MonotonicTime startTime { MonotonicTime::now() };
    bool didSeeScript { false };
};

class HTMLParserScheduler final : public RefCounted<HTMLParserScheduler> {
    WTF_MAKE_TZONE_ALLOCATED(HTMLParserScheduler);
    WTF_MAKE_NONCOPYABLE(HTMLParserScheduler);
public:
    static Ref<HTMLParserScheduler> create(HTMLDocumentParser&);
    ~HTMLParserScheduler();

    void detach();

    bool shouldYieldBeforeToken(PumpSession& session)
    {
#if PLATFORM(IOS_FAMILY)
        if (WebThreadShouldYield())
            return true;
#endif
        if (m_documentHasActiveParserYieldTokens) [[unlikely]]
            return true;

        if (session.processedTokens > session.processedTokensOnLastCheck + numberOfTokensBeforeCheckingForYield || session.didSeeScript) [[unlikely]]
            return checkForYield(session);

        ++session.processedTokens;
        return false;
    }
    bool shouldYieldBeforeExecutingScript(const ScriptElement*, PumpSession&);

    void scheduleForResume();
    bool isScheduledForResume() const { return m_isSuspendedWithActiveTimer || m_continueNextChunkTimer.isActive() || m_documentHasActiveParserYieldTokens; }

    void suspend();
    void resume();

    void didBeginYieldingParser()
    {
        ASSERT(!m_documentHasActiveParserYieldTokens);
        m_documentHasActiveParserYieldTokens = true;
    }

    void didEndYieldingParser()
    {
        ASSERT(m_documentHasActiveParserYieldTokens);
        m_documentHasActiveParserYieldTokens = false;

        if (!isScheduledForResume())
            scheduleForResume();
    }

private:
    explicit HTMLParserScheduler(HTMLDocumentParser&);

    static const unsigned numberOfTokensBeforeCheckingForYield = 4096; // Performance optimization

    void continueNextChunkTimerFired();

    bool checkForYield(PumpSession& session)
    {
        session.processedTokensOnLastCheck = session.processedTokens;
        session.didSeeScript = false;

        Seconds elapsedTime = MonotonicTime::now() - session.startTime;
        return elapsedTime > m_parserTimeLimit;
    }

    CheckedPtr<HTMLDocumentParser> m_parser;

    Seconds m_parserTimeLimit;
    Timer m_continueNextChunkTimer;
    bool m_isSuspendedWithActiveTimer;
#if ASSERT_ENABLED
    bool m_suspended;
#endif
    bool m_documentHasActiveParserYieldTokens { false };
};

} // namespace WebCore
