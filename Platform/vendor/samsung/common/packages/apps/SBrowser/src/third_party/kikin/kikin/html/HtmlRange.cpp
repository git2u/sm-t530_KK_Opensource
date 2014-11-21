/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#define LOG_TAG "kikin_range"

#include "config.h"
#include "kikin/html/HtmlRange.h"

#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"

#include "core/html/HTMLAnchorElement.h"
#include "core/rendering/style/RenderStyle.h"

#include <wtf/Vector.h>
#include "core/platform/text/RegularExpression.h"

#include "kikin/list/Ranges.h"
#include "kikin/util/KikinUtils.h"
#include "kikin/list/ClientRects.h"

using namespace WebCore;

namespace kikin {

void replace(String& string, const RegularExpression& target, const String& replacement)
{
    int index = 0;
    while (index < static_cast<int>(string.length())) {
        int matchLength;
        index = target.match(string, index, &matchLength);
        if (index < 0)
            break;
        string.replace(index, matchLength, replacement);
        index += replacement.length();
        if (!matchLength)
            break;  // Avoid infinite loop on 0-length matches, e.g. [a-z]*
    }
}

// static 
HashSet<WTF::String> HtmlRange::s_ignoreTags;

// static 
PassRefPtr<HtmlRange> HtmlRange::createFromTextNode(const WTF::String& text, Node* node,
		int start, const WTF::String& type) {
	ASSERT(node);

	Document* document = node->parentElement()->document();
	ASSERT(document);

	RefPtr<WebCore::Range> range = document->createRange();
	ASSERT(range);

	int end = text.length();
	int matchLength = 0;


	RegularExpression startWhiteSpaceRegex("^(\\s+)", WTF::TextCaseInsensitive);
	int matchIndex = startWhiteSpaceRegex.match(text, 0, &matchLength);

	if (matchIndex != -1) {
		start = start + matchLength;
		end = end - matchLength;
	}

	matchLength = 0;
	RegularExpression endWhiteSpaceRegex("(\\s+)$", WTF::TextCaseInsensitive);
	matchIndex = endWhiteSpaceRegex.match(text, 0, &matchLength);

	if (matchIndex != -1) {
		end = end - matchLength;
	}

	// WARNING: startEle and endEle needs to be
	// TextNodes for SOS to work!
	ExceptionCode ec = 0;
	range->setStart(node, start, ec);
	ASSERT(!ec);

	range->setEnd(node, (start + end), ec);
	ASSERT(!ec);

	return adoptRef(new HtmlRange(range.release(), node, type));
}

// static 
WTF::PassRefPtr<HtmlRange> HtmlRange::createFromNode(Node* node, const WTF::String& type)
{
	ASSERT(node);

	Document* document = node->document();
	ASSERT(document);

	RefPtr<WebCore::Range> range = document->createRange();
	ASSERT(range);

	Node* startElement = util::KikinUtils::findLeaf(node, true);
	Node* endElement = util::KikinUtils::findLeaf(node, false);

	ASSERT(startElement);
	ASSERT(endElement);

	// WARNING: startEle and endEle needs to be
	// TextNodes for SOS to work!
	ExceptionCode ec = 0;
	range->setStart(startElement, 0, ec);
	ASSERT(!ec);

	range->setEnd(endElement, endElement->nodeValue().length(), ec);
	ASSERT(!ec);

	return adoptRef(new HtmlRange(range.release(), node, type));
}

// static 
PassRefPtr<list::Ranges> HtmlRange::createRangesFromStartingPoint(Node* startContainer, const int offset, const int noChars)
{
	RefPtr<list::Ranges> ranges = list::Ranges::create();

	if (!startContainer) {
		return ranges.release();
	}

	const String& textContent = startContainer->textContent();
	HashMap<String, int>* backCount = HtmlRange::countNonBlankChars(textContent, offset, false, noChars);
	HashMap<String, int>* forwardCount = HtmlRange::countNonBlankChars(textContent, offset, true, noChars);

	int remainingChars = backCount->get("remainingChars");
	int startOffset = backCount->get("startOffset");
	int endOffset = backCount->get("endOffset");
	String textSubStr = textContent.substring(startOffset, (endOffset - startOffset));

	remainingChars = forwardCount->get("remainingChars");
	startOffset = forwardCount->get("startOffset");
	endOffset = forwardCount->get("endOffset");
	textSubStr = textContent.substring(startOffset, (endOffset - startOffset));

	if (backCount->get("remainingChars") > 0) {
		ranges->append(HtmlRange::createRangesFromStartingPoint(startContainer, false, backCount->get("remainingChars")));
	}

	ranges->append(HtmlRange::createRange(startContainer, backCount->get("startOffset"), forwardCount->get("endOffset")));

	if (forwardCount->get("remainingChars") > 0) {
		ranges->append(HtmlRange::createRangesFromStartingPoint(startContainer, true, forwardCount->get("remainingChars")));
	}

	// delete the instances of hashmap we have created
	delete backCount;
	delete forwardCount;

	return ranges.release();
}

// static
HashMap<String, int>* HtmlRange::countNonBlankChars(const WTF::String& text, int offset, bool forward, int noChars)
{
	int direction = forward ? 1 : -1;
	int length = text.length();
	int originalOffset = offset;

	// only start traversing if we are not on the edges
	if (!((forward && length == offset) ||
			(!forward && offset == 0)))
	{
		do {
			if (text[offset] != ' ')
				noChars--;
			offset = offset + direction;
		} while (offset > 0 && offset < length && noChars > 0);
	}

	HashMap<String, int>* result = new HashMap<String, int>();
	result->set("startOffset", (forward ? originalOffset : offset));
	result->set("endOffset", (forward ? offset : originalOffset));
	result->set("remainingChars", noChars);

	return result;
}

// static 
void HtmlRange::createIgnoreTagsList()
{
	s_ignoreTags.add("script");
	s_ignoreTags.add("style");
	s_ignoreTags.add("noscript");
	s_ignoreTags.add("submit");
	s_ignoreTags.add("input");
	s_ignoreTags.add("textarea");
	s_ignoreTags.add("title");
	s_ignoreTags.add("iframe");
}

// static 
PassRefPtr<list::Ranges> HtmlRange::createRangesFromStartingPoint(Node* currentContainer, bool forward, int noChars)
{
	int j = 0;

	RefPtr<list::Ranges> ranges = list::Ranges::create();

	while (j < 10 && noChars > 0) {
		if (forward) {
			currentContainer = util::KikinUtils::getNextTextNode(currentContainer, true);
		} else {
			currentContainer = util::KikinUtils::getPreviousTextNode(currentContainer, true);
		}

		if (!currentContainer) {
			break;
		}

		if (s_ignoreTags.isEmpty())
			HtmlRange::createIgnoreTagsList();

		Element* parentElement = (currentContainer->parentElement() ? currentContainer->parentElement() :
			(currentContainer->parentNode() ? currentContainer->parentNode()->parentElement() : 0));

		if (parentElement && s_ignoreTags.contains(parentElement->tagName().lower()))
			continue;

		const String& nodeValue = currentContainer->nodeValue();
		if (currentContainer->isTextNode()) {
			int matchLength = 0;

			RegularExpression whiteSpaceRegex("^(\\s+)$", WTF::TextCaseInsensitive);
			int matchIndex = whiteSpaceRegex.match(nodeValue, 0, &matchLength);

			if (matchIndex == -1) {    // Regex should fail
				j++;

				HashMap<String, int>* count = HtmlRange::countNonBlankChars(nodeValue, (forward ? 0 : nodeValue.length()), forward, noChars);

				int remainingChars = count->get("remainingChars");
				int startOffset = count->get("startOffset");
				int endOffset = count->get("endOffset");
				String textSubStr = nodeValue.substring(startOffset, (endOffset - startOffset));

				if (count->get("remainingChars") > 0) {
					RefPtr<HtmlRange> range = HtmlRange::createRange(currentContainer, 0, nodeValue.length());
					if (forward) {
						ranges->append(range.release());
					} else {
						ranges->insert(range.release(), 0);
					}

					noChars = count->get("remainingChars");
				} else {
					RefPtr<HtmlRange> range = HtmlRange::createRange(currentContainer, count->get("startOffset"), count->get("endOffset"));
					if (forward) {
						ranges->append(range.release());
					} else {
						ranges->insert(range.release(), 0);
					}

					delete count;
					break;
				}

				delete count;
			}
		}
	}

	return ranges.release();
}

// static 
PassRefPtr<HtmlRange> HtmlRange::createRange(Node* container, int startOffset, int endOffset)
{
	ASSERT(container);

	Document* document = container->document();
	ASSERT(document);

	RefPtr<WebCore::Range> range = document->createRange();
	ASSERT(range);

	ExceptionCode ec = 0;
	range->setStart(container, startOffset, ec);
	ASSERT(!ec);

	range->setEnd(container, endOffset, ec);
	ASSERT(!ec);

	return adoptRef(new HtmlRange(range.release(), container, "parser"));
}

// static 
PassRefPtr<HtmlRange> HtmlRange::create(WTF::PassRefPtr<WebCore::Range> range, WebCore::Node* node, const WTF::String& type)
{
	ASSERT(range);
	ASSERT(node);
	return adoptRef(new HtmlRange(range, node, type));
}

HtmlRange::HtmlRange(WTF::PassRefPtr<WebCore::Range> range, Node* node, const WTF::String& type) : m_range(range),
		m_node(node),
		m_type(type),
		m_previousText(),
		m_postText(),
		m_zoom(1.0f)
{
	if (m_node) {
		RenderStyle* rendererStyle = m_node->renderStyle();
		if (rendererStyle) {
			m_zoom = rendererStyle->effectiveZoom();
		}
	}
}

HtmlRange::~HtmlRange()
{
}

WebCore::Range* HtmlRange::getRange() 
{ 
	return m_range.get();
}

WTF::PassRefPtr<ClientRect> HtmlRange::getZoomAdjustedClientRect(WTF::PassRefPtr<ClientRect> rect)
{
	if (!rect) 
	{
		return rect;
	}

	FloatRect floatRect((rect->left() * m_zoom), 
			(rect->top() * m_zoom),
			(rect->width() * m_zoom),
			(rect->height() * m_zoom));

	return ClientRect::create(floatRect);
}

WTF::PassRefPtr<list::ClientRects> HtmlRange::getClientRects()
{
	RefPtr<list::ClientRects> rects = list::ClientRects::create();

	// simple life: only one rect anyway..?
	if (m_range->startContainer()->isSameNode(m_range->endContainer()) &&
			m_range->getClientRects()->length() == 1)
	{
		RefPtr<ClientRect> rect = getZoomAdjustedClientRect(m_range->getBoundingClientRect());

		ExceptionCode ec = 0;
		if (rect && !m_range->toString(ec).isEmpty() && !ec && rect->width() > 0 && rect->height() > 0) {
			rects->append(rect.release());
		}

		return rects.release();
	}

	// get all text nodes in the global range
	ExceptionCode ec = 0;
	Node* commonAncestorContainer = m_range->commonAncestorContainer(ec);
	ASSERT(!ec);

	Vector<Node*> textNodes = util::KikinUtils::getTextNodesBetween(commonAncestorContainer, m_range->startContainer(), m_range->endContainer());
	for (unsigned i = 0; i < textNodes.size(); i++) {
		Node* textNode = textNodes[i];
		int start = ((i == 0) ? m_range->startOffset() : 0);
		int end = (m_range->endContainer()->isSameNode(textNode) ? m_range->endOffset() : -1);

		int numberOfRects = rects->length();

		rects->append(getRectsForTextNode(textNode, start, end));
	}

	return rects.release();
}

// static 
bool HtmlRange::isWhiteSpaceOrDelimeter(UChar c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '-' || c == 0x2014 || c == 0x00A0; // the second dash sign is special! do not remove it
}

PassRefPtr<list::ClientRects> HtmlRange::getRectsForTextNode(Node* node, int start, int end)
{
	RefPtr<list::ClientRects> ret = list::ClientRects::create();
	const String& nodeValue = node->nodeValue();

	end = ((end == -1 && !nodeValue.isEmpty()) ? (nodeValue.length()) : end);
	int j = start;

	// empty text
	if (j >= end || nodeValue.isEmpty()) {
		return ret.release();
	}

	Vector<String> texts;
	Vector<int> textStarts;
	Vector<int> textEnds;

	RegularExpression nonWordCharacterSearchRegex("[\\s\\–\\u2014\\u00A0]|$", WTF::TextCaseInsensitive);

	// cut the text in parts to analyse
	while (j < end) {
		// search for the next space or delimiter
		int matchLength = 0;
		int found = nonWordCharacterSearchRegex.match(nodeValue, j, &matchLength);

		if (found > -1) {
			// add the word
			if (found > end) {
				found = end;
			}

			texts.append(nodeValue.substring(j, (found - j)));
			textStarts.append(j);
			textEnds.append(found);

			j = found;

			if (j < end) {
				texts.append(nodeValue.substring(j, 1));
				textStarts.append(j);
				textEnds.append(++j);
			}

		} else {
			texts.append(nodeValue.substring(j, (end - j)));
			textStarts.append(j);
			textEnds.append(end);

			j = end;

			break;
		}
	}

	int curIdx = 0;
	String text = texts[curIdx];
	int textStart = textStarts[curIdx];
	int textEnd = textEnds[curIdx];

	ASSERT(node);

	Document* document = node->document();
	ASSERT(document);

	RefPtr<WebCore::Range> range = document->createRange();
	ASSERT(range);

	ExceptionCode ec = 0;
	range->setStart(node, textStart, ec);
	ASSERT(!ec);

	// get rects associated with those parts
	for (size_t i = 1; i < texts.size(); i++) {
		text = texts[i];
		textStart = textStarts[i];
		textEnd = textEnds[i];

		range->setEnd(node, textStart, ec);
		ASSERT(!ec);

		RefPtr<list::ClientRects> rects = getCleanedClientRects(range);

		if (rects->length() > 1) {
			String curText = "";
			for (unsigned k = curIdx; k < i - 1; k++) {
				curText.append(texts[k]);
			}

			// remove the last non visible character of a sentence (not in the rect)
			UChar lastCharacter = curText[curText.length() - 1];
			if ((curText.length() > 1 && U16_IS_SINGLE(curText.length() - 2)) && (lastCharacter == ' ' ||
					lastCharacter == '\n' || lastCharacter == '\r' || lastCharacter == '\t'))
			{
				curText = curText.left(curText.length() - 1);
			}

			// Don't push empty rects
			if (!curText.isEmpty()) {
				ret->append(rects->item(0));
			}

			curIdx = i - 1;
			range->setStart(node, textStarts[curIdx], ec);
			ASSERT(!ec);
		}
	}

	// add last text
	range->setEnd(node, textEnd, ec);
	ASSERT(!ec);

	RefPtr<list::ClientRects> rects = getCleanedClientRects(range);
	if (rects->length() == 1) {
		ClientRect* rect = rects->item(0);
		for (unsigned k = 0; ((k < rects->length()) && (rect->width() == 0)); k++) {
			rect = rects->item(k);
		}

		String curText = "";
		for (unsigned k = curIdx; k < texts.size(); k++) {
			curText.append(texts[k]);
		}

		if (!curText.stripWhiteSpace().isEmpty()) {
			ret->append(rects->item(0));
		}

	} else if (rects->length() > 1) {

		String curText = "";
		for (unsigned k = curIdx; k < texts.size() - 1; k++) {
			curText.append(texts[k]);
		}

		if (!curText.isEmpty()) {
			ret->append(rects->item(0));
		}

		curText = texts[texts.size() - 1];
		if (!curText.stripWhiteSpace().isEmpty()) {
			ret->append(rects->item(rects->length() - 1));
		}
	}

	return ret.release();
}

PassRefPtr<list::ClientRects> HtmlRange::getCleanedClientRects(PassRefPtr<WebCore::Range> range)
{
	ASSERT(range);
	RefPtr<list::ClientRects> ret = list::ClientRects::create();
	RefPtr<ClientRectList> rects = range->getClientRects();

	for (unsigned i = 0; i < rects->length(); i++) {
		ClientRect* rect = rects->item(i);
		if (rect->width() > 0 && rect->height() > 0) {
			ret->append(getZoomAdjustedClientRect(rect));
		}
	}

	return ret.release();
}

const WTF::String HtmlRange::getText() const
{
	ExceptionCode ec = 0;
	return m_range->toString(ec);
}

const WTF::String HtmlRange::getTag()
{
	Element* elem = getElement();
	if (elem)
		return elem->tagName();

	return WTF::String();
}

const WTF::String HtmlRange::getAlt()
{
	Element* elem = getElement();
	if (elem)
		return elem->title();

	return WTF::String();
}

const WTF::String HtmlRange::getHref()
{
	Element* elem = getElement();
	if (elem) {
		if (elem->parentNode() && elem->parentNode()->parentElement() && (elem->parentNode()->parentElement()->tagName().lower() == "a")) {
			WebCore::HTMLAnchorElement* aElem = static_cast<WebCore::HTMLAnchorElement*>(elem->parentNode()->parentElement());
			if (!aElem->href().isEmpty()) {
				return aElem->href();
			}
		}

		if ((elem->tagName().lower() == "a")) {
			WebCore::HTMLAnchorElement* aElem = static_cast<WebCore::HTMLAnchorElement*>(elem);
			if (!aElem->href().isEmpty()) {
				return aElem->href();
			}
		}
	}

	return WTF::String();
}

WebCore::Element* HtmlRange::getElement()
{
	return m_node->parentNode()->parentElement();
}

bool HtmlRange::shouldIgnoreCharacter(UChar32 character)
{
	return (character == ' ' || character == '\r' || character == '\n' ||
			character == 0x000A || character == 0x000D || character == 0x0009);
}

void HtmlRange::extendSelectionToText(const WTF::String& extendedText, bool beforeRange)
{
	Node* rangeNode = (beforeRange ? m_range->startContainer() : m_range->endContainer());
	String exText(extendedText);

	// remove spaces from exText and postText
	exText = exText.stripWhiteSpace();
	exText.replace(" ", "").replace(0x000A, "");

	if (beforeRange) {
		int offset = m_range->startOffset();
		String text = ((offset == 0) ? rangeNode->nodeValue() : rangeNode->nodeValue().left(offset));
		Node* node = rangeNode;
		unsigned idx = 0;

		// update start
		if (exText.length() > 0) {
			while (true) {
				offset--;

				if (offset < 0) {
					node = util::KikinUtils::getPreviousTextNode(node, true);
					if (node == NULL) {
						break;
					}

					ASSERT(node);
					text = node->nodeValue();
					offset = node->nodeValue().length();
					continue;
				}

				UChar32 textCharacter;
				if (offset - 1 >= 0) {
					textCharacter = text.characterStartingAt(offset - 1);
					if (!U16_IS_SINGLE(textCharacter)) {
						offset--;
					} else {
						textCharacter = text.characterStartingAt(offset);
					}
				} else {
					textCharacter = text.characterStartingAt(offset);
				}

				if (shouldIgnoreCharacter(textCharacter))
				{
					continue;
				}

				UChar32 preTextCharacter;
				if (exText.length() >= (idx + 2)) {
					preTextCharacter = exText.characterStartingAt(exText.length() - (idx + 2));
					if (!U16_IS_SINGLE(preTextCharacter)) {
						idx++;
					} else {
						preTextCharacter = exText.characterStartingAt(exText.length() - (idx + 1));
					}
				} else {
					preTextCharacter = exText.characterStartingAt(exText.length() - (idx + 1));
				}

				if (textCharacter != preTextCharacter) {
					break;
				}

				idx++;

				if (idx == exText.length()) {
					ExceptionCode ec = 0;
					m_range->setStart(node, offset, ec);
					ASSERT(!ec);

					break;
				}
			}
		}
	} else {
		String text = rangeNode->nodeValue();
		Node* node = rangeNode;
		unsigned idx = 0;
		int offset = m_range->endOffset() - 1;

		// update end
		if (exText.length() > 0) {
			while (true) {
				offset++;

				if ((text.length() == 0) || (offset >= static_cast<int>(node->nodeValue().length()))) {
					node = util::KikinUtils::getNextTextNode(node, true);
					if (node == NULL) {
						break;
					}

					ASSERT(node);
					text = node->nodeValue();
					offset = -1;
					continue;
				}

				UChar32 textCharacter = text.characterStartingAt(offset);
				if (shouldIgnoreCharacter(textCharacter))
				{
					continue;
				}

				UChar32 postTextCharacter = exText.characterStartingAt(idx);
				if (textCharacter != postTextCharacter) {
					break;
				}

				idx++;

				if (!U16_IS_SINGLE(textCharacter)) {
					offset++;
					idx++;
				}

				if (idx == exText.length()) {
					ExceptionCode ec = 0;
					m_range->setEnd(node, (offset + 1), ec);
					ASSERT(!ec);

					break;
				}
			}
		}
	}
}

void HtmlRange::shrinkSelection(const WTF::String& removedText, bool beforeRange)
{
	Node* rangeNode = (beforeRange ? m_range->startContainer() : m_range->endContainer());
	String textToRemove(removedText);

	// remove spaces from textToRemove
	textToRemove = textToRemove.stripWhiteSpace();
	textToRemove.replace(" ", "").replace(0x000A, "");

	if (beforeRange) {
		int nodeOffset = m_range->startOffset();
		String text = rangeNode->nodeValue().right(rangeNode->nodeValue().length() - nodeOffset);
		Node* node = rangeNode;
		unsigned idx = 0;

		// set the offset one index before the start
		int offset = -1;

		// update start
		if (textToRemove.length() > 0) {

			while (true) {
				offset++;

				if ((text.length() == 0) || (offset >= static_cast<int>(text.length()))) {
					node = util::KikinUtils::getNextTextNode(node, true);

					if (node == NULL) {
						break;
					}
					ASSERT(node);
					text = node->nodeValue();
					offset = -1;
					nodeOffset = 0;
					continue;
				}

				UChar32 textCharacter = text.characterStartingAt(offset);
				if (shouldIgnoreCharacter(textCharacter))
				{
					continue;
				}

				UChar32 postTextCharacter = textToRemove.characterStartingAt(idx);
				if (textCharacter != postTextCharacter) {
					break;
				}

				idx++;

				if (!U16_IS_SINGLE(textCharacter)) {
					offset++;
					idx++;
				}

				if (idx == textToRemove.length()) {

					// Remove all the white space characters till the next valid
					do {
						offset++;

						// if we don't have any more characters in the node get the next node
						if ((text.length() == 0) || (offset >= static_cast<int>(text.length()))) {
							node = util::KikinUtils::getNextTextNode(node, false);

							ASSERT(node);
							text = node->nodeValue();
							offset = -1;
							nodeOffset = -1;  // here we set the node offset to zero because we are not looking for the character
							// after the selection to be removed.
							continue;
						}
						textCharacter = text.characterStartingAt(offset);
					} while (textCharacter == ' ' || textCharacter == '\r' || textCharacter == '\n' ||
							textCharacter == 0x000A || textCharacter == 0x000D);

					ExceptionCode ec = 0;
					m_range->setStart(node, (offset + nodeOffset), ec);
					ASSERT(!ec);

					break;
				}
			}
		}
	} else {
		String text = rangeNode->nodeValue();
		Node* node = rangeNode;
		unsigned idx = 0;
		int offset = m_range->endOffset();

		// update end
		if (textToRemove.length() > 0) {
			while (true) {
				offset--;

				if (offset < 0) {
					node = util::KikinUtils::getPreviousTextNode(node, true);
					if (node == NULL) {
						break;
					}

					ASSERT(node);
					text = node->nodeValue();
					offset = node->nodeValue().length();
					continue;
				}

				UChar32 textCharacter;
				if (offset - 1 >= 0) {
					textCharacter = text.characterStartingAt(offset - 1);
					if (!U16_IS_SINGLE(textCharacter)) {
						offset--;
					} else {
						textCharacter = text.characterStartingAt(offset);
					}
				} else {
					textCharacter = text.characterStartingAt(offset);
				}

				if (shouldIgnoreCharacter(textCharacter))
				{
					continue;
				}

				UChar32 preTextCharacter;
				if (textToRemove.length() >= (idx + 2)) {
					preTextCharacter = textToRemove.characterStartingAt(textToRemove.length() - (idx + 2));
					if (!U16_IS_SINGLE(preTextCharacter)) {
						idx++;
					} else {
						preTextCharacter = textToRemove.characterStartingAt(textToRemove.length() - (idx + 1));
					}
				} else {
					preTextCharacter = textToRemove.characterStartingAt(textToRemove.length() - (idx + 1));
				}

				if (textCharacter != preTextCharacter) {
					break;
				}

				idx++;
				if (idx == textToRemove.length()) {
					// Remove all the white space characters till the next valid
					do {
						offset--;

						// if we don't have any more characters in the node get the next node
						if (offset < 0) {
							node = util::KikinUtils::getPreviousTextNode(node, false);

							ASSERT(node);
							text = node->nodeValue();
							offset = text.length();
							continue;
						}
						textCharacter = text.characterStartingAt(offset);
					} while (textCharacter == ' ' || textCharacter == '\r' || textCharacter == '\n' ||
							textCharacter == 0x000A || textCharacter == 0x000D);

					offset++;
					ExceptionCode ec = 0;
					m_range->setEnd(node, offset, ec);
					ASSERT(!ec);

					break;
				}
			}
		}
	}
}

void HtmlRange::resetWordsAround()
{
	m_previousText = WTF::String();
	m_postText = WTF::String();
}

void HtmlRange::saveWordsBeforeRange()
{
	if (!m_range) {
	    return;
	}

	Vector<String> before;
	m_previousText = WTF::String();

	Node* startEl = m_range->startContainer();
	int startOffset = m_range->startOffset();

	// get the text right next to the range (same text element)
	if ((startOffset > 0) && (startEl->nodeValue().stripWhiteSpace().length() > 0)) {
		before.append(startEl->nodeValue().left(startOffset));
	} else {
		before.append(" ");
	}

	// get previous text nodes values
	Node* prevNode = startEl;
	Node* currentNode = startEl;
	int j = 0;

	// Don't go beyond three valid previous text nodes.
	while (j < 3) {

		// Find the previous text node
		prevNode = util::KikinUtils::getPreviousTextNode(prevNode, false);
		if (!prevNode) {
			break;
		}

		// create ignorable elements list
		if (s_ignoreTags.isEmpty())
			HtmlRange::createIgnoreTagsList();

		// calculate the previous node's element.
		// sometimes if there are bunch of spaces than node does not have a parent element
		// at that time we check for node's parent node's element.
		Element* previousNodeElement = prevNode->parentElement() ? prevNode->parentElement() :
				prevNode->parentNode()->parentElement();

		// if previous node is a text node and the text of the node is not only white space
		// and node's element is not in the ignorable elements list, then it a valid previous
		// node whose text we can add while calculating pre-text for boundary correction.
		if (prevNode->isTextNode() && (prevNode->nodeValue().stripWhiteSpace().length() > 0) &&
				previousNodeElement && !s_ignoreTags.contains(previousNodeElement->tagName().lower()))
		{
			// calculate the current node's element.
			// sometimes if there are bunch of spaces than node does not have a parent element
			// at that time we check for node's parent node's element.
			Element* currentNodeElement = currentNode->parentElement() ? currentNode->parentElement() :
					currentNode->parentNode()->parentElement();

			// if previous node element is sibling to current node element, then previous
			// node is in different paragraph. In other words previous node is part of a
			// sentence of current node
			if ((currentNodeElement->parentElement()->isSameNode(previousNodeElement)) ||
					(previousNodeElement->parentElement()->isSameNode(currentNodeElement)))
			{
				++j;
				before.append(prevNode->nodeValue());
			} else {

				// break the loop if the previous node is not part of a current sentence
				break;
			}

			// save the previous node as current node
			currentNode = prevNode;
		}
	}

	// reverse the nodes we have added and build the previous text string
	for (int i = before.size() - 1; i >= 0; i--) {
		if ((before[i].length() > 0)) {
			m_previousText.append(before[i] + " ");
		}
	}

	// if there are more than 50 characters in the calculated
	// previous text, strip them down to last 50 characters.
	int len = m_previousText.length();
	if (len > 50) {
		m_previousText = m_previousText.right(50);
	}

	// replace all the double white spaces with single white space
	RegularExpression whiteSpaceRegex("\\s+", WTF::TextCaseInsensitive);
	replace(m_previousText, whiteSpaceRegex, " ");
}

void HtmlRange::saveWordsAfterRange()
{
	if (!m_range) {
	    return;
	}

	Vector<String> after;
	m_postText = WTF::String();

	Node* endEl = m_range->endContainer();
	int endOffset = m_range->endOffset();

	// get the text right next to the range (same text element)
	if ((endEl->nodeValue().stripWhiteSpace().length() > 0) && (endOffset < static_cast<int>(endEl->nodeValue().length()))) {
		after.append(endEl->nodeValue().substring(endOffset, endEl->nodeValue().length() - endOffset));
	} else {
		after.append(" ");
	}

	// get next text nodes values
	Node* nextNode = endEl;
	Node* currentNode = endEl;
	int j = 0;

	// Don't go beyond three valid next text nodes.
	while (j < 3) {

		// Find the next text node
		nextNode = util::KikinUtils::getNextTextNode(nextNode, false);
		if (!nextNode) {
			break;
		}

		// create ignorable elements list
		if (s_ignoreTags.isEmpty())
			HtmlRange::createIgnoreTagsList();

		// calculate the next node's element.
		// sometimes if there are bunch of spaces than node does not have a parent element
		// at that time we check for node's parent node's element.
		Element* nextNodeElement = nextNode->parentElement() ? nextNode->parentElement() :
				nextNode->parentNode()->parentElement();

		// if next node is a text node and the text of the node is not only white spaces
		// and node's element is not in the ignorable elements list, then it a valid next
		// node whose text we can add while calculating post-text for boundary correction.
		if (nextNode->isTextNode() && (nextNode->nodeValue().stripWhiteSpace().length() > 0) &&
				nextNodeElement && !s_ignoreTags.contains(nextNodeElement->tagName().lower()))
		{
			// calculate the current node's element.
			// sometimes if there are bunch of spaces than node does not have a parent element
			// at that time we check for node's parent node's element.
			Element* currentNodeElement = currentNode->parentElement() ? currentNode->parentElement() :
					currentNode->parentNode()->parentElement();

			// if next node element is sibling to current node element, then next
			// node is in different paragraph. In other words next node is part of a
			// sentence of current node
			if ((currentNodeElement->parentElement()->isSameNode(nextNodeElement)) ||
					(nextNodeElement->parentElement()->isSameNode(currentNodeElement)))
			{
				++j;
				after.append(nextNode->nodeValue());
			} else {

				// break the loop if the next node is not part of a current sentence
				break;
			}

			// save the next node as current node
			currentNode = nextNode;
		}
	}

	// build the post text string
	for (size_t i = 0; i < after.size(); i++) {
		if (after[i].length() > 0) {
			m_postText.append(after[i] + " ");
		}
	}

	// if there are more than 50 characters in the calculated
	// post text, strip them down to first 50 characters.
	int len = m_postText.length();
	if (len > 50) {
		m_postText = m_postText.left(50);
	}

	// replace all the double white spaces with single white space
	RegularExpression whiteSpaceRegex("\\s+", WTF::TextCaseInsensitive);
	replace(m_postText, whiteSpaceRegex, " ");
}

void HtmlRange::setStart(WebCore::Node* node, int offset) {
	if (m_range) {
		ExceptionCode ec = 0;
		m_range->setStart(node, offset, ec);
		ASSERT(!ec);
	}
}

void HtmlRange::setEnd(WebCore::Node* node, int offset) {
	if (m_range) {
		ExceptionCode ec = 0;
		m_range->setEnd(node, offset, ec);
		ASSERT(!ec);
	}
}

void HtmlRange::setNode(WebCore::Node* node) {
	m_node = node;
}

} /* namespace kikin */
