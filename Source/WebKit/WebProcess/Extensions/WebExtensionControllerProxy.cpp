/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebExtensionControllerProxy.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionContextProxy.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerProxyMessages.h"
#include "WebFrame.h"
#include "WebProcess.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace WebCore;

static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>& webExtensionControllerProxies()
{
    static MainRunLoopNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionControllerProxy>>> controllers;
    return controllers;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebExtensionControllerProxy);

RefPtr<WebExtensionControllerProxy> WebExtensionControllerProxy::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllerProxies().get(identifier).get();
}

Ref<WebExtensionControllerProxy> WebExtensionControllerProxy::getOrCreate(const WebExtensionControllerParameters& parameters, WebPage* newPage)
{
    auto updateProperties = [&](WebExtensionControllerProxy& controller) {
        WebExtensionContextProxySet contexts;
        WebExtensionContextProxyBaseURLMap baseURLMap;

        for (auto& contextParameters : parameters.contextParameters) {
            Ref context = WebExtensionContextProxy::getOrCreate(contextParameters, controller, newPage);
            baseURLMap.add(contextParameters.baseURL.protocolHostAndPort(), context);
            contexts.add(context);
        }

        controller.m_testingMode = parameters.testingMode;
        controller.m_extensionContexts = WTFMove(contexts);
        controller.m_extensionContextBaseURLMap = WTFMove(baseURLMap);
    };

    if (RefPtr controller = get(parameters.identifier)) {
        updateProperties(*controller);
        return *controller;
    }

    Ref result = adoptRef(*new WebExtensionControllerProxy(parameters));
    updateProperties(result);
    return result;
}

WebExtensionControllerProxy::WebExtensionControllerProxy(const WebExtensionControllerParameters& parameters)
    : m_identifier(parameters.identifier)
{
    ASSERT(!get(m_identifier));
    webExtensionControllerProxies().add(m_identifier, *this);

    WebProcess::singleton().addMessageReceiver(Messages::WebExtensionControllerProxy::messageReceiverName(), m_identifier, *this);
}

WebExtensionControllerProxy::~WebExtensionControllerProxy()
{
    webExtensionControllerProxies().remove(m_identifier);
    WebProcess::singleton().removeMessageReceiver(*this);
}

void WebExtensionControllerProxy::load(const WebExtensionContextParameters& contextParameters)
{
    auto context = WebExtensionContextProxy::getOrCreate(contextParameters, *this);
    m_extensionContextBaseURLMap.add(contextParameters.baseURL.protocolHostAndPort(), context);
    m_extensionContexts.add(context);
}

void WebExtensionControllerProxy::unload(WebExtensionContextIdentifier contextIdentifier)
{
    m_extensionContextBaseURLMap.removeIf([&](auto& entry) {
        return entry.value->unprivilegedIdentifier() == contextIdentifier;
    });

    m_extensionContexts.removeIf([&](auto& entry) {
        return entry->unprivilegedIdentifier() == contextIdentifier;
    });
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const String& uniqueIdentifier) const
{
    for (auto& extensionContext : m_extensionContexts) {
        if (extensionContext->uniqueIdentifier() == uniqueIdentifier)
            return extensionContext.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(const URL& url) const
{
    return m_extensionContextBaseURLMap.get(url.protocolHostAndPort());
}

RefPtr<WebExtensionContextProxy> WebExtensionControllerProxy::extensionContext(WebFrame& frame, DOMWrapperWorld& world) const
{
    if (!world.isNormal()) {
        auto prefix = "WebExtension-"_s;
        if (!world.name().startsWith(prefix))
            return nullptr;

        auto prefixLength = prefix.length();
        auto uniqueIdentifier = world.name().substring(prefixLength);
        return extensionContext(uniqueIdentifier);
    }

    return extensionContext(frame.url());
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
