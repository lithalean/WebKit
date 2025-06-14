/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "HTMLModelElement.h"

#if ENABLE(MODEL_ELEMENT)

#include "CachedResourceLoader.h"
#include "ContainerNodeInlines.h"
#include "DOMMatrixReadOnly.h"
#include "DOMPointReadOnly.h"
#include "DOMPromiseProxy.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementInlines.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatPoint3D.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerCA.h"
#include "HTMLModelElementCamera.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSourceElement.h"
#include "JSDOMPromiseDeferred.h"
#include "JSEventTarget.h"
#include "JSHTMLModelElement.h"
#include "JSHTMLModelElementCamera.h"
#include "LayoutRect.h"
#include "LayoutSize.h"
#include "LegacySchemeRegistry.h"
#include "Logging.h"
#include "MIMETypeRegistry.h"
#include "Model.h"
#include "ModelPlayer.h"
#include "ModelPlayerAnimationState.h"
#include "ModelPlayerProvider.h"
#include "ModelPlayerTransformState.h"
#include "MouseEvent.h"
#include "NodeInlines.h"
#include "Page.h"
#include "PlaceholderModelPlayer.h"
#include "RenderBoxInlines.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderModel.h"
#include "RenderReplaced.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

#if ENABLE(MODEL_PROCESS)
#include "ModelContext.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLModelElement);

static const Seconds reloadModelDelay { 1_s };

HTMLModelElement::HTMLModelElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, { TypeFlag::HasCustomStyleResolveCallbacks, TypeFlag::HasDidMoveToNewDocument })
    , ActiveDOMObject(document)
    , m_readyPromise { makeUniqueRef<ReadyPromise>(*this, &HTMLModelElement::readyPromiseResolve) }
#if ENABLE(MODEL_PROCESS)
    , m_entityTransform(DOMMatrixReadOnly::create(TransformationMatrix::identity, DOMMatrixReadOnly::Is2D::No))
    , m_boundingBoxCenter(DOMPointReadOnly::create({ }))
    , m_boundingBoxExtents(DOMPointReadOnly::create({ }))
    , m_environmentMapReadyPromise(makeUniqueRef<EnvironmentMapPromise>())
#endif
{
}

HTMLModelElement::~HTMLModelElement()
{
    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }

#if ENABLE(MODEL_PROCESS)
    if (m_environmentMapResource) {
        m_environmentMapResource->removeClient(*this);
        m_environmentMapResource = nullptr;
    }
#endif

    deleteModelPlayer();
}

Ref<HTMLModelElement> HTMLModelElement::create(const QualifiedName& tagName, Document& document)
{
    auto model = adoptRef(*new HTMLModelElement(tagName, document));
    model->suspendIfNeeded();
    return model;
}

void HTMLModelElement::suspend(ReasonForSuspension reasonForSuspension)
{
    RELEASE_LOG(ModelElement, "%p - HTMLModelElement::suspend(): %d", this, static_cast<int>(reasonForSuspension));

    if (reasonForSuspension == ReasonForSuspension::BackForwardCache)
        unloadModelPlayer(true);
}

void HTMLModelElement::resume()
{
    RELEASE_LOG(ModelElement, "%p - HTMLModelElement::resume()", this);
    startReloadModelTimer();
}

RefPtr<Model> HTMLModelElement::model() const
{
    if (!m_dataComplete)
        return nullptr;
    
    return m_model;
}

static bool isSupportedModelType(const AtomString& type)
{
    return type.isEmpty() || MIMETypeRegistry::isSupportedModelMIMEType(type);
}

URL HTMLModelElement::selectModelSource() const
{
    // FIXME: This should probably work more like media element resource
    // selection, where if a <source> element fails to load, an error event
    // is dispatched to it, and we continue to try subsequent <source>s.

    if (!document().hasBrowsingContext())
        return { };

    if (auto src = getNonEmptyURLAttribute(srcAttr); src.isValid())
        return src;

    for (auto& element : childrenOfType<HTMLSourceElement>(*this)) {
        if (!isSupportedModelType(element.attributeWithoutSynchronization(typeAttr)))
            continue;

        if (auto src = element.getNonEmptyURLAttribute(srcAttr); src.isValid())
            return src;
    }

    return { };
}

void HTMLModelElement::visibilityStateChanged()
{
    if (m_modelPlayer)
        m_modelPlayer->visibilityStateDidChange();

    if (!isVisible()) {
        m_reloadModelTimer = nullptr;
        return;
    }

    if (m_modelPlayer && !m_modelPlayer->isPlaceholder())
        return;

    startReloadModelTimer();
}

void HTMLModelElement::sourcesChanged()
{
    setSourceURL(selectModelSource());
}

