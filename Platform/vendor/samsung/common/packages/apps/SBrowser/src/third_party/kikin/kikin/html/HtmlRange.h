/*
 * Copyright (C) 2012 kikin Inc.
 *
 * All Rights Reserved.
 *
 * Author: Kapil Goel
 */

#ifndef KikinHtmlRange_h
#define KikinHtmlRange_h

#include <string>

#include <wtf/RefCounted.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
    
    /* Forward declaration of Range. */
    class Range;
    
    /* Forward declaration of Node. */
    class Node;
    
    /* Forward declaration of Element. */
    class Element;

	/* Forward declaration of ClientRect. */
    class ClientRect;

	/* Forward declaration of ClientRectList. */
    class ClientRectList;
}

namespace kikin {

namespace list {

/* Forward declaration of Ranges. */
class Ranges;

/* Forward declaration of ClientRects. */
class ClientRects;
    
} /* namespace list */

/* Class saves the info about the range of a single token in the entity. */
class HtmlRange : public WTF::RefCounted<HtmlRange>  {
public:
    
    /* Destructor */
    ~HtmlRange();
    
    /* Creates the instance of Range object from text node and adds the reference to it. */
    static WTF::PassRefPtr<HtmlRange> createFromTextNode(const WTF::String& text, WebCore::Node* node, int start, const WTF::String& type);
    
    /* Creates the instance of Range objectadds the reference to it. */
    static WTF::PassRefPtr<HtmlRange> create(WTF::PassRefPtr<WebCore::Range> range, WebCore::Node* node, const WTF::String& type);
    
    /* Creates the instance of Range object from node and adds the reference to it. */
    static WTF::PassRefPtr<HtmlRange> createFromNode(WebCore::Node* node, const WTF::String& type);
    
    /* Creates the list of ranges. */
    static WTF::PassRefPtr<list::Ranges> createRangesFromStartingPoint(WebCore::Node* startContainer, const int offset, const int noChars);
    
    /* Returns the Webcore range object for the range. */
    WebCore::Range* getRange();
    
    /* Returns the client rects for the range. */
    WTF::PassRefPtr<list::ClientRects> getClientRects();
    
    /* Returns the text for the range. */
    const WTF::String getText() const;
    
    /* Returns the type of entity for the range. */
    const WTF::String& getType() const { return m_type; }
    
    /* Checks if the character is a white space character. */
    static bool isWhiteSpaceOrDelimeter(UChar c);
    
    /* Extends the selection to given text in the given direction. */
    void extendSelectionToText(const WTF::String& extendedText, const bool beforeRange);

    /* Shrinks the selection in the given direction. */
    void shrinkSelection(const WTF::String& removedText, bool beforeRange);
    
    /* Returns the text before the range. */ 
    const WTF::String getPreviousText() const { return m_previousText; }
    
    /* Returns the text after the range. */
    const WTF::String getPostText() const { return m_postText; }
    
    /* Returns the element tag name for the range. */
    const WTF::String getTag();
    
    /* Returns the alt for the range. */
    const WTF::String getAlt();
    
    /* Returns the element's href for the range. */
    const WTF::String getHref();
    
    /* Resets the saved text before and after the range. */
    void resetWordsAround();
    
    /* Saves the words before range. */
    void saveWordsBeforeRange();

    /* Saves the words after range. */
    void saveWordsAfterRange();

    void setStart(WebCore::Node* node, int offset);
    void setEnd(WebCore::Node* node, int offset);
    void setNode(WebCore::Node* node);
    
private:
    
    /* Constructor */
    HtmlRange(WTF::PassRefPtr<WebCore::Range> range, WebCore::Node* node, const WTF::String& type);
    
    /* Counts the number of non-blank chars. */
    static WTF::HashMap<String, int>* countNonBlankChars(const WTF::String& text, int offset, bool forward, int noChars);
    
    /* Creates the list of ranges. */
    static WTF::PassRefPtr<list::Ranges> createRangesFromStartingPoint(WebCore::Node* currentContainer, bool forward, int noChars);
    
    /* Creates the range object and adds the reference to it. */
    static WTF::PassRefPtr<HtmlRange> createRange(WebCore::Node* container, int startOffset, int endOffset);
    
    /* Creates the list of ignorable tag names. */
    static void createIgnoreTagsList();
    
    /* Returns the rects for the text node. */
    WTF::PassRefPtr<list::ClientRects> getRectsForTextNode(WebCore::Node* node, int start, int end);
    
    /* Returns the cleaned rects for the range. */
    WTF::PassRefPtr<list::ClientRects> getCleanedClientRects(WTF::PassRefPtr<WebCore::Range> range);
    
    /* Returns the element for the range. */
    WebCore::Element* getElement();

	/* Returns the rect after adjusting the zoom level of the page. */
	WTF::PassRefPtr<WebCore::ClientRect> getZoomAdjustedClientRect(WTF::PassRefPtr<WebCore::ClientRect> rect);

	/** Checks whether this character should be ignored while shrinking/expanding selection. */
	bool shouldIgnoreCharacter(UChar32 character);
    
private:
    
    /* WebCore Range for the token. */
    WTF::RefPtr<WebCore::Range> m_range;
    
    /* Node for the range. */
    WebCore::Node* m_node;
    
    /* Type of entity. */
    WTF::String m_type;
    
    /* Text before the range. */
    WTF::String m_previousText;
    
    /* Text after the range. */
    WTF::String m_postText;

	/* Effective zoom level for the node. */
	float m_zoom;
    
    /* List of ignorable tag names. */
    static WTF::HashSet<WTF::String> s_ignoreTags;
};

}   // namespace kikin

#endif // KikinHtmlRange_h
