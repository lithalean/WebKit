/*
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "DOMSelection.h"

#include "CommonAtomStrings.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "Editing.h"
#include "FrameSelection.h"
#include "LocalFrame.h"
#include "NodeInlines.h"
#include "Quirks.h"
#include "Range.h"
#include "ShadowRoot.h"
#include "StaticRange.h"
#include "TextIterator.h"

namespace WebCore {

static RefPtr<Node> selectionShadowAncestor(LocalFrame& frame)
{
    RefPtr node = frame.selection().selection().base().anchorNode();
    if (!node || !node->isInShadowTree())
        return nullptr;
    return node->protectedDocument()->ancestorNodeInThisScope(node.get());
}

DOMSelection::DOMSelection(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

Ref<DOMSelection> DOMSelection::create(LocalDOMWindow& window)
{
    return adoptRef(*new DOMSelection(window));
}

RefPtr<LocalFrame> DOMSelection::frame() const
{
    return LocalDOMWindowProperty::frame();
}

std::optional<SimpleRange> DOMSelection::range() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return std::nullopt;
    auto range = frame->selection().selection().range();
    if (!range || range->start.container->isInShadowTree())
        return std::nullopt;
    return range;
}

Position DOMSelection::anchorPosition() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    return frame->selection().selection().anchor();
}

Position DOMSelection::focusPosition() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    return frame->selection().selection().focus();
}

RefPtr<Node> DOMSelection::anchorNode() const
{
    return shadowAdjustedNode(anchorPosition());
}

unsigned DOMSelection::anchorOffset() const
{
    return shadowAdjustedOffset(anchorPosition());
}

RefPtr<Node> DOMSelection::focusNode() const
{
    return shadowAdjustedNode(focusPosition());
}

unsigned DOMSelection::focusOffset() const
{
    return shadowAdjustedOffset(focusPosition());
}

bool DOMSelection::isCollapsed() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return true;
    auto range = this->range();
    return !range || range->collapsed();
}

String DOMSelection::type() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return "None"_s;
    auto range = frame->selection().selection().range();
    if (!range)
        return "None"_s;
    if (range->collapsed())
        return "Caret"_s;
    return "Range"_s;
}

String DOMSelection::direction() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return noneAtom();
    auto& selection = frame->selection().selection();
    // FIXME: This can return a direction for a caret, which does not make logical sense.
    if (selection.directionality() == Directionality::None || selection.isNone())
        return noneAtom();
    return selection.isBaseFirst() ? "forward"_s : "backward"_s;
}

unsigned DOMSelection::rangeCount() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return 0;
    if (frame->selection().associatedLiveRange())
        return 1;
    if (selectionShadowAncestor(*frame))
        return 1;
    return 0;
}

ExceptionOr<void> DOMSelection::collapse(Node* node, unsigned offset)
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    if (!node) {
        removeAllRanges();
        return { };
    }
    if (auto result = Range::checkNodeOffsetPair(*node, offset); result.hasException())
        return result.releaseException();
    if (!(node->isConnected() && frame->document() == &node->document()) && &node->rootNode() != frame->document())
        return { };
    CheckedRef selection = frame->selection();
    selection->disassociateLiveRange();
    selection->moveTo(makeContainerOffsetPosition(node, offset), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToEnd()
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    CheckedRef selection = frame->selection();
    if (selection->isNone())
        return Exception { ExceptionCode::InvalidStateError };
    selection->disassociateLiveRange();
    selection->moveTo(selection->selection().uncanonicalizedEnd(), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::collapseToStart()
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    CheckedRef selection = frame->selection();
    if (selection->isNone())
        return Exception { ExceptionCode::InvalidStateError };
    selection->disassociateLiveRange();
    selection->moveTo(selection->selection().uncanonicalizedStart(), Affinity::Downstream);
    return { };
}

void DOMSelection::empty()
{
    removeAllRanges();
}

ExceptionOr<void> DOMSelection::setBaseAndExtent(Node& anchorNode, unsigned anchorOffset, Node& focusNode, unsigned focusOffset)
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    if (auto result = Range::checkNodeOffsetPair(anchorNode, anchorOffset); result.hasException())
        return result.releaseException();
    if (auto result = Range::checkNodeOffsetPair(focusNode, focusOffset); result.hasException())
        return result.releaseException();
    Ref document = *frame->document();
    if (!document->isShadowIncludingInclusiveAncestorOf(&anchorNode) || !document->isShadowIncludingInclusiveAncestorOf(&focusNode))
        return { };
    CheckedRef selection = frame->selection();
    selection->disassociateLiveRange();
    selection->moveTo(makeContainerOffsetPosition(&anchorNode, anchorOffset), makeContainerOffsetPosition(&focusNode, focusOffset), Affinity::Downstream);
    return { };
}

ExceptionOr<void> DOMSelection::setPosition(Node* node, unsigned offset)
{
    return collapse(node, offset);
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    FrameSelection::Alteration alter;
    if (equalLettersIgnoringASCIICase(alterString, "extend"_s))
        alter = FrameSelection::Alteration::Extend;
    else if (equalLettersIgnoringASCIICase(alterString, "move"_s))
        alter = FrameSelection::Alteration::Move;
    else
        return;

    SelectionDirection direction;
    if (equalLettersIgnoringASCIICase(directionString, "forward"_s))
        direction = SelectionDirection::Forward;
    else if (equalLettersIgnoringASCIICase(directionString, "backward"_s))
        direction = SelectionDirection::Backward;
    else if (equalLettersIgnoringASCIICase(directionString, "left"_s))
        direction = SelectionDirection::Left;
    else if (equalLettersIgnoringASCIICase(directionString, "right"_s))
        direction = SelectionDirection::Right;
    else
        return;

    TextGranularity granularity;
    if (equalLettersIgnoringASCIICase(granularityString, "character"_s))
        granularity = TextGranularity::CharacterGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "word"_s))
        granularity = TextGranularity::WordGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentence"_s))
        granularity = TextGranularity::SentenceGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "line"_s))
        granularity = TextGranularity::LineGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraph"_s))
        granularity = TextGranularity::ParagraphGranularity;
    else if (equalLettersIgnoringASCIICase(granularityString, "lineboundary"_s))
        granularity = TextGranularity::LineBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "sentenceboundary"_s))
        granularity = TextGranularity::SentenceBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "paragraphboundary"_s))
        granularity = TextGranularity::ParagraphBoundary;
    else if (equalLettersIgnoringASCIICase(granularityString, "documentboundary"_s))
        granularity = TextGranularity::DocumentBoundary;
    else
        return;

    if (RefPtr frame = this->frame())
        frame->checkedSelection()->modify(alter, direction, granularity);
}

ExceptionOr<void> DOMSelection::extend(Node& node, unsigned offset)
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };

    if (rangeCount() < 1 && !(frame->selection().isCaretOrRange()))
        return Exception { ExceptionCode::InvalidStateError, "extend() requires a Range to be added to the Selection"_s };

    if (!(node.isConnected() && frame->document() == &node.document()) && &node.rootNode() != frame->document())
        return { };
    if (auto result = Range::checkNodeOffsetPair(node, offset); result.hasException())
        return result.releaseException();
    CheckedRef selection = frame->selection();
    auto newSelection = selection->selection();
    newSelection.setExtent(makeContainerOffsetPosition(&node, offset));
    selection->disassociateLiveRange();
    selection->setSelection(WTFMove(newSelection));
    return { };
}

static RefPtr<Range> createLiveRangeBeforeShadowHostWithSelection(LocalFrame& frame)
{
    if (RefPtr shadowAncestor = selectionShadowAncestor(frame))
        return createLiveRange(makeSimpleRange(*makeBoundaryPointBeforeNode(*shadowAncestor)));

    return nullptr;
}

ExceptionOr<Ref<Range>> DOMSelection::getRangeAt(unsigned index)
{
    if (index >= rangeCount())
        return Exception { ExceptionCode::IndexSizeError };
    Ref frame = this->frame().releaseNonNull();
    if (RefPtr liveRange = frame->selection().associatedLiveRange())
        return liveRange.releaseNonNull();
    return createLiveRangeBeforeShadowHostWithSelection(frame.get()).releaseNonNull();
}

void DOMSelection::removeAllRanges()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;
    frame->checkedSelection()->clear();
}

void DOMSelection::addRange(Range& liveRange)
{
    RefPtr frame = this->frame();
    if (!frame)
        return;
    CheckedRef selection = frame->selection();
    if (selection->isNone())
        selection->associateLiveRange(liveRange);
}

ExceptionOr<void> DOMSelection::removeRange(Range& liveRange)
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    if (&liveRange != frame->selection().associatedLiveRange())
        return Exception { ExceptionCode::NotFoundError };
    removeAllRanges();
    return { };
}

Vector<Ref<StaticRange>> DOMSelection::getComposedRanges(std::optional<Variant<RefPtr<ShadowRoot>, GetComposedRangesOptions>>&& firstShadowRootOrOptions, FixedVector<std::reference_wrapper<ShadowRoot>>&& remainingShadowRoots)
{
    RefPtr frame = this->frame();
    if (!frame)
        return { };
    auto range = frame->selection().selection().range();
    if (!range)
        return { };

    UncheckedKeyHashSet<Ref<ShadowRoot>> shadowRootSet;
    if (firstShadowRootOrOptions) {
        if (auto* firstShadowRoot = std::get_if<RefPtr<ShadowRoot>>(&*firstShadowRootOrOptions)) {
            shadowRootSet.reserveInitialCapacity(remainingShadowRoots.size() + 1);
            shadowRootSet.add(firstShadowRoot->releaseNonNull());
            for (auto& root : remainingShadowRoots)
                shadowRootSet.add(root.get());
        } else {
            auto* options = std::get_if<GetComposedRangesOptions>(&*firstShadowRootOrOptions);
            RELEASE_ASSERT(options);
            for (auto& shadowRoot : options->shadowRoots)
                shadowRootSet.add(WTFMove(shadowRoot));
        }
    }

    Ref startNode = range->startContainer();
    unsigned startOffset = range->startOffset();
    while (startNode->isInShadowTree() && !shadowRootSet.contains(startNode->protectedContainingShadowRoot().get())) {
        RefPtr host = startNode->shadowHost();
        ASSERT(host && host->parentNode());
        startNode = *host->parentNode();
        startOffset = host->computeNodeIndex();
    }

    Ref endNode = range->endContainer();
    unsigned endOffset = range->endOffset();
    while (endNode->isInShadowTree() && !shadowRootSet.contains(endNode->protectedContainingShadowRoot().get())) {
        RefPtr host = endNode->shadowHost();
        ASSERT(host && host->parentNode());
        endNode = *host->parentNode();
        endOffset = host->computeNodeIndex() + 1;
    }

    return { StaticRange::create(SimpleRange { BoundaryPoint { WTFMove(startNode), startOffset }, BoundaryPoint { WTFMove(endNode), endOffset } }) };
}

void DOMSelection::deleteFromDocument()
{
    RefPtr frame = this->frame();
    if (!frame)
        return;
    if (RefPtr range = frame->selection().associatedLiveRange())
        range->deleteContents();
}

bool DOMSelection::containsNode(Node& node, bool allowPartial) const
{
    auto range = this->range();
    return range && (allowPartial ? intersects(*range, node) : contains(*range, node));
}

ExceptionOr<void> DOMSelection::selectAllChildren(Node& node)
{
    // This doesn't (and shouldn't) select the characters in a node if passed a text node.
    // Selection API specification seems to have this wrong: https://github.com/w3c/selection-api/issues/125
    return setBaseAndExtent(node, 0, node, node.countChildNodes());
}

String DOMSelection::toString() const
{
    RefPtr frame = this->frame();
    if (!frame)
        return String();

    OptionSet<TextIteratorBehavior> options;
    if (!frame->document()->quirks().needsToCopyUserSelectNoneQuirk())
        options.add(TextIteratorBehavior::IgnoresUserSelectNone);

    auto range = frame->selection().selection().range();
    return range ? plainText(*range, options) : emptyString();
}

RefPtr<Node> DOMSelection::shadowAdjustedNode(const Position& position) const
{
    if (position.isNull())
        return nullptr;

    RefPtr containerNode = position.containerNode();
    RefPtr adjustedNode = frame()->protectedDocument()->ancestorNodeInThisScope(containerNode.get());
    if (!adjustedNode)
        return nullptr;

    if (containerNode == adjustedNode)
        return containerNode;

    return adjustedNode->parentNodeGuaranteedHostFree();
}

unsigned DOMSelection::shadowAdjustedOffset(const Position& position) const
{
    if (position.isNull())
        return 0;

    RefPtr containerNode = position.containerNode();
    RefPtr adjustedNode = frame()->protectedDocument()->ancestorNodeInThisScope(containerNode.get());
    if (!adjustedNode)
        return 0;

    if (containerNode == adjustedNode)
        return position.computeOffsetInContainerNode();

    return adjustedNode->computeNodeIndex();
}

} // namespace WebCore