CachedResourceRequest HTMLModelElement::createResourceRequest(const URL& resourceURL, FetchOptions::Destination destination)
{
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.destination = destination;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    auto crossOriginAttribute = parseCORSSettingsAttribute(attributeWithoutSynchronization(HTMLNames::crossoriginAttr));
    // Make sure CORS is always enabled by passing a non-null cross origin attribute
    if (crossOriginAttribute.isNull()) {
        Ref documentOrigin = protectedDocument()->securityOrigin();
        if (LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(documentOrigin->protocol()) || documentOrigin->protocol() != resourceURL.protocol())
            crossOriginAttribute = "anonymous"_s;
    }
    auto request = createPotentialAccessControlRequest(ResourceRequest { URL { resourceURL } }, WTFMove(options), document(), crossOriginAttribute);
    request.setInitiator(*this);

    return request;
}

void HTMLModelElement::setSourceURL(const URL& url)
{
    if (url == m_sourceURL)
        return;

    m_sourceURL = url;

    m_data.reset();
    m_dataMemoryCost.store(0, std::memory_order_relaxed);
    m_dataComplete = false;
    m_model = nullptr;

    if (m_resource) {
        m_resource->removeClient(*this);
        m_resource = nullptr;
    }

    deleteModelPlayer();

#if ENABLE(MODEL_PROCESS)
    m_entityTransform = DOMMatrixReadOnly::create(TransformationMatrix::identity, DOMMatrixReadOnly::Is2D::No);
    m_boundingBoxCenter = DOMPointReadOnly::create({ });
    m_boundingBoxExtents = DOMPointReadOnly::create({ });
#endif

    if (!m_readyPromise->isFulfilled())
        m_readyPromise->reject(Exception { ExceptionCode::AbortError });

    m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &HTMLModelElement::readyPromiseResolve);
    m_shouldCreateModelPlayerUponRendererAttachment = false;

    if (m_sourceURL.isEmpty()) {
        ActiveDOMObject::queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
        reportExtraMemoryCost();
        return;
    }

    auto request = createResourceRequest(m_sourceURL, FetchOptions::Destination::Model);
    auto resource = document().protectedCachedResourceLoader()->requestModelResource(WTFMove(request));
    if (!resource.has_value()) {
        ActiveDOMObject::queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
        if (!m_readyPromise->isFulfilled())
            m_readyPromise->reject(Exception { ExceptionCode::NetworkError });
        return;
    }

    m_data.empty();

    m_resource = resource.value();
    m_resource->addClient(*this);
}

HTMLModelElement& HTMLModelElement::readyPromiseResolve()
{
    return *this;
}

// MARK: - DOM overrides.

void HTMLModelElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    ActiveDOMObject::didMoveToNewDocument(newDocument);
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    sourcesChanged();
}

// MARK: - Rendering overrides.

RenderPtr<RenderElement> HTMLModelElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    return createRenderer<RenderModel>(*this, WTFMove(style));
}

void HTMLModelElement::didAttachRenderers()
{
    if (!m_shouldCreateModelPlayerUponRendererAttachment)
        return;

    m_shouldCreateModelPlayerUponRendererAttachment = false;
    createModelPlayer();
}

// MARK: - CachedRawResourceClient

void HTMLModelElement::dataReceived(CachedResource& resource, const SharedBuffer& buffer)
{
    if (&resource == m_resource)
        m_data.append(buffer);
#if ENABLE(MODEL_PROCESS)
    else if (&resource == m_environmentMapResource)
        m_environmentMapData.append(buffer);
#endif
    else
        ASSERT_NOT_REACHED();
}

void HTMLModelElement::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    if (&resource == m_resource)
        modelResourceFinished();
#if ENABLE(MODEL_PROCESS)
    else if (&resource == m_environmentMapResource)
        environmentMapResourceFinished();
#endif
}

// MARK: - ModelPlayer support

void HTMLModelElement::modelDidChange()
{
    auto* page = document().page();
    if (!page) {
        if (!m_readyPromise->isFulfilled())
            m_readyPromise->reject(Exception { ExceptionCode::AbortError });
        return;
    }

    auto* renderer = this->renderer();
    if (!renderer) {
        m_shouldCreateModelPlayerUponRendererAttachment = true;
        return;
    }

    createModelPlayer();
}

void HTMLModelElement::createModelPlayer()
{
    if (!m_model)
        return;

    auto size = contentSize();
    if (size.isEmpty())
        return;

    if (m_modelPlayer)
        deleteModelPlayer();

    ASSERT(document().page());
#if ENABLE(MODEL_PROCESS)
    m_entityTransform = DOMMatrixReadOnly::create(TransformationMatrix::identity, DOMMatrixReadOnly::Is2D::No);
    m_boundingBoxCenter = DOMPointReadOnly::create({ });
    m_boundingBoxExtents = DOMPointReadOnly::create({ });
#endif
    if (!m_modelPlayerProvider)
        m_modelPlayerProvider = document().page()->modelPlayerProvider();
    if (RefPtr protectedModelPlayerProvider = m_modelPlayerProvider.get())
        m_modelPlayer = protectedModelPlayerProvider->createModelPlayer(*this);
    if (!m_modelPlayer) {
        if (!m_readyPromise->isFulfilled())
            m_readyPromise->reject(Exception { ExceptionCode::AbortError });
        return;
    }

#if ENABLE(MODEL_PROCESS)
    m_modelPlayer->setAutoplay(autoplay());
    m_modelPlayer->setLoop(loop());
    m_modelPlayer->setPlaybackRate(m_playbackRate, [&](double) { });
    m_modelPlayer->setHasPortal(hasPortal());
    m_modelPlayer->setStageMode(stageMode());
#endif

    // FIXME: We need to tell the player if the size changes as well, so passing this
    // in with load probably doesn't make sense.
    m_modelPlayer->load(*m_model, size);

#if ENABLE(MODEL_PROCESS)
    if (m_environmentMapData)
        m_modelPlayer->setEnvironmentMap(m_environmentMapData.takeAsContiguous().get());
    else if (!m_environmentMapURL.isEmpty())
        environmentMapRequestResource();
#endif
}

