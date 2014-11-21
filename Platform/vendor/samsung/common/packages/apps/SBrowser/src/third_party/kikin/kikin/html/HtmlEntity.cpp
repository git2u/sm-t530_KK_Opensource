/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */
#define LOG_TAG "kikin_html_entity"

#include "config.h"
#include "kikin/html/HtmlEntity.h"

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"

#include "kikin/list/Ranges.h"
#include "kikin/list/ClientRects.h"
#include "kikin/html/HtmlRange.h"

using namespace WebCore;

namespace kikin {
    
HtmlEntity::HtmlEntity(WTF::PassRefPtr<list::Ranges> ranges, const WTF::String& type, const WTF::String& language) :
		Entity((const_cast<WTF::String&>(type)).charactersWithNullTermination(),
				(const_cast<WTF::String&>(language)).charactersWithNullTermination()),
				m_ranges(ranges)
{   
}
    
HtmlEntity::~HtmlEntity()
{
	m_ranges.clear();
}
    
// static 
HtmlEntity* HtmlEntity::create(WTF::PassRefPtr<list::Ranges> ranges,
		const WTF::String& type, const WTF::String& language)
{ 
    return new HtmlEntity(ranges, type, language);
}
    
WTF::PassRefPtr<list::ClientRects> HtmlEntity::getClientRects(WebCore::Document* ownerDocument)
{
    RefPtr<list::ClientRects> rects = list::ClientRects::create();

//    ALOGE("Number of ranges:%d\t%d", m_ranges->length(), __LINE__);

    // add all objects from all the ranges
    for (unsigned i = 0; i < m_ranges->length(); i++) {
    	rects->append(m_ranges->item(i)->getClientRects());
    }

    return rects.release();
}

WTF::PassRefPtr<list::ClientRects> HtmlEntity::modifySelectionByText(const WTF::String& oldQuery,
		const WTF::String& newQuery, WebCore::Document* ownerDocument)
{
	if (newQuery.length() == 0 || oldQuery.length() == 0) {
//		LOGE_KIKIN("HtmlEntity::modifySelectionByText() > the new query is empty or old query is empty!");
		return getClientRects(ownerDocument);
	}

	if (m_ranges && m_ranges->length() > 0) {
//		LOGE_KIKIN("Modifying text selection, %d", __LINE__);
//		LOGE_KIKIN("Old query:'%ls' \tNew query:'%ls' \t%d", oldQuery.utf8().data(), newQuery.utf8().data(), __LINE__);
		HtmlRange* range;
		if (newQuery.contains(oldQuery)) {
//			LOGE_KIKIN("New query contains old query.");
			int startIndex = newQuery.find(oldQuery);
//			LOGE_KIKIN("Start index:%d \t%d", startIndex, __LINE__);
			if (startIndex > 0) {
				String preText = newQuery.left(startIndex);
//				LOGE_KIKIN("Pre text:'%ls' \t%d", preText.utf8().data(), __LINE__);
				range = m_ranges->item(0);
				range->extendSelectionToText(preText, true);
			}

			unsigned endIndex = startIndex + oldQuery.length();
			unsigned newQueryLength = newQuery.length();
//			LOGE_KIKIN("End index:%d \tNew query length:%d \t%d", endIndex, newQueryLength, __LINE__);
			if (endIndex != newQueryLength) {
				String postText = newQuery.right(newQueryLength - endIndex);
//				LOGE_KIKIN("Post text:'%ls' \t%d", postText.utf8().data(), __LINE__);
				range = m_ranges->item(m_ranges->length() - 1);
				range->extendSelectionToText(postText, false);
			}
		} else if (oldQuery.contains(newQuery)) {
//			LOGE_KIKIN("Old query contains new query.");
			int startIndex = oldQuery.find(newQuery);
//			LOGE_KIKIN("Start index:%d \t%d", startIndex, __LINE__);
			if (startIndex > 0) {
				String preText = oldQuery.left(startIndex);
//				LOGE_KIKIN("Pre text before removing spaces:'%ls' \t%d", preText.utf8().data(), __LINE__);

				// remove spaces from post text
				preText = preText.stripWhiteSpace();
				preText.replace(" ", "").replace(0x000A, "");
//				LOGE_KIKIN("pre text after removing spaces:'%ls', %d", preText.utf8().data(), __LINE__);

				while (preText.length() > 0) {
					range = m_ranges->item(0);
					String rangeText = range->getText();
//					LOGE_KIKIN("Range text before removing spaces:'%ls', %d", rangeText.utf8().data(), __LINE__);

					// remove spaces from range text
					rangeText = rangeText.stripWhiteSpace();
					rangeText.replace(" ", "").replace(0x000A, "");
//					LOGE_KIKIN("Range text after removing spaces:'%ls', %d", rangeText.utf8().data(), __LINE__);

					if (preText.startsWith(rangeText)) {
						removeRange(range);
						preText.remove(0, rangeText.length());
//						LOGE_KIKIN("New pre text:'%ls', %d", preText.utf8().data(), __LINE__);
					} else {
						range->shrinkSelection(preText, true);
						break;
					}
				}
			}

			unsigned endIndex = startIndex + newQuery.length();
			unsigned oldQueryLength = oldQuery.length();
//			LOGE_KIKIN("End index:%d \tOld query length:%d \t%d", endIndex, oldQueryLength, __LINE__);
			if (endIndex != oldQueryLength) {
				String postText = oldQuery.right(oldQueryLength - endIndex);
//				LOGE_KIKIN("Post text before removing spaces:'%ls' \t%d", postText.utf8().data(), __LINE__);

				// remove spaces from post text
				postText = postText.stripWhiteSpace();
				postText.replace(" ", "").replace(0x000A, "");
//				LOGE_KIKIN("Post text after removing spaces:'%ls', %d", postText.utf8().data(), __LINE__);

				while (postText.length() > 0) {
					range = m_ranges->item(m_ranges->length() - 1);
					String rangeText = range->getText();
//					LOGE_KIKIN("Range text before removing spaces:'%ls', %d", rangeText.utf8().data(), __LINE__);

					// remove spaces from range text
					rangeText = rangeText.stripWhiteSpace();
					rangeText.replace(" ", "").replace(0x000A, "");
//					LOGE_KIKIN("Range text after removing spaces:'%ls', %d", rangeText.utf8().data(), __LINE__);

					if (postText.endsWith(rangeText)) {
						removeRange(range);
						postText.remove((postText.length() - rangeText.length()), rangeText.length());
//						LOGE_KIKIN("New post text:'%ls', %d", postText.utf8().data(), __LINE__);
					} else {
						range->shrinkSelection(postText, false);
						break;
					}
				}
			}
//		} else {
//			LOGE_KIKIN("HtmlEntity::modifySelectionByText() > neither the newQuery nor the oldQuery contains each other!");
		}
	}

//	LOGE_KIKIN("Returning text selection rects: %d", __LINE__);
	return getClientRects(ownerDocument);
}
    
const WTF::String HtmlEntity::getPreviousText() const
{
    if (m_ranges && m_ranges->length() > 0) {
        HtmlRange* range = m_ranges->item(0);
        return range->getPreviousText();
    }
    
    return WTF::String();
}

HtmlRange* HtmlEntity::getFirstRange() {
    return (m_ranges && m_ranges->length() > 0) ? m_ranges->item(0) : NULL;
}

HtmlRange* HtmlEntity::getLastRange() {
    return (m_ranges && m_ranges->length() > 0) ? m_ranges->item(m_ranges->length() - 1) : NULL;
}

HtmlRange* HtmlEntity::getPreviousRange(HtmlRange* range)
{
    if (m_ranges) {
        int index = m_ranges->indexOf(range);
        return ((index - 1) > -1) ? m_ranges->item(index - 1) : NULL;
    }
    return NULL;
}

HtmlRange* HtmlEntity::getNextRange(HtmlRange* range)
{
    if (m_ranges) {
        int index = m_ranges->indexOf(range);
        return ((index > -1) && ((index + 1) < static_cast<int>(m_ranges->length()))) ?
                m_ranges->item(static_cast<unsigned>(index + 1)) : NULL;
    }
    return NULL;
}

void HtmlEntity::removeRange(HtmlRange* range)
{
    if (m_ranges) {
        int index = m_ranges->indexOf(range);
        if ((index > -1) && (index < static_cast<int>(m_ranges->length())))
        {
            m_ranges->remove(static_cast<unsigned>(index));
        }
    }
}
    
const WTF::String HtmlEntity::getPostText() const
{
    if (m_ranges && m_ranges->length() > 0) {
        HtmlRange* range = m_ranges->item(m_ranges->length() - 1);
        return range->getPostText();
    }
    
    return WTF::String();
}
    
void HtmlEntity::resetWordsAround() const
{
    if (m_ranges && m_ranges->length() > 0) {
        HtmlRange* range = m_ranges->item(0);
        range->resetWordsAround();

        range = m_ranges->item(m_ranges->length() - 1);
        range->resetWordsAround();
    }
}
    
void HtmlEntity::saveWordsAround() const
{
    if (m_ranges && m_ranges->length() > 0) {
        HtmlRange* range = m_ranges->item(0);
        range->saveWordsBeforeRange();

        range = m_ranges->item(m_ranges->length() - 1);
        range->saveWordsAfterRange();
    }
}
    
const WTF::String HtmlEntity::getText() const
{
    WTF::String text;
    for (unsigned i = 0; i < m_ranges->length(); i++)
    {
        HtmlRange* range = m_ranges->item(i);
        if (i != 0 && range->getType() != "twitter")
        	text.append(" ");
        
        text.append(range->getText());
    }
    
    return text;
}
    
const WTF::String HtmlEntity::getTag() const
{
    HtmlRange* range = m_ranges->item(0);
    return range->getTag();
}
    
const WTF::String HtmlEntity::getAlt() const
{
    HtmlRange* range = m_ranges->item(0);
    return range->getAlt();
}

const WTF::String HtmlEntity::getHref() const
{
    HtmlRange* range = m_ranges->item(0);
    return range->getHref();
}

const WTF::String HtmlEntity::getType_WTF() const
{
	const kString & type = getType();
	return WTF::String(type.data(), type.length());
}

const WTF::String HtmlEntity::getValue_WTF() const
{
	const kString & value = getValue();
	return WTF::String(value.data(), value.length());
}

const WTF::String HtmlEntity::getLanguage_WTF() const
{
	const kString & langauge = getLanguage();
	return WTF::String(langauge.data(), langauge.length());
}

const WTF::String HtmlEntity::getTouchedWord_WTF() const
{
	const kString & touchedWord = getTouchedWord();
	return WTF::String(touchedWord.data(), touchedWord.length());
}

} /* namespace kikin */