void HTMLModelElement::deleteModelPlayer()
{
    RefPtr protectedModelPlayerProvider = m_modelPlayerProvider.get();
    if (protectedModelPlayerProvider && m_modelPlayer)
        protectedModelPlayerProvider->deleteModelPlayer(*m_modelPlayer);

    m_modelPlayer = nullptr;
}

void HTMLModelElement::unloadModelPlayer(bool onSuspend)
{
    if (!m_modelPlayer || m_modelPlayer->isPlaceholder())
        return;

    auto animationState = m_modelPlayer->currentAnimationState();
    auto transformState = m_modelPlayer->currentTransformState();
    if (!animationState || !transformState) {
        RELEASE_LOG(ModelElement, "%p - HTMLModelElement: Model player cannot handle temporary unload", this);
        deleteModelPlayer();
        return;
    }

    RELEASE_LOG(ModelElement, "%p - HTMLModelElement: Temporarily unload model player: %p", this, m_modelPlayer.get());
    deleteModelPlayer();

    m_modelPlayer = PlaceholderModelPlayer::create(onSuspend, *animationState, WTFMove(*transformState));
}

void HTMLModelElement::reloadModelPlayer()
{
    if (!m_modelPlayer) {
        RELEASE_LOG_INFO(ModelElement, "%p - HTMLModelElement::reloadModelPlayer: no model player", this);
        createModelPlayer();
        return;
    }

    if (!m_modelPlayer->isPlaceholder()) {
        RELEASE_LOG_INFO(ModelElement, "%p - HTMLModelElement::reloadModelPlayer: no placeholder to reload", this);
        return;
    }

    if (!m_model) {
        RELEASE_LOG_INFO(ModelElement, "%p - HTMLModelElement::reloadModelPlayer: no model to reload", this);
        return;
    }

    auto size = contentSize();
    if (size.isEmpty()) {
        RELEASE_LOG_INFO(ModelElement, "%p - HTMLModelElement::reloadModelPlayer: content size is empty", this);
        return;
    }

    ASSERT(document().page());

    auto animationState = m_modelPlayer->currentAnimationState();
    auto transformState = m_modelPlayer->currentTransformState();
    ASSERT(animationState && transformState);

    if (!m_modelPlayerProvider)
        m_modelPlayerProvider = document().page()->modelPlayerProvider();
    if (RefPtr protectedModelPlayerProvider = m_modelPlayerProvider.get())
        m_modelPlayer = protectedModelPlayerProvider->createModelPlayer(*this);
    if (!m_modelPlayer) {
        RELEASE_LOG_ERROR(ModelElement, "%p - HTMLModelElement: Failed to create model player to reload with", this);
        return;
    }

    RELEASE_LOG(ModelElement, "%p - HTMLModelElement: Reloading previous states to new model player: %p", this, m_modelPlayer.get());
    m_modelPlayer->reload(*m_model, size, *animationState, WTFMove(*transformState));

#if ENABLE(MODEL_PROCESS)
    if (m_environmentMapData)
        m_modelPlayer->setEnvironmentMap(m_environmentMapData.takeAsContiguous().get());
    else if (!m_environmentMapURL.isEmpty())
        environmentMapRequestResource();
#endif
}

void HTMLModelElement::startReloadModelTimer()
{
    if (m_reloadModelTimer)
        return;

    Seconds delay = document().page() && document().page()->shouldDisableModelLoadDelaysForTesting() ? 0_s : reloadModelDelay;
    m_reloadModelTimer = document().checkedEventLoop()->scheduleTask(delay, TaskSource::ModelElement, [weakThis = WeakPtr { *this }] {
        if (weakThis)
            weakThis->reloadModelTimerFired();
    });
}

void HTMLModelElement::reloadModelTimerFired()
{
    m_reloadModelTimer = nullptr;

    if (!isVisible())
        return;

    RELEASE_LOG(ModelElement, "%p - HTMLModelElement: Reloading model player after becoming visible", this);
    reloadModelPlayer();
}

bool HTMLModelElement::usesPlatformLayer() const
{
    return m_modelPlayer && m_modelPlayer->layer();
}

PlatformLayer* HTMLModelElement::platformLayer() const
{
    if (m_modelPlayer)
        return m_modelPlayer->layer();
    return nullptr;
}

std::optional<LayerHostingContextIdentifier> HTMLModelElement::layerHostingContextIdentifier() const
{
    if (m_modelPlayer)
        return m_modelPlayer->layerHostingContextIdentifier();
    return std::nullopt;
}

void HTMLModelElement::sizeMayHaveChanged()
{
    if (m_modelPlayer)
        m_modelPlayer->sizeDidChange(contentSize());
    else
        createModelPlayer();
}

void HTMLModelElement::didUpdateLayerHostingContextIdentifier(ModelPlayer& modelPlayer, LayerHostingContextIdentifier identifier)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);
    ASSERT_UNUSED(identifier, identifier.toUInt64() > 0);

    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();
}

void HTMLModelElement::logWarning(ModelPlayer& modelPlayer, const String& warningMessage)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);

    protectedDocument()->addConsoleMessage(MessageSource::Other, MessageLevel::Warning, warningMessage);
}

void HTMLModelElement::didFinishLoading(ModelPlayer& modelPlayer)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);

    reportExtraMemoryCost();

    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();
    if (!m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);
}

void HTMLModelElement::didFailLoading(ModelPlayer& modelPlayer, const ResourceError&)
{
    ASSERT_UNUSED(modelPlayer, &modelPlayer == m_modelPlayer);
    if (!m_readyPromise->isFulfilled())
        m_readyPromise->reject(Exception { ExceptionCode::AbortError });

    m_dataMemoryCost.store(0, std::memory_order_relaxed);
    reportExtraMemoryCost();
}

RefPtr<GraphicsLayer> HTMLModelElement::graphicsLayer() const
{
    auto* page = document().page();
    if (!page)
        return nullptr;

    auto* renderLayerModelObject = dynamicDowncast<RenderLayerModelObject>(this->renderer());
    if (!renderLayerModelObject)
        return nullptr;

    if (!renderLayerModelObject->isComposited())
        return nullptr;

    return renderLayerModelObject->layer()->backing()->graphicsLayer();
}

std::optional<PlatformLayerIdentifier> HTMLModelElement::layerID() const
{
    auto graphicsLayer = this->graphicsLayer();
    if (!graphicsLayer)
        return std::nullopt;

    return graphicsLayer->primaryLayerID();
}

std::optional<PlatformLayerIdentifier> HTMLModelElement::modelContentsLayerID() const
{
    auto graphicsLayer = this->graphicsLayer();
    if (!graphicsLayer)
        return std::nullopt;

    return graphicsLayer->contentsLayerIDForModel();
}

bool HTMLModelElement::isVisible() const
{
    return !document().hidden();
}

#if ENABLE(MODEL_PROCESS)
RefPtr<ModelContext> HTMLModelElement::modelContext() const
{
    auto modelLayerIdentifier = layerID();
    if (!modelLayerIdentifier)
        return nullptr;

    auto modelContentsLayerHostingContextIdentifier = layerHostingContextIdentifier();
    if (!modelContentsLayerHostingContextIdentifier)
        return nullptr;

    return ModelContext::create(*modelLayerIdentifier, *modelContentsLayerHostingContextIdentifier, contentSize(), hasPortal() ? ModelContextDisablePortal::No : ModelContextDisablePortal::Yes, std::nullopt).ptr();
}

const DOMMatrixReadOnly& HTMLModelElement::entityTransform() const
{
    return m_entityTransform.get();
}

ExceptionOr<void> HTMLModelElement::setEntityTransform(const DOMMatrixReadOnly& transform)
{
    if (supportsStageModeInteraction())
        return Exception { ExceptionCode::InvalidStateError,  "Transform is read-only unless StageMode is set to 'none'"_s };

    auto player = m_modelPlayer;
    if (!player) {
        ASSERT_NOT_REACHED();
        return Exception { ExceptionCode::UnknownError };
    }

    TransformationMatrix matrix = transform.transformationMatrix();

    if (!player->supportsTransform(matrix))
        return Exception { ExceptionCode::NotSupportedError };

    m_entityTransform = DOMMatrixReadOnly::create(matrix, DOMMatrixReadOnly::Is2D::No);
    player->setEntityTransform(matrix);

    return { };
}

void HTMLModelElement::didUpdateEntityTransform(ModelPlayer&, const TransformationMatrix& transform)
{
    m_entityTransform = DOMMatrixReadOnly::create(transform, DOMMatrixReadOnly::Is2D::No);
}

const DOMPointReadOnly& HTMLModelElement::boundingBoxCenter() const
{
    return m_boundingBoxCenter;
}

const DOMPointReadOnly& HTMLModelElement::boundingBoxExtents() const
{
    return m_boundingBoxExtents;
}

void HTMLModelElement::didUpdateBoundingBox(ModelPlayer&, const FloatPoint3D& center, const FloatPoint3D& extents)
{
    m_boundingBoxCenter = DOMPointReadOnly::fromFloatPoint(center);
    m_boundingBoxExtents = DOMPointReadOnly::fromFloatPoint(extents);
}

void HTMLModelElement::didFinishEnvironmentMapLoading(bool succeeded)
{
    if (!m_environmentMapURL.isEmpty() && !m_environmentMapReadyPromise->isFulfilled()) {
        if (succeeded)
            m_environmentMapReadyPromise->resolve();
        else {
            m_environmentMapReadyPromise->reject(Exception { ExceptionCode::AbortError });
            m_environmentMapDataMemoryCost.store(0, std::memory_order_relaxed);
        }
        reportExtraMemoryCost();
    }
}

bool HTMLModelElement::supportsStageModeInteraction() const
{
    return stageMode() != StageModeOperation::None;
}

void HTMLModelElement::beginStageModeTransform(const TransformationMatrix& transform)
{
    if (m_modelPlayer)
        m_modelPlayer->beginStageModeTransform(transform);
}

void HTMLModelElement::updateStageModeTransform(const TransformationMatrix& transform)
{
    if (m_modelPlayer)
        m_modelPlayer->updateStageModeTransform(transform);
}

void HTMLModelElement::endStageModeInteraction()
{
    if (m_modelPlayer)
        m_modelPlayer->endStageModeInteraction();
}

void HTMLModelElement::tryAnimateModelToFitPortal(bool handledDrag, CompletionHandler<void(bool)>&& completionHandler)
{
    if (hasPortal() && m_modelPlayer)
        return m_modelPlayer->animateModelToFitPortal(WTFMove(completionHandler));

    completionHandler(handledDrag);
}

void HTMLModelElement::resetModelTransformAfterDrag()
{
    if (hasPortal() && m_modelPlayer)
        m_modelPlayer->resetModelTransformAfterDrag();
}

void HTMLModelElement::didUnload(ModelPlayer& modelPlayer)
{
    if (m_modelPlayer != &modelPlayer)
        return;

    unloadModelPlayer(false);

    if (!isVisible())
        return;

    // FIXME: rdar://148027600 Prevent infinite reloading of model.
    startReloadModelTimer();
}

#endif // ENABLE(MODEL_PROCESS)

// MARK: - Fullscreen support.

void HTMLModelElement::enterFullscreen()
{
    if (m_modelPlayer)
        m_modelPlayer->enterFullscreen();
}

// MARK: - Interaction support.

bool HTMLModelElement::supportsDragging() const
{
    if (!m_modelPlayer)
        return true;

    return m_modelPlayer->supportsDragging();
}

bool HTMLModelElement::isDraggableIgnoringAttributes() const
{
    return supportsDragging();
}

bool HTMLModelElement::isInteractive() const
{
    return hasAttributeWithoutSynchronization(HTMLNames::interactiveAttr);
}

void HTMLModelElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == srcAttr)
        sourcesChanged();
    else if (name == interactiveAttr) {
        if (m_modelPlayer)
            m_modelPlayer->setInteractionEnabled(isInteractive());
    }
#if ENABLE(MODEL_PROCESS)
    else if (name == autoplayAttr)
        updateAutoplay();
    else if (name == loopAttr)
        updateLoop();
    else if (name == environmentmapAttr)
        updateEnvironmentMap();
    else if (name == stagemodeAttr)
        updateStageMode();
#if PLATFORM(VISION)
    else if (document().settings().modelNoPortalAttributeEnabled() && name == noportalAttr)
        updateHasPortal();
#endif
#endif
    else
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void HTMLModelElement::defaultEventHandler(Event& event)
{
    HTMLElement::defaultEventHandler(event);

    if (!m_modelPlayer || !m_modelPlayer->supportsMouseInteraction())
        return;

    auto type = event.type();
    if (type != eventNames().mousedownEvent && type != eventNames().mousemoveEvent && type != eventNames().mouseupEvent)
        return;

    auto& mouseEvent = downcast<MouseEvent>(event);

    if (mouseEvent.button() != MouseButton::Left)
        return;

    if (type == eventNames().mousedownEvent && !m_isDragging && !event.defaultPrevented() && isInteractive())
        dragDidStart(mouseEvent);
    else if (type == eventNames().mousemoveEvent && m_isDragging)
        dragDidChange(mouseEvent);
    else if (type == eventNames().mouseupEvent && m_isDragging)
        dragDidEnd(mouseEvent);
}

LayoutPoint HTMLModelElement::flippedLocationInElementForMouseEvent(MouseEvent& event)
{
    LayoutUnit flippedY { event.offsetY() };
    if (auto* renderModel = dynamicDowncast<RenderModel>(renderer()))
        flippedY = renderModel->paddingBoxHeight() - flippedY;
    return { LayoutUnit(event.offsetX()), flippedY };
}

void HTMLModelElement::dragDidStart(MouseEvent& event)
{
    ASSERT(!m_isDragging);

    RefPtr frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsElement(this);
    event.setDefaultHandled();
    m_isDragging = true;

    if (m_modelPlayer)
        m_modelPlayer->handleMouseDown(flippedLocationInElementForMouseEvent(event), event.timeStamp());
}

void HTMLModelElement::dragDidChange(MouseEvent& event)
{
    ASSERT(m_isDragging);

    event.setDefaultHandled();

    if (m_modelPlayer)
        m_modelPlayer->handleMouseMove(flippedLocationInElementForMouseEvent(event), event.timeStamp());
}

void HTMLModelElement::dragDidEnd(MouseEvent& event)
{
    ASSERT(m_isDragging);

    RefPtr frame = document().frame();
    if (!frame)
        return;

    frame->eventHandler().setCapturingMouseEventsElement(nullptr);
    event.setDefaultHandled();
    m_isDragging = false;

    if (m_modelPlayer)
        m_modelPlayer->handleMouseUp(flippedLocationInElementForMouseEvent(event), event.timeStamp());
}

// MARK: - Camera support.

void HTMLModelElement::getCamera(CameraPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject(Exception { ExceptionCode::AbortError });
        return;
    }

    m_modelPlayer->getCamera([promise = WTFMove(promise)] (std::optional<HTMLModelElementCamera> camera) mutable {
        if (!camera)
            promise.reject();
        else
            promise.resolve(*camera);
    });
}

void HTMLModelElement::setCamera(HTMLModelElementCamera camera, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject(Exception { ExceptionCode::AbortError });
        return;
    }

    m_modelPlayer->setCamera(camera, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

// MARK: - Animations support.

#if ENABLE(MODEL_PROCESS)
void HTMLModelElement::setPlaybackRate(double playbackRate)
{
    if (m_playbackRate == playbackRate)
        return;

    m_playbackRate = playbackRate;

    if (m_modelPlayer)
        m_modelPlayer->setPlaybackRate(playbackRate, [&](double) { });
}

double HTMLModelElement::duration() const
{
    return m_modelPlayer ? m_modelPlayer->duration() : 0;
}

bool HTMLModelElement::paused() const
{
    return m_modelPlayer ? m_modelPlayer->paused() : true;
}

void HTMLModelElement::play(DOMPromiseDeferred<void>&& promise)
{
    setPaused(false, WTFMove(promise));
}

void HTMLModelElement::pause(DOMPromiseDeferred<void>&& promise)
{
    setPaused(true, WTFMove(promise));
}

void HTMLModelElement::setPaused(bool paused, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setPaused(paused, [promise = WTFMove(promise)] (bool succeeded) mutable {
        if (succeeded)
            promise.resolve();
        else
            promise.reject();
    });
}

bool HTMLModelElement::autoplay() const
{
    return hasAttributeWithoutSynchronization(HTMLNames::autoplayAttr);
}

void HTMLModelElement::updateAutoplay()
{
    if (m_modelPlayer)
        m_modelPlayer->setAutoplay(autoplay());
}

WebCore::StageModeOperation HTMLModelElement::stageMode() const
{
    String attr = attributeWithoutSynchronization(HTMLNames::stagemodeAttr);
    if (equalLettersIgnoringASCIICase(attr, "orbit"_s))
        return WebCore::StageModeOperation::Orbit;

    return WebCore::StageModeOperation::None;
}

void HTMLModelElement::updateStageMode()
{
    if (m_modelPlayer)
        m_modelPlayer->setStageMode(stageMode());
}

bool HTMLModelElement::loop() const
{
    return hasAttributeWithoutSynchronization(HTMLNames::loopAttr);
}

void HTMLModelElement::updateLoop()
{
    if (m_modelPlayer)
        m_modelPlayer->setLoop(loop());
}

double HTMLModelElement::currentTime() const
{
    return m_modelPlayer ? m_modelPlayer->currentTime().seconds() : 0;
}

void HTMLModelElement::setCurrentTime(double currentTime)
{
    if (m_modelPlayer)
        m_modelPlayer->setCurrentTime(Seconds(currentTime), [&]() { });
}

bool HTMLModelElement::hasPortal() const
{
#if PLATFORM(VISION)
    return !(document().settings().modelNoPortalAttributeEnabled() && hasAttributeWithoutSynchronization(HTMLNames::noportalAttr));
#else
    return true;
#endif
}

void HTMLModelElement::updateHasPortal()
{
    if (CheckedPtr renderer = this->renderer())
        renderer->updateFromElement();

    if (RefPtr modelPlayer = m_modelPlayer)
        modelPlayer->setHasPortal(hasPortal());
}

const URL& HTMLModelElement::environmentMap() const
{
    return m_environmentMapURL;
};

void HTMLModelElement::setEnvironmentMap(const URL& url)
{
    if (url.string() == m_environmentMapURL.string())
        return;

    m_environmentMapURL = url;
    m_environmentMapDataMemoryCost.store(0, std::memory_order_relaxed);

    environmentMapResetAndReject(Exception { ExceptionCode::AbortError });
    m_environmentMapReadyPromise = makeUniqueRef<EnvironmentMapPromise>();

    if (m_environmentMapURL.isEmpty()) {
        // sending a message with empty data to indicate resource removal
        if (m_modelPlayer)
            m_modelPlayer->setEnvironmentMap(SharedBuffer::create());
        reportExtraMemoryCost();
        return;
    }

    environmentMapRequestResource();
}

void HTMLModelElement::updateEnvironmentMap()
{
    setEnvironmentMap(selectEnvironmentMapURL());
}

URL HTMLModelElement::selectEnvironmentMapURL() const
{
    if (!document().hasBrowsingContext())
        return { };

    if (hasAttributeWithoutSynchronization(environmentmapAttr)) {
        const auto& attr = attributeWithoutSynchronization(environmentmapAttr).string().trim(isASCIIWhitespace);
        if (StringView(attr).containsOnly<isASCIIWhitespace<UChar>>())
            return { };
        return getURLAttribute(environmentmapAttr);
    }

    return { };
}

void HTMLModelElement::environmentMapRequestResource()
{
    auto request = createResourceRequest(m_environmentMapURL, FetchOptions::Destination::Environmentmap);
    auto resource = document().protectedCachedResourceLoader()->requestEnvironmentMapResource(WTFMove(request));
    if (!resource.has_value()) {
        if (!m_environmentMapReadyPromise->isFulfilled())
            m_environmentMapReadyPromise->reject(Exception { ExceptionCode::NetworkError });
        // sending a message with empty data to indicate resource removal
        if (m_modelPlayer)
            m_modelPlayer->setEnvironmentMap(SharedBuffer::create());
        return;
    }

    m_environmentMapData.empty();

    m_environmentMapResource = resource.value();
    m_environmentMapResource->addClient(*this);
}

void HTMLModelElement::environmentMapResetAndReject(Exception&& exception)
{
    m_environmentMapData.reset();

    if (m_environmentMapResource) {
        m_environmentMapResource->removeClient(*this);
        m_environmentMapResource = nullptr;
    }

    if (!m_environmentMapReadyPromise->isFulfilled())
        m_environmentMapReadyPromise->reject(WTFMove(exception));
}

void HTMLModelElement::environmentMapResourceFinished()
{
    int status = m_environmentMapResource->response().httpStatusCode();
    if (m_environmentMapResource->loadFailedOrCanceled() || (status && (status < 200 || status > 299))) {
        environmentMapResetAndReject(Exception { ExceptionCode::NetworkError });

        // sending a message with empty data to indicate resource removal
        if (m_modelPlayer)
            m_modelPlayer->setEnvironmentMap(SharedBuffer::create());
        return;
    }
    if (m_modelPlayer) {
        m_environmentMapDataMemoryCost.store(m_environmentMapData.size(), std::memory_order_relaxed);
        m_modelPlayer->setEnvironmentMap(m_environmentMapData.takeAsContiguous().get());
    }

    m_environmentMapResource->removeClient(*this);
    m_environmentMapResource = nullptr;
}

#endif // ENABLE(MODEL_PROCESS)

void HTMLModelElement::modelResourceFinished()
{
    auto invalidateResourceHandleAndUpdateRenderer = [&] {
        m_resource->removeClient(*this);
        m_resource = nullptr;

        if (CheckedPtr renderer = this->renderer())
            renderer->updateFromElement();
    };

    if (m_resource->loadFailedOrCanceled()) {
        m_data.reset();

        ActiveDOMObject::queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));

        invalidateResourceHandleAndUpdateRenderer();

        if (!m_readyPromise->isFulfilled())
            m_readyPromise->reject(Exception { ExceptionCode::NetworkError });
        return;
    }

    m_dataComplete = true;
    m_dataMemoryCost.store(m_data.size(), std::memory_order_relaxed);
    m_model = Model::create(m_data.takeAsContiguous().get(), m_resource->mimeType(), m_resource->url());

    ActiveDOMObject::queueTaskToDispatchEvent(*this, TaskSource::DOMManipulation, Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));

    invalidateResourceHandleAndUpdateRenderer();

    modelDidChange();
}

void HTMLModelElement::isPlayingAnimation(IsPlayingAnimationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> isPlaying) mutable {
        if (!isPlaying)
            promise.reject();
        else
            promise.resolve(*isPlaying);
    });
}

void HTMLModelElement::setAnimationIsPlaying(bool isPlaying, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setAnimationIsPlaying(isPlaying, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

void HTMLModelElement::playAnimation(DOMPromiseDeferred<void>&& promise)
{
    setAnimationIsPlaying(true, WTFMove(promise));
}

void HTMLModelElement::pauseAnimation(DOMPromiseDeferred<void>&& promise)
{
    setAnimationIsPlaying(false, WTFMove(promise));
}

void HTMLModelElement::isLoopingAnimation(IsLoopingAnimationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isLoopingAnimation([promise = WTFMove(promise)] (std::optional<bool> isLooping) mutable {
        if (!isLooping)
            promise.reject();
        else
            promise.resolve(*isLooping);
    });
}

void HTMLModelElement::setIsLoopingAnimation(bool isLooping, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setIsLoopingAnimation(isLooping, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

void HTMLModelElement::animationDuration(DurationPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->animationDuration([promise = WTFMove(promise)] (std::optional<Seconds> duration) mutable {
        if (!duration)
            promise.reject();
        else
            promise.resolve(duration->seconds());
    });
}

void HTMLModelElement::animationCurrentTime(CurrentTimePromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->animationCurrentTime([promise = WTFMove(promise)] (std::optional<Seconds> currentTime) mutable {
        if (!currentTime)
            promise.reject();
        else
            promise.resolve(currentTime->seconds());
    });
}

void HTMLModelElement::setAnimationCurrentTime(double currentTime, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setAnimationCurrentTime(Seconds(currentTime), [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

// MARK: - Audio support.

void HTMLModelElement::hasAudio(HasAudioPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> hasAudio) mutable {
        if (!hasAudio)
            promise.reject();
        else
            promise.resolve(*hasAudio);
    });
}

void HTMLModelElement::isMuted(IsMutedPromise&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->isPlayingAnimation([promise = WTFMove(promise)] (std::optional<bool> isMuted) mutable {
        if (!isMuted)
            promise.reject();
        else
            promise.resolve(*isMuted);
    });
}

void HTMLModelElement::setIsMuted(bool isMuted, DOMPromiseDeferred<void>&& promise)
{
    if (!m_modelPlayer) {
        promise.reject();
        return;
    }

    m_modelPlayer->setIsMuted(isMuted, [promise = WTFMove(promise)] (bool success) mutable {
        if (success)
            promise.resolve();
        else
            promise.reject();
    });
}

bool HTMLModelElement::virtualHasPendingActivity() const
{
    // We need to ensure the JS wrapper is kept alive if a load is in progress and we may yet dispatch
    // "load" or "error" events, ie. as long as we have a resource, meaning we are in the process of loading.
    return m_resource;
}

void HTMLModelElement::stop()
{
    RELEASE_LOG(ModelElement, "%p - HTMLModelElement::stop()", this);

    // Once an active DOM object has been stopped it cannot be restarted,
    // so we can delete the model player now.
    deleteModelPlayer();
}

#if PLATFORM(COCOA)
Vector<RetainPtr<id>> HTMLModelElement::accessibilityChildren()
{
    if (!m_modelPlayer)
        return { };
    return m_modelPlayer->accessibilityChildren();
}
#endif

LayoutSize HTMLModelElement::contentSize() const
{
    if (CheckedPtr renderer = this->renderer())
        return downcast<RenderReplaced>(*renderer).replacedContentRect().size();

    return LayoutSize();
}

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
String HTMLModelElement::inlinePreviewUUIDForTesting() const
{
    if (!m_modelPlayer)
        return emptyString();
    return m_modelPlayer->inlinePreviewUUIDForTesting();
}
#endif

void HTMLModelElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == widthAttr) {
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(value, attributeWithoutSynchronization(heightAttr), style);
    } else if (name == heightAttr) {
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
        applyAspectRatioFromWidthAndHeightAttributesToStyle(attributeWithoutSynchronization(widthAttr), value, style);
    } else
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
}

bool HTMLModelElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr)
        return true;
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

bool HTMLModelElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr
#if ENABLE(MODEL_PROCESS)
        || attribute.name() == environmentmapAttr
#endif
        || HTMLElement::isURLAttribute(attribute);
}

Node::InsertedIntoAncestorResult HTMLModelElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    auto insertResult = HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (insertionType.connectedToDocument) {
        document().registerForVisibilityStateChangedCallbacks(*this);
        m_modelPlayerProvider = document().page()->modelPlayerProvider();
    }

    return insertResult;
}

void HTMLModelElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (removalType.disconnectedFromDocument) {
        document().unregisterForVisibilityStateChangedCallbacks(*this);
        deleteModelPlayer();
    }
}

void HTMLModelElement::reportExtraMemoryCost()
{
    const size_t currentCost = memoryCost();
    if (m_reportedDataMemoryCost < currentCost) {
        auto* context = Node::scriptExecutionContext();
        if (!context)
            return;
        JSC::VM& vm = context->vm();
        JSC::JSLockHolder lock(vm);
        ASSERT_WITH_MESSAGE(vm.currentThreadIsHoldingAPILock(), "Extra memory reporting expects to happen from one thread");
        vm.heap.reportExtraMemoryAllocated(nullptr, currentCost - m_reportedDataMemoryCost);
        m_reportedDataMemoryCost = currentCost;
    }
}

size_t HTMLModelElement::memoryCost() const
{
    // May be called from GC threads.
    auto cost = m_dataMemoryCost.load(std::memory_order_relaxed);
#if ENABLE(MODEL_PROCESS)
    cost += m_environmentMapDataMemoryCost.load(std::memory_order_relaxed);
#endif
    return cost;
}

#if ENABLE(RESOURCE_USAGE)
size_t HTMLModelElement::externalMemoryCost() const
{
    // For the purposes of Web Inspector, external memory means memory reported as
    // 1) being traceable from JS objects, i.e. GC owned memory
    // 2) not allocated from "Page" category, e.g. from bmalloc.
    return memoryCost();
}
#endif

}

#endif // ENABLE(MODEL_ELEMENT)
