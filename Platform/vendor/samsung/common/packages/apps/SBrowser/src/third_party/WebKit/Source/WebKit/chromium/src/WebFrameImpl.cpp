/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// How ownership works
// -------------------
//
// Big oh represents a refcounted relationship: owner O--- ownee
//
// WebView (for the toplevel frame only)
//    O
//    |
//   Page O------- Frame (m_mainFrame) O-------O FrameView
//                   ||
//                   ||
//               FrameLoader O-------- WebFrame (via FrameLoaderClient)
//
// FrameLoader and Frame are formerly one object that was split apart because
// it got too big. They basically have the same lifetime, hence the double line.
//
// WebFrame is refcounted and has one ref on behalf of the FrameLoader/Frame.
// This is not a normal reference counted pointer because that would require
// changing WebKit code that we don't control. Instead, it is created with this
// ref initially and it is removed when the FrameLoader is getting destroyed.
//
// WebFrames are created in two places, first in WebViewImpl when the root
// frame is created, and second in WebFrame::CreateChildFrame when sub-frames
// are created. WebKit will hook up this object to the FrameLoader/Frame
// and the refcount will be correct.
//
// How frames are destroyed
// ------------------------
//
// The main frame is never destroyed and is re-used. The FrameLoader is re-used
// and a reference to the main frame is kept by the Page.
//
// When frame content is replaced, all subframes are destroyed. This happens
// in FrameLoader::detachFromParent for each subframe.
//
// Frame going away causes the FrameLoader to get deleted. In FrameLoader's
// destructor, it notifies its client with frameLoaderDestroyed. This calls
// WebFrame::Closing and then derefs the WebFrame and will cause it to be
// deleted (unless an external someone is also holding a reference).

#include "config.h"
#include "WebFrameImpl.h"

#include <algorithm>
#include "AssociatedURLLoader.h"
#include "AsyncFileSystemChromium.h"
#include "DOMUtilitiesPrivate.h"
#include "EventListenerWrapper.h"
#include "FindInPageCoordinates.h"
#include "HTMLNames.h"
#include "PageOverlay.h"
#include "V8DOMFileSystem.h"
#include "V8DirectoryEntry.h"
#include "V8FileEntry.h"
#include "WebConsoleMessage.h"
#include "WebDOMEvent.h"
#include "WebDOMEventListener.h"
#include "WebDataSourceImpl.h"
#include "WebDevToolsAgentPrivate.h"
#include "WebDocument.h"
#include "WebFindOptions.h"
#include "WebFormElement.h"
#include "WebFrameClient.h"
#include "WebHistoryItem.h"
#include "WebIconURL.h"
#include "WebInputElement.h"
#include "WebNode.h"
#include "WebPerformance.h"
#include "WebPlugin.h"
#include "WebPluginContainerImpl.h"
#include "WebPrintParams.h"
#include "WebRange.h"
#include "WebScriptSource.h"
#include "WebSecurityOrigin.h"
#include "WebSerializedScriptValue.h"
#include "WebViewImpl.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentMarker.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/IconURL.h"
#include "core/dom/MessagePort.h"
#include "core/dom/Node.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/dom/default/chromium/PlatformMessagePortChannelChromium.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SpellChecker.h"
#include "core/editing/TextAffinity.h"
#include "core/editing/TextIterator.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/history/BackForwardController.h"
#include "core/history/HistoryItem.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLLinkElement.h"
#include "core/html/PluginDocument.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FormState.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/SubstituteData.h"
#include "core/page/Chrome.h"
#include "core/page/Console.h"
#include "core/page/DOMWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Performance.h"
#include "core/page/PrintContext.h"
#include "core/page/SecurityPolicy.h"
#include "core/page/Settings.h"
#include "core/platform/AsyncFileSystem.h"
#include "core/platform/KURL.h"
#include "core/platform/SchemeRegistry.h"
#include "core/platform/ScrollTypes.h"
#include "core/platform/ScrollbarTheme.h"
#include "core/platform/chromium/ClipboardUtilitiesChromium.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/FontCache.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/skia/SkiaUtils.h"
#include "core/platform/Logging.h"
#include "core/platform/network/ResourceHandle.h"
#include "core/platform/network/ResourceRequest.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderFrame.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTreeAsText.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/style/StyleInheritedData.h"
#include "core/xml/XPathResult.h"
#include "modules/filesystem/DOMFileSystem.h"
#include "modules/filesystem/DirectoryEntry.h"
#include "modules/filesystem/FileEntry.h"
#include "modules/filesystem/FileSystemType.h"
#include <public/Platform.h>
#include <public/WebFileSystem.h>
#include <public/WebFileSystemType.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebURLError.h>
#include <public/WebVector.h>
#include <wtf/CurrentTime.h>
#include <wtf/HashMap.h>
#if defined(SBROWSER_TEXT_SELECTION)
#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderText.h"
#endif

// SAMSUNG: Reader
#if defined(SBROWSER_READER_NATIVE)
#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/ClientRect.h"
#include <stdlib.h>
#include "../../core/platform/text/RegularExpression.h"
#include <math.h>
#include <string>
#include <stdio.h>
#include <time.h>
#endif
//#include "base/logging.h"
// SAMSUNG: Reader

// -------------------------------------------------- kikin Modification starts --------------------------------------------------------------
#include "core/html/HTMLElement.h"     // Not under the kikin flag as it could be useful for others.
#include "core/html/HTMLMetaElement.h" // Not under the kikin flag as it could be useful for others.

#if defined (SBROWSER_KIKIN)
#include "kikin/html/HtmlAnalyzer.h"
#include "kikin/html/HtmlEntity.h"
#include "kikin/html/HtmlRange.h"
#include "kikin/list/Ranges.h"
#include "kikin/list/ClientRects.h"
#endif
// --------------------------------------------------- kikin Modification ends ---------------------------------------------------------------

// Get rid of WTF's LOG, so that we can use base/logging's LOG
#undef LOG
#include "base/logging.h"

using namespace WebCore;

namespace WebKit {

static int frameCount = 0;

// Key for a StatsCounter tracking how many WebFrames are active.
static const char* const webFrameActiveCount = "WebFrameActiveCount";

// Backend for contentAsPlainText, this is a recursive function that gets
// the text for the current frame and all of its subframes. It will append
// the text of each frame in turn to the |output| up to |maxChars| length.
//
// The |frame| must be non-null.
//
// FIXME: We should use StringBuilder rather than Vector<UChar>.
static void frameContentAsPlainText(size_t maxChars, Frame* frame, Vector<UChar>* output)
{
    Document* document = frame->document();
    if (!document)
        return;

    if (!frame->view())
        return;

    // TextIterator iterates over the visual representation of the DOM. As such,
    // it requires you to do a layout before using it (otherwise it'll crash).
    if (frame->view()->needsLayout())
        frame->view()->layout();

    // Select the document body.
    RefPtr<Range> range(document->createRange());
    ExceptionCode exception = 0;
    range->selectNodeContents(document->body(), exception);

    if (!exception) {
        // The text iterator will walk nodes giving us text. This is similar to
        // the plainText() function in core/editing/TextIterator.h, but we implement the maximum
        // size and also copy the results directly into a wstring, avoiding the
        // string conversion.
        for (TextIterator it(range.get()); !it.atEnd(); it.advance()) {
            const UChar* chars = it.characters();
            if (!chars) {
                if (it.length()) {
                    // It appears from crash reports that an iterator can get into a state
                    // where the character count is nonempty but the character pointer is
                    // null. advance()ing it will then just add that many to the null
                    // pointer which won't be caught in a null check but will crash.
                    //
                    // A null pointer and 0 length is common for some nodes.
                    //
                    // IF YOU CATCH THIS IN A DEBUGGER please let brettw know. We don't
                    // currently understand the conditions for this to occur. Ideally, the
                    // iterators would never get into the condition so we should fix them
                    // if we can.
                    ASSERT_NOT_REACHED();
                    break;
                }

                // Just got a null node, we can forge ahead!
                continue;
            }
            size_t toAppend =
                std::min(static_cast<size_t>(it.length()), maxChars - output->size());
            output->append(chars, toAppend);
            if (output->size() >= maxChars)
                return; // Filled up the buffer.
        }
    }

    // The separator between frames when the frames are converted to plain text.
    const UChar frameSeparator[] = { '\n', '\n' };
    const size_t frameSeparatorLen = 2;

    // Recursively walk the children.
    FrameTree* frameTree = frame->tree();
    for (Frame* curChild = frameTree->firstChild(); curChild; curChild = curChild->tree()->nextSibling()) {
        // Ignore the text of non-visible frames.
        RenderView* contentRenderer = curChild->contentRenderer();
        RenderPart* ownerRenderer = curChild->ownerRenderer();
        if (!contentRenderer || !contentRenderer->width() || !contentRenderer->height()
            || (contentRenderer->x() + contentRenderer->width() <= 0) || (contentRenderer->y() + contentRenderer->height() <= 0)
            || (ownerRenderer && ownerRenderer->style() && ownerRenderer->style()->visibility() != VISIBLE)) {
            continue;
        }

        // Make sure the frame separator won't fill up the buffer, and give up if
        // it will. The danger is if the separator will make the buffer longer than
        // maxChars. This will cause the computation above:
        //   maxChars - output->size()
        // to be a negative number which will crash when the subframe is added.
        if (output->size() >= maxChars - frameSeparatorLen)
            return;

        output->append(frameSeparator, frameSeparatorLen);
        frameContentAsPlainText(maxChars, curChild, output);
        if (output->size() >= maxChars)
            return; // Filled up the buffer.
    }
}

#if defined(SBROWSER_FPAUTH_IMPL)
void WebFrameImpl::CheckFramesWithForms(){

	LOG(INFO)<<"FP:WebFrameImpl::CheckFramesWithForms() ";

	 FrameTree* frameTree = frame()->tree();
    	 for (Frame* curChild = frameTree->firstChild(); curChild; curChild = curChild->tree()->traverseNext()) {
		 	if(curChild->document()->forms()){
		 		LOG(INFO)<<"FP:WebFrameImpl::CheckFramesWithForms()++"   ;
		 		client()->TriggerDidFinishDocumentLoad(fromFrame(curChild));
		 		LOG(INFO)<<"FP:WebFrameImpl::CheckFramesWithForms()-- ";
		 	}		 	
    	 }
}
#endif

#if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
void WebFrameImpl::setContentDetectionResult(const WebContentDetectionResult& result){
    m_contentDetectionResult = result; 
}
void WebFrameImpl::selectRange(Range* range)
{
    if(range)
        frame()->selection()->setSelectedRange(range, WebCore::VP_DEFAULT_AFFINITY, false);
}
#endif

#if defined(SBROWSER_TEXT_SELECTION)
struct AdvanceTouchNodeData {
    Node* mUrlNode;
    Node* mInnerNode;
    LayoutRect mBounds;
};

// get the bounding box of the Node
static LayoutRect getAbsoluteBoundingBox(Node* node)
{
    if(!node)
        return IntRect(0,0,0,0) ;
 //MPSG100005781 end --
    LayoutRect rect;
    RenderObject* render = node->renderer();
    if (!render)
        return rect;
    if (render->isRenderInline())
        rect = toRenderInline(render)->linesVisualOverflowBoundingBox();
    else if (render->isBox())
        rect = toRenderBox(render)->visualOverflowRect();
    else if (render->isText())
        rect = toRenderText(render)->linesBoundingBox();
    else
    	{}
    FloatPoint absPos = render->localToAbsolute(FloatPoint(), UseTransforms);
    rect.move(absPos.x(), absPos.y());
    return rect;
}
void  WebFrameImpl::OnClearTextSelection()
 {
     WebCore::Frame* frame_p = frame();
     if(!frame_p)
          return;
     frame_p->eventHandler()->clearExistingSelection();
 }
bool WebFrameImpl::PointOnRegion(int x, int y)
{
	WebCore::Frame* frame_p =  frame();
	WebCore::IntRect box;
      WTF::Vector<IntRect> boxVector;
	int boxX, boxY, boxWidth, boxHeight;
	bool result = false ;
	SkRegion prev_region, region;

	RefPtr<Range> range;
	if (  frame_p->selection()->isRange() &&  (range = frame_p->selection()->toNormalizedRange()) )
	{
		ExceptionCode ec = 0;
		RefPtr<Range> tempRange = range->cloneRange(ec);
		// consider image selection also while getting the bounds.
        boxVector = tempRange->boundingBoxEx();
        if (!boxVector.isEmpty())
        {
		region.setRect(boxVector[0]);
        for (size_t i = 1; i < boxVector.size(); i++) {
            region.op(boxVector[i], SkRegion::kUnion_Op);
        }
	box=boxVector[0];
	for (size_t i = 1; i < boxVector.size(); ++i){
        	box.unite(boxVector[i]);
	}
           prev_region.setRect(box.x(), box.y(), box.x() + box.width(), box.y() + box.height());
		   result = prev_region.contains( x, y);
			boxX = box.x();
			boxY = box.y();
			boxWidth = box.width();
			boxHeight = box.height();
			if (boxX < 0)
			{
				boxX = 0;
			}
			if (boxY < 0)
			{
				boxY = 0;
			}
			if (box.x() < 0 || box.y() < 0)
			{
				region.setRect(boxX, boxY, boxX + boxWidth, boxY + boxHeight);
			}

		}
	}
   boxVector.clear();//arvind.maan RTL selection fix
	if (result == false)
		result = region.contains( x,  y);

	return result;

}
bool  WebFrameImpl::getClosestWord(WebCore::IntPoint m_globalpos, WebCore::IntPoint& m_mousePos)
{
    int slop =16;
    Frame* frame_p = frame() ;
    HitTestResult hitTestResult = frame_p->eventHandler()->hitTestResultAtPoint(LayoutPoint(m_globalpos.x(), m_globalpos.y()),
         HitTestRequest::Active | HitTestRequest::ReadOnly, IntSize(slop, slop));

    bool found = false;
    AdvanceTouchNodeData final ;

    LayoutRect testRect(m_globalpos.x() - slop, m_globalpos.y() - slop, 2 * slop + 1, 2 * slop + 1);

    const ListHashSet<RefPtr<Node> >& list = hitTestResult.rectBasedTestResult();

    if (list.isEmpty())
        return false;

    frame_p = hitTestResult.innerNode()->document()->frame();
    Vector<AdvanceTouchNodeData> nodeDataList;
    ListHashSet<RefPtr<Node> >::const_iterator last = list.end();

    for (ListHashSet<RefPtr<Node> >::const_iterator it = list.begin(); it != last; ++it) {
        Node* it_Node = it->get();

		while (it_Node) {
			if (it_Node->nodeType() == Node::TEXT_NODE) {
				found = true;
				break;
			} else {
				it_Node = it_Node->parentNode();
			}
		}

		if (found) {
			AdvanceTouchNodeData newNode;
			newNode.mInnerNode = it_Node;
			LayoutRect rect = getAbsoluteBoundingBox(it_Node);
			newNode.mBounds = rect;
			nodeDataList.append(newNode);
		}
		else
			continue;
    }

    //get best intersecting rect
    final.mInnerNode = 0;

    int area = 0;
    Vector<AdvanceTouchNodeData>::const_iterator nlast = nodeDataList.end();
    for (Vector<AdvanceTouchNodeData>::const_iterator n = nodeDataList.begin(); n != nlast; ++n){
        LayoutRect rect = n->mBounds;
        rect.intersect(testRect);
        int a = rect.width() * rect.height();
        if (a > area)
           {
               final = *n;
               area = a;
            }
    }

     //Adjust mouse position
    IntPoint frameAdjust = IntPoint(0,0);
    if (frame_p != frame()) {
        frameAdjust = frame_p->view()->contentsToWindow(IntPoint());
	   // TOBe check Jitendra
	   //WebSize scroll_offset = webview()->mainFrame()->scrollOffset();
	   WebSize scroll_offset =  scrollOffset();
	   float scroll_width = static_cast<float>(scroll_offset.width);
	   float scroll_height = static_cast<float>(scroll_offset.height);
          frameAdjust.move((int)scroll_width, (int)scroll_height);
      }

    LayoutRect rect = final.mBounds;
    rect.move(frameAdjust.x(), frameAdjust.y());
    // adjust m_mousePos if it is not inside the returned highlight rectangle
    testRect.move(frameAdjust.x(), frameAdjust.y());
    // IntPoint RectSample = IntPoint(testRect.x(), testRect.y());
    testRect.intersect(rect);

// bounding rect of node is area which cover the surrounding area of the text.
    if ((testRect.width()!=0) && (testRect.height()!=0))
	{
		m_mousePos = WebCore::IntPoint(testRect.x(), testRect.y()) ;
		return true;
	} else {
		return false;
	}
}

bool WebFrameImpl::OnSelectClosestWord(int x , int y )
{
#if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
    if(m_contentDetectionResult.isValid())
    {
        RefPtr<Range> range = static_cast<PassRefPtr<Range> >(m_contentDetectionResult.range());
        selectRange(range.get());
        return true;
    }
#endif
    WebCore::Frame* frame_p = viewImpl()->focusedWebCoreFrame();
    IntPoint unscaledPoint(WebPoint(x,y));
    unscaledPoint.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());
    WebCore::IntPoint wndPoint = frame()->view()->contentsToWindow(unscaledPoint);

    if (!frame_p->eventHandler())
        {
            return false;
        }

    WebCore::PlatformEvent::Type met1 = WebCore::PlatformEvent::MouseMoved;
    WebCore::PlatformMouseEvent pme1(wndPoint, unscaledPoint, NoButton, met1,
				false, false, false, false, false, 0);
    bool bReturn;
    bReturn = frame_p->eventHandler()->sendContextMenuEventForWordSelection(pme1, false);

    if(false == bReturn) //PLM:P130731-5016
        {
            IntPoint m_MousePos;
            if ( true == getClosestWord(unscaledPoint,m_MousePos))
                {
                    WebCore::IntPoint wndPoint = frame_p->view()->contentsToWindow(m_MousePos);
                    WebCore::PlatformMouseEvent pme2(wndPoint, m_MousePos, NoButton, met1,
											  false, false, false, false, false, 0);
                    bReturn = frame_p->eventHandler()->sendContextMenuEventForWordSelection(pme2, false);
                }
        }

    return bReturn;
}

#endif
static long long generateFrameIdentifier()
{
    static long long next = 0;
    return ++next;
}

WebPluginContainerImpl* WebFrameImpl::pluginContainerFromFrame(Frame* frame)
{
    if (!frame)
        return 0;
    if (!frame->document() || !frame->document()->isPluginDocument())
        return 0;
    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame->document());
    return static_cast<WebPluginContainerImpl *>(pluginDocument->pluginWidget());
}

// Simple class to override some of PrintContext behavior. Some of the methods
// made virtual so that they can be overridden by ChromePluginPrintContext.
class ChromePrintContext : public PrintContext {
    WTF_MAKE_NONCOPYABLE(ChromePrintContext);
public:
    ChromePrintContext(Frame* frame)
        : PrintContext(frame)
        , m_printedPageWidth(0)
    {
    }

    virtual ~ChromePrintContext() { }

    virtual void begin(float width, float height)
    {
        ASSERT(!m_printedPageWidth);
        m_printedPageWidth = width;
        PrintContext::begin(m_printedPageWidth, height);
    }

    virtual void end()
    {
        PrintContext::end();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        IntRect pageRect = m_pageRects[pageNumber];
        return m_printedPageWidth / pageRect.width();
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    // On Linux, we don't have the problem with NativeTheme, hence we let WebKit
    // do the scaling and ignore the return value.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        IntRect pageRect = m_pageRects[pageNumber];
        float scale = m_printedPageWidth / pageRect.width();

        context.save();
#if OS(UNIX) && !OS(DARWIN)
        context.scale(WebCore::FloatSize(scale, scale));
#endif
        context.translate(static_cast<float>(-pageRect.x()), static_cast<float>(-pageRect.y()));
        context.clip(pageRect);
        frame()->view()->paintContents(&context, pageRect);
        context.restore();
        return scale;
    }

    void spoolAllPagesWithBoundaries(GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
    {
        if (!frame()->document() || !frame()->view() || !frame()->document()->renderer())
            return;

        frame()->document()->updateLayout();

        float pageHeight;
        computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

        const float pageWidth = pageSizeInPixels.width();
        size_t numPages = pageRects().size();
        int totalHeight = numPages * (pageSizeInPixels.height() + 1) - 1;

        // Fill the whole background by white.
        graphicsContext.setFillColor(Color(255, 255, 255), ColorSpaceDeviceRGB);
        graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

        graphicsContext.save();

        int currentHeight = 0;
        for (size_t pageIndex = 0; pageIndex < numPages; pageIndex++) {
            // Draw a line for a page boundary if this isn't the first page.
            if (pageIndex > 0) {
                graphicsContext.save();
                graphicsContext.setStrokeColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
                graphicsContext.setFillColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
                graphicsContext.drawLine(IntPoint(0, currentHeight), IntPoint(pageWidth, currentHeight));
                graphicsContext.restore();
            }

            graphicsContext.save();

            graphicsContext.translate(0, currentHeight);
#if !OS(UNIX) || OS(DARWIN)
            // Account for the disabling of scaling in spoolPage. In the context
            // of spoolAllPagesWithBoundaries the scale HAS NOT been pre-applied.
            float scale = getPageShrink(pageIndex);
            graphicsContext.scale(WebCore::FloatSize(scale, scale));
#endif
            spoolPage(graphicsContext, pageIndex);
            graphicsContext.restore();

            currentHeight += pageSizeInPixels.height() + 1;
        }

        graphicsContext.restore();
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        PrintContext::computePageRects(printRect, headerHeight, footerHeight, userScaleFactor, outPageHeight);
    }

    virtual int pageCount() const
    {
        return PrintContext::pageCount();
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return true;
    }

private:
    // Set when printing.
    float m_printedPageWidth;
};

// Simple class to override some of PrintContext behavior. This is used when
// the frame hosts a plugin that supports custom printing. In this case, we
// want to delegate all printing related calls to the plugin.
class ChromePluginPrintContext : public ChromePrintContext {
public:
    ChromePluginPrintContext(Frame* frame, WebPluginContainerImpl* plugin, const WebPrintParams& printParams)
        : ChromePrintContext(frame), m_plugin(plugin), m_pageCount(0), m_printParams(printParams)
    {
    }

    virtual ~ChromePluginPrintContext() { }

    virtual void begin(float width, float height)
    {
    }

    virtual void end()
    {
        m_plugin->printEnd();
    }

    virtual float getPageShrink(int pageNumber) const
    {
        // We don't shrink the page (maybe we should ask the widget ??)
        return 1.0;
    }

    virtual void computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
    {
        m_printParams.printContentArea = IntRect(printRect);
        m_pageCount = m_plugin->printBegin(m_printParams);
    }

    virtual int pageCount() const
    {
        return m_pageCount;
    }

    // Spools the printed page, a subrect of frame(). Skip the scale step.
    // NativeTheme doesn't play well with scaling. Scaling is done browser side
    // instead. Returns the scale to be applied.
    virtual float spoolPage(GraphicsContext& context, int pageNumber)
    {
        m_plugin->printPage(pageNumber, &context);
        return 1.0;
    }

    virtual bool shouldUseBrowserOverlays() const
    {
        return false;
    }

private:
    // Set when printing.
    WebPluginContainerImpl* m_plugin;
    int m_pageCount;
    WebPrintParams m_printParams;

};

static WebDataSource* DataSourceForDocLoader(DocumentLoader* loader)
{
    return loader ? WebDataSourceImpl::fromDocumentLoader(loader) : 0;
}

WebFrameImpl::FindMatch::FindMatch(PassRefPtr<Range> range, int ordinal)
    : m_range(range)
    , m_ordinal(ordinal)
{
}

class WebFrameImpl::DeferredScopeStringMatches {
public:
    DeferredScopeStringMatches(WebFrameImpl* webFrame, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
        : m_timer(this, &DeferredScopeStringMatches::doTimeout)
        , m_webFrame(webFrame)
        , m_identifier(identifier)
        , m_searchText(searchText)
        , m_options(options)
        , m_reset(reset)
    {
        m_timer.startOneShot(0.0);
    }

private:
    void doTimeout(Timer<DeferredScopeStringMatches>*)
    {
        m_webFrame->callScopeStringMatches(this, m_identifier, m_searchText, m_options, m_reset);
    }

    Timer<DeferredScopeStringMatches> m_timer;
    RefPtr<WebFrameImpl> m_webFrame;
    int m_identifier;
    WebString m_searchText;
    WebFindOptions m_options;
    bool m_reset;
};

// WebFrame -------------------------------------------------------------------

int WebFrame::instanceCount()
{
    return frameCount;
}

WebFrame* WebFrame::frameForCurrentContext()
{
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    if (context.IsEmpty())
        return 0;
    return frameForContext(context);
}

WebFrame* WebFrame::frameForContext(v8::Handle<v8::Context> context)
{
   return WebFrameImpl::fromFrame(toFrameIfNotDetached(context));
}

WebFrame* WebFrame::fromFrameOwnerElement(const WebElement& element)
{
    return WebFrameImpl::fromFrameOwnerElement(PassRefPtr<Element>(element).get());
}

WebString WebFrameImpl::uniqueName() const
{
    return frame()->tree()->uniqueName();
}

WebString WebFrameImpl::assignedName() const
{
    return frame()->tree()->name();
}

void WebFrameImpl::setName(const WebString& name)
{
    frame()->tree()->setName(name);
}

long long WebFrameImpl::identifier() const
{
    return m_identifier;
}

WebVector<WebIconURL> WebFrameImpl::iconURLs(int iconTypesMask) const
{
    // The URL to the icon may be in the header. As such, only
    // ask the loader for the icon if it's finished loading.
    if (frame()->loader()->state() == FrameStateComplete)
        return frame()->loader()->icon()->urlsForTypes(iconTypesMask);
    return WebVector<WebIconURL>();
}

WebSize WebFrameImpl::scrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return view->scrollOffset();
}

WebSize WebFrameImpl::minimumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->minimumScrollPosition());
}

WebSize WebFrameImpl::maximumScrollOffset() const
{
    FrameView* view = frameView();
    if (!view)
        return WebSize();
    return toIntSize(view->maximumScrollPosition());
}

void WebFrameImpl::setScrollOffset(const WebSize& offset)
{
    if (FrameView* view = frameView())
        view->setScrollOffset(IntPoint(offset.width, offset.height));
}

WebSize WebFrameImpl::contentsSize() const
{
    return frame()->view()->contentsSize();
}

int WebFrameImpl::contentsPreferredWidth() const
{
    if (frame()->document() && frame()->document()->renderView()) {
        FontCachePurgePreventer fontCachePurgePreventer;
        return frame()->document()->renderView()->minPreferredLogicalWidth();
    }
    return 0;
}

int WebFrameImpl::documentElementScrollHeight() const
{
    if (frame()->document() && frame()->document()->documentElement())
        return frame()->document()->documentElement()->scrollHeight();
    return 0;
}

bool WebFrameImpl::hasVisibleContent() const
{
    return frame()->view()->visibleWidth() > 0 && frame()->view()->visibleHeight() > 0;
}

WebRect WebFrameImpl::visibleContentRect() const
{
    return frame()->view()->visibleContentRect();
}

bool WebFrameImpl::hasHorizontalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->horizontalScrollbar();
}

bool WebFrameImpl::hasVerticalScrollbar() const
{
    return frame() && frame()->view() && frame()->view()->verticalScrollbar();
}

WebView* WebFrameImpl::view() const
{
    return viewImpl();
}

WebFrame* WebFrameImpl::opener() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->loader()->opener());
}

void WebFrameImpl::setOpener(const WebFrame* webFrame)
{
    frame()->loader()->setOpener(webFrame ? static_cast<const WebFrameImpl*>(webFrame)->frame() : 0);
}

WebFrame* WebFrameImpl::parent() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->parent());
}

WebFrame* WebFrameImpl::top() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->top());
}

WebFrame* WebFrameImpl::firstChild() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->firstChild());
}

WebFrame* WebFrameImpl::lastChild() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->lastChild());
}

WebFrame* WebFrameImpl::nextSibling() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->nextSibling());
}

WebFrame* WebFrameImpl::previousSibling() const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->previousSibling());
}

WebFrame* WebFrameImpl::traverseNext(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->traverseNextWithWrap(wrap));
}

WebFrame* WebFrameImpl::traversePrevious(bool wrap) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->traversePreviousWithWrap(wrap));
}

WebFrame* WebFrameImpl::findChildByName(const WebString& name) const
{
    if (!frame())
        return 0;
    return fromFrame(frame()->tree()->child(name));
}

WebFrame* WebFrameImpl::findChildByExpression(const WebString& xpath) const
{
    if (xpath.isEmpty())
        return 0;

    Document* document = frame()->document();

    ExceptionCode ec = 0;
    RefPtr<XPathResult> xpathResult = document->evaluate(xpath, document, 0, XPathResult::ORDERED_NODE_ITERATOR_TYPE, 0, ec);
    if (!xpathResult)
        return 0;

    Node* node = xpathResult->iterateNext(ec);
    if (!node || !node->isFrameOwnerElement())
        return 0;
    HTMLFrameOwnerElement* frameElement = toFrameOwnerElement(node);
    return fromFrame(frameElement->contentFrame());
}

WebDocument WebFrameImpl::document() const
{
    if (!frame() || !frame()->document())
        return WebDocument();
    return WebDocument(frame()->document());
}

WebPerformance WebFrameImpl::performance() const
{
    if (!frame())
        return WebPerformance();
    return WebPerformance(frame()->document()->domWindow()->performance());
}

NPObject* WebFrameImpl::windowObject() const
{
    if (!frame())
        return 0;
    return frame()->script()->windowScriptNPObject();
}

void WebFrameImpl::bindToWindowObject(const WebString& name, NPObject* object)
{
    if (!frame() || !frame()->script()->canExecuteScripts(NotAboutToExecuteScript))
        return;
    frame()->script()->bindToWindowObject(frame(), String(name), object);
}

void WebFrameImpl::executeScript(const WebScriptSource& source)
{
    ASSERT(frame());
    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    frame()->script()->executeScript(ScriptSourceCode(source.code, source.url, position));
}

void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup)
{
    ASSERT(frame());
    ASSERT(worldID > 0);

    Vector<ScriptSourceCode> sources;
    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

void WebFrameImpl::setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin& securityOrigin)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldSecurityOrigin(worldID, securityOrigin.get());
}

void WebFrameImpl::setIsolatedWorldContentSecurityPolicy(int worldID, const WebString& policy)
{
    ASSERT(frame());
    DOMWrapperWorld::setIsolatedWorldContentSecurityPolicy(worldID, policy);
}

void WebFrameImpl::addMessageToConsole(const WebConsoleMessage& message)
{
    ASSERT(frame());

    MessageLevel webCoreMessageLevel;
    switch (message.level) {
    case WebConsoleMessage::LevelDebug:
        webCoreMessageLevel = DebugMessageLevel;
        break;
    case WebConsoleMessage::LevelLog:
        webCoreMessageLevel = LogMessageLevel;
        break;
    case WebConsoleMessage::LevelWarning:
        webCoreMessageLevel = WarningMessageLevel;
        break;
    case WebConsoleMessage::LevelError:
        webCoreMessageLevel = ErrorMessageLevel;
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    frame()->document()->addConsoleMessage(OtherMessageSource, webCoreMessageLevel, message.text);
}

void WebFrameImpl::collectGarbage()
{
    if (!frame())
        return;
    if (!frame()->settings()->isScriptEnabled())
        return;
    V8GCController::collectGarbage();
}

bool WebFrameImpl::checkIfRunInsecureContent(const WebURL& url) const
{
    ASSERT(frame());
    return frame()->loader()->mixedContentChecker()->canRunInsecureContent(frame()->document()->securityOrigin(), url);
}

v8::Handle<v8::Value> WebFrameImpl::executeScriptAndReturnValue(const WebScriptSource& source)
{
    ASSERT(frame());

    // FIXME: This fake user gesture is required to make a bunch of pyauto
    // tests pass. If this isn't needed in non-test situations, we should
    // consider removing this code and changing the tests.
    // http://code.google.com/p/chromium/issues/detail?id=86397
    UserGestureIndicator gestureIndicator(DefinitelyProcessingNewUserGesture);

    TextPosition position(OrdinalNumber::fromOneBasedInt(source.startLine), OrdinalNumber::first());
    return frame()->script()->executeScript(ScriptSourceCode(source.code, source.url, position)).v8Value();
}

void WebFrameImpl::executeScriptInIsolatedWorld(int worldID, const WebScriptSource* sourcesIn, unsigned numSources, int extensionGroup, WebVector<v8::Local<v8::Value> >* results)
{
    ASSERT(frame());
    ASSERT(worldID > 0);

    Vector<ScriptSourceCode> sources;

    for (unsigned i = 0; i < numSources; ++i) {
        TextPosition position(OrdinalNumber::fromOneBasedInt(sourcesIn[i].startLine), OrdinalNumber::first());
        sources.append(ScriptSourceCode(sourcesIn[i].code, sourcesIn[i].url, position));
    }

    if (results) {
        Vector<ScriptValue> scriptResults;
        frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, &scriptResults);
        WebVector<v8::Local<v8::Value> > v8Results(scriptResults.size());
        for (unsigned i = 0; i < scriptResults.size(); i++)
            v8Results[i] = v8::Local<v8::Value>::New(scriptResults[i].v8Value());
        results->swap(v8Results);
    } else
        frame()->script()->evaluateInIsolatedWorld(worldID, sources, extensionGroup, 0);
}

v8::Handle<v8::Value> WebFrameImpl::callFunctionEvenIfScriptDisabled(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, int argc, v8::Handle<v8::Value> argv[])
{
    ASSERT(frame());
    return frame()->script()->callFunctionEvenIfScriptDisabled(function, receiver, argc, argv).v8Value();
}

v8::Local<v8::Context> WebFrameImpl::mainWorldScriptContext() const
{
    if (!frame())
        return v8::Local<v8::Context>();
    return ScriptController::mainWorldContext(frame());
}

v8::Handle<v8::Value> WebFrameImpl::createFileSystem(WebFileSystemType type, const WebString& name, const WebString& path)
{
    ASSERT(frame());
    return toV8(DOMFileSystem::create(frame()->document(), name, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, path.utf8().data()), AsyncFileSystemChromium::create()), v8::Handle<v8::Object>(), frame()->script()->currentWorldContext()->GetIsolate());
}

v8::Handle<v8::Value> WebFrameImpl::createSerializableFileSystem(WebFileSystemType type, const WebString& name, const WebString& path)
{
    ASSERT(frame());
    RefPtr<DOMFileSystem> fileSystem = DOMFileSystem::create(frame()->document(), name, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, path.utf8().data()), AsyncFileSystemChromium::create());
    fileSystem->makeClonable();
    return toV8(fileSystem.release(), v8::Handle<v8::Object>(), frame()->script()->currentWorldContext()->GetIsolate());
}

v8::Handle<v8::Value> WebFrameImpl::createFileEntry(WebFileSystemType type, const WebString& fileSystemName, const WebString& fileSystemPath, const WebString& filePath, bool isDirectory)
{
    ASSERT(frame());

    RefPtr<DOMFileSystemBase> fileSystem = DOMFileSystem::create(frame()->document(), fileSystemName, static_cast<WebCore::FileSystemType>(type), KURL(ParsedURLString, fileSystemPath.utf8().data()), AsyncFileSystemChromium::create());
    if (isDirectory)
        return toV8(DirectoryEntry::create(fileSystem, filePath), v8::Handle<v8::Object>(), frame()->script()->currentWorldContext()->GetIsolate());
    return toV8(FileEntry::create(fileSystem, filePath), v8::Handle<v8::Object>(), frame()->script()->currentWorldContext()->GetIsolate());
}

void WebFrameImpl::reload(bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader()->history()->saveDocumentAndScrollState();
    frame()->loader()->reload(ignoreCache);
}

void WebFrameImpl::reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache)
{
    ASSERT(frame());
    frame()->loader()->history()->saveDocumentAndScrollState();
    frame()->loader()->reloadWithOverrideURL(overrideUrl, ignoreCache);
}

void WebFrameImpl::loadRequest(const WebURLRequest& request)
{
    ASSERT(frame());
    ASSERT(!request.isNull());
    const ResourceRequest& resourceRequest = request.toResourceRequest();

    if (resourceRequest.url().protocolIs("javascript")) {
        loadJavaScriptURL(resourceRequest.url());
        return;
    }

    frame()->loader()->load(FrameLoadRequest(frame(), resourceRequest));
}

void WebFrameImpl::loadHistoryItem(const WebHistoryItem& item)
{
    ASSERT(frame());
    RefPtr<HistoryItem> historyItem = PassRefPtr<HistoryItem>(item);
    ASSERT(historyItem);

    frame()->loader()->prepareForHistoryNavigation();
    RefPtr<HistoryItem> currentItem = frame()->loader()->history()->currentItem();
    m_inSameDocumentHistoryLoad = currentItem && currentItem->shouldDoSameDocumentNavigationTo(historyItem.get());
    frame()->page()->goToItem(historyItem.get(), FrameLoadTypeIndexedBackForward);
    m_inSameDocumentHistoryLoad = false;
}

void WebFrameImpl::loadData(const WebData& data, const WebString& mimeType, const WebString& textEncoding, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());

    // If we are loading substitute data to replace an existing load, then
    // inherit all of the properties of that original request.  This way,
    // reload will re-attempt the original request.  It is essential that
    // we only do this when there is an unreachableURL since a non-empty
    // unreachableURL informs FrameLoader::reload to load unreachableURL
    // instead of the currently loaded URL.
    ResourceRequest request;
    if (replace && !unreachableURL.isEmpty())
        request = frame()->loader()->originalRequest();
    request.setURL(baseURL);

    FrameLoadRequest frameRequest(frame(), request, SubstituteData(data, mimeType, textEncoding, unreachableURL));
    ASSERT(frameRequest.substituteData().isValid());
    frame()->loader()->load(frameRequest);
    if (replace) {
        // Do this to force WebKit to treat the load as replacing the currently
        // loaded page.
        frame()->loader()->setReplacing();
    }
}

void WebFrameImpl::loadHTMLString(const WebData& data, const WebURL& baseURL, const WebURL& unreachableURL, bool replace)
{
    ASSERT(frame());
    loadData(data, WebString::fromUTF8("text/html"), WebString::fromUTF8("UTF-8"), baseURL, unreachableURL, replace);
}

bool WebFrameImpl::isLoading() const
{
    if (!frame())
        return false;
    return frame()->loader()->isLoading();
}

void WebFrameImpl::stopLoading()
{
    if (!frame())
        return;
    // FIXME: Figure out what we should really do here.  It seems like a bug
    // that FrameLoader::stopLoading doesn't call stopAllLoaders.
    frame()->loader()->stopAllLoaders();
    frame()->loader()->stopLoading(UnloadEventPolicyNone);
}

WebDataSource* WebFrameImpl::provisionalDataSource() const
{
    ASSERT(frame());

    // We regard the policy document loader as still provisional.
    DocumentLoader* documentLoader = frame()->loader()->provisionalDocumentLoader();
    if (!documentLoader)
        documentLoader = frame()->loader()->policyDocumentLoader();

    return DataSourceForDocLoader(documentLoader);
}

WebDataSource* WebFrameImpl::dataSource() const
{
    ASSERT(frame());
    return DataSourceForDocLoader(frame()->loader()->documentLoader());
}

WebHistoryItem WebFrameImpl::previousHistoryItem() const
{
    ASSERT(frame());
    // We use the previous item here because documentState (filled-out forms)
    // only get saved to history when it becomes the previous item.  The caller
    // is expected to query the history item after a navigation occurs, after
    // the desired history item has become the previous entry.
    return WebHistoryItem(frame()->loader()->history()->previousItem());
}

WebHistoryItem WebFrameImpl::currentHistoryItem() const
{
    ASSERT(frame());

    // We're shutting down.
    if (!frame()->loader()->activeDocumentLoader())
        return WebHistoryItem();

    // If we are still loading, then we don't want to clobber the current
    // history item as this could cause us to lose the scroll position and
    // document state.  However, it is OK for new navigations.
    // FIXME: Can we make this a plain old getter, instead of worrying about
    // clobbering here?
    if (!m_inSameDocumentHistoryLoad && (frame()->loader()->loadType() == FrameLoadTypeStandard
        || !frame()->loader()->activeDocumentLoader()->isLoadingInAPISense()))
        frame()->loader()->history()->saveDocumentAndScrollState();

    return WebHistoryItem(frame()->page()->backForward()->currentItem());
}

void WebFrameImpl::enableViewSourceMode(bool enable)
{
    if (frame())
        frame()->setInViewSourceMode(enable);
}

bool WebFrameImpl::isViewSourceModeEnabled() const
{
    if (!frame())
        return false;
    return frame()->inViewSourceMode();
}

void WebFrameImpl::setReferrerForRequest(WebURLRequest& request, const WebURL& referrerURL)
{
    String referrer = referrerURL.isEmpty() ? frame()->loader()->outgoingReferrer() : String(referrerURL.spec().utf16());
    referrer = SecurityPolicy::generateReferrerHeader(frame()->document()->referrerPolicy(), request.url(), referrer);
    if (referrer.isEmpty())
        return;
    request.setHTTPHeaderField(WebString::fromUTF8("Referer"), referrer);
}

void WebFrameImpl::dispatchWillSendRequest(WebURLRequest& request)
{
    ResourceResponse response;
    frame()->loader()->client()->dispatchWillSendRequest(0, 0, request.toMutableResourceRequest(), response);
}

WebURLLoader* WebFrameImpl::createAssociatedURLLoader(const WebURLLoaderOptions& options)
{
    return new AssociatedURLLoader(this, options);
}

void WebFrameImpl::commitDocumentData(const char* data, size_t length)
{
    frame()->loader()->documentLoader()->commitData(data, length);
}

unsigned WebFrameImpl::unloadListenerCount() const
{
    return frame()->document()->domWindow()->pendingUnloadEventListeners();
}

bool WebFrameImpl::willSuppressOpenerInNewFrame() const
{
    return frame()->loader()->suppressOpenerInNewFrame();
}

void WebFrameImpl::replaceSelection(const WebString& text)
{
    bool selectReplacement = false;
    bool smartReplace = true;
    frame()->editor()->replaceSelectionWithText(text, selectReplacement, smartReplace);
}

void WebFrameImpl::insertText(const WebString& text)
{
    if (frame()->editor()->hasComposition())
        frame()->editor()->confirmComposition(text);
    else
        frame()->editor()->insertText(text, 0);
}

void WebFrameImpl::setMarkedText(const WebString& text, unsigned location, unsigned length)
{
    Vector<CompositionUnderline> decorations;
    frame()->editor()->setComposition(text, decorations, location, length);
}

void WebFrameImpl::unmarkText()
{
    frame()->editor()->cancelComposition();
}

bool WebFrameImpl::hasMarkedText() const
{
    return frame()->editor()->hasComposition();
}

WebRange WebFrameImpl::markedRange() const
{
    return frame()->editor()->compositionRange();
}

bool WebFrameImpl::firstRectForCharacterRange(unsigned location, unsigned length, WebRect& rect) const
{
    if ((location + length < location) && (location + length))
        length = 0;

    RefPtr<Range> range = TextIterator::rangeFromLocationAndLength(frame()->selection()->rootEditableElementOrDocumentElement(), location, length);
    if (!range)
        return false;
    IntRect intRect = frame()->editor()->firstRectForRange(range.get());
    rect = WebRect(intRect);
    rect = frame()->view()->contentsToWindow(rect);
    return true;
}

size_t WebFrameImpl::characterIndexForPoint(const WebPoint& webPoint) const
{
    if (!frame())
        return notFound;

    IntPoint point = frame()->view()->windowToContents(webPoint);
    HitTestResult result = frame()->eventHandler()->hitTestResultAtPoint(point);
    RefPtr<Range> range = frame()->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (!range)
        return notFound;

    size_t location, length;
    TextIterator::getLocationAndLengthFromRange(frame()->selection()->rootEditableElementOrDocumentElement(), range.get(), location, length);
    return location;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebNode& node)
{
    ASSERT(frame());

    if (name.length() <= 2)
        return false;

    // Since we don't have NSControl, we will convert the format of command
    // string and call the function on Editor directly.
    String command = name;

    // Make sure the first letter is upper case.
    command.replace(0, 1, command.substring(0, 1).upper());

    // Remove the trailing ':' if existing.
    if (command[command.length() - 1] == UChar(':'))
        command = command.substring(0, command.length() - 1);

    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (!pluginContainer)
        pluginContainer = static_cast<WebPluginContainerImpl*>(node.pluginContainer());
    if (pluginContainer && pluginContainer->executeEditCommand(name))
        return true;

    bool result = true;

    // Specially handling commands that Editor::execCommand does not directly
    // support.
    if (command == "DeleteToEndOfParagraph") {
        if (!frame()->editor()->deleteWithDirection(DirectionForward, ParagraphBoundary, true, false))
            frame()->editor()->deleteWithDirection(DirectionForward, CharacterGranularity, true, false);
    } else if (command == "Indent")
        frame()->editor()->indent();
    else if (command == "Outdent")
        frame()->editor()->outdent();
    else if (command == "DeleteBackward")
        result = frame()->editor()->command(AtomicString("BackwardDelete")).execute();
    else if (command == "DeleteForward")
        result = frame()->editor()->command(AtomicString("ForwardDelete")).execute();
    else if (command == "AdvanceToNextMisspelling") {
        // Wee need to pass false here or else the currently selected word will never be skipped.
        frame()->editor()->advanceToNextMisspelling(false);
    } else if (command == "ToggleSpellPanel")
        frame()->editor()->showSpellingGuessPanel();
    else
        result = frame()->editor()->command(command).execute();
    return result;
}

bool WebFrameImpl::executeCommand(const WebString& name, const WebString& value)
{
    ASSERT(frame());
    String webName = name;

    // moveToBeginningOfDocument and moveToEndfDocument are only handled by WebKit for editable nodes.
    if (!frame()->editor()->canEdit() && webName == "moveToBeginningOfDocument")
        return viewImpl()->propagateScroll(ScrollUp, ScrollByDocument);

    if (!frame()->editor()->canEdit() && webName == "moveToEndOfDocument")
        return viewImpl()->propagateScroll(ScrollDown, ScrollByDocument);

    return frame()->editor()->command(webName).execute(value);
}

bool WebFrameImpl::isCommandEnabled(const WebString& name) const
{
    ASSERT(frame());
    return frame()->editor()->command(name).isEnabled();
}

void WebFrameImpl::enableContinuousSpellChecking(bool enable)
{
    if (enable == isContinuousSpellCheckingEnabled())
        return;
    frame()->editor()->toggleContinuousSpellChecking();
}

bool WebFrameImpl::isContinuousSpellCheckingEnabled() const
{
    return frame()->editor()->isContinuousSpellCheckingEnabled();
}

void WebFrameImpl::requestTextChecking(const WebElement& webElement)
{
    if (webElement.isNull())
        return;
    RefPtr<Range> rangeToCheck = rangeOfContents(const_cast<Element*>(webElement.constUnwrap<Element>()));
    frame()->editor()->spellChecker()->requestCheckingFor(SpellCheckRequest::create(TextCheckingTypeSpelling | TextCheckingTypeGrammar, TextCheckingProcessBatch, rangeToCheck, rangeToCheck));
}

void WebFrameImpl::replaceMisspelledRange(const WebString& text)
{
    // If this caret selection has two or more markers, this function replace the range covered by the first marker with the specified word as Microsoft Word does.
    if (pluginContainerFromFrame(frame()))
        return;
    RefPtr<Range> caretRange = frame()->selection()->toNormalizedRange();
    if (!caretRange)
        return;
    Vector<DocumentMarker*> markers = frame()->document()->markers()->markersInRange(caretRange.get(), DocumentMarker::Spelling | DocumentMarker::Grammar);
    if (markers.size() < 1 || markers[0]->startOffset() >= markers[0]->endOffset())
        return;
    RefPtr<Range> markerRange = Range::create(caretRange->ownerDocument(), caretRange->startContainer(), markers[0]->startOffset(), caretRange->endContainer(), markers[0]->endOffset());
    if (!markerRange)
        return;
    if (!frame()->selection()->shouldChangeSelection(markerRange.get()))
        return;
    frame()->selection()->setSelection(markerRange.get(), CharacterGranularity);
    frame()->editor()->replaceSelectionWithText(text, false, false);
}

void WebFrameImpl::removeSpellingMarkers()
{
    frame()->document()->markers()->removeMarkers(DocumentMarker::Spelling | DocumentMarker::Grammar);
}

bool WebFrameImpl::hasSelection() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->hasSelection();

    // frame()->selection()->isNone() never returns true.
    return (frame()->selection()->start() != frame()->selection()->end());
}

WebRange WebFrameImpl::selectionRange() const
{
    return frame()->selection()->toNormalizedRange();
}

WebString WebFrameImpl::selectionAsText() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsText();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range)
        return WebString();

    String text = range->text();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(text);
#endif
    replaceNBSPWithSpace(text);
    return text;
}

WebString WebFrameImpl::selectionAsMarkup() const
{
    WebPluginContainerImpl* pluginContainer = pluginContainerFromFrame(frame());
    if (pluginContainer)
        return pluginContainer->plugin()->selectionAsMarkup();

    RefPtr<Range> range = frame()->selection()->toNormalizedRange();
    if (!range)
        return WebString();

    return createMarkup(range.get(), 0, AnnotateForInterchange, false, ResolveNonLocalURLs);
}

void WebFrameImpl::selectWordAroundPosition(Frame* frame, VisiblePosition position)
{
    VisibleSelection selection(position);
    selection.expandUsingGranularity(WordGranularity);

    if (frame->selection()->shouldChangeSelection(selection)) {
// -------------------------------------------------- kikin Modification starts -------------------------------------------------------------
        #if defined (SBROWSER_KIKIN)
        selection = getKikinSelection(frame, selection);
        #endif
// --------------------------------------------------- kikin Modification ends --------------------------------------------------------------
        TextGranularity granularity = selection.isRange() ? WordGranularity : CharacterGranularity;
        frame->selection()->setSelection(selection, granularity);
    }
}

bool WebFrameImpl::selectWordAroundCaret()
{
    FrameSelection* selection = frame()->selection();
    ASSERT(!selection->isNone());
    if (selection->isNone() || selection->isRange())
        return false;
    selectWordAroundPosition(frame(), selection->selection().visibleStart());
    return true;
}

void WebFrameImpl::selectRange(const WebPoint& base, const WebPoint& extent)
{
    IntPoint unscaledBase = base;
    IntPoint unscaledExtent = extent;
    unscaledExtent.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());
    unscaledBase.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());
	

#if defined(SBROWSER_CQ_MPSG100011788)

    Element* editable = frame()->selection()->rootEditableElement();

    if (editable
#if defined(SBROWSER_PLM_P140214_04362)
    && frame()->selection()->selectionType()==VisibleSelection::RangeSelection
#endif
    ){

		FrameSelection* selection = frame()->selection();

		IntPoint baseContentsPoint = frame()->view()->windowToContents(unscaledBase);
		IntPoint extentContentsPoint = frame()->view()->windowToContents(unscaledExtent);

		LayoutPoint baseLocalPoint(editable->convertFromPage(baseContentsPoint));
		LayoutPoint extentLocalPoint(editable->convertFromPage(extentContentsPoint));

		VisiblePosition basePosition = editable->renderer()->positionForPoint(baseLocalPoint);
		VisiblePosition extentPosition = editable->renderer()->positionForPoint(extentLocalPoint);

		if (basePosition.isNull() || extentPosition.isNull() || basePosition == extentPosition)
		return;

		Position start = selection->selection().start();
		Position end = selection->selection().end();

		if (comparePositions(basePosition,extentPosition) > 0){//Base is not first
			selection->adjustSelection(false);
		}else{ //Base is first
			selection->adjustSelection(true);
		}

		frame()->selection()->setExtent(extentPosition,UserTriggered);

		return;
    	}

#endif

    VisiblePosition basePosition = visiblePositionForWindowPoint(unscaledBase);
    VisiblePosition extentPosition = visiblePositionForWindowPoint(unscaledExtent);
#if defined(SBROWSER_CQ_MPSG100012331)
    if (basePosition.isNull() || extentPosition.isNull() || basePosition == extentPosition)
        return;
#endif
    VisibleSelection newSelection = VisibleSelection(basePosition, extentPosition);

    if (frame()->selection()->shouldChangeSelection(newSelection))
        frame()->selection()->setSelection(newSelection, CharacterGranularity);
}

void WebFrameImpl::selectRange(const WebRange& webRange)
{
    if (RefPtr<Range> range = static_cast<PassRefPtr<Range> >(webRange))
        frame()->selection()->setSelectedRange(range.get(), WebCore::VP_DEFAULT_AFFINITY, false);
}

void WebFrameImpl::moveCaretSelectionTowardsWindowPoint(const WebPoint& point)
{
    IntPoint unscaledPoint(point);
    unscaledPoint.scale(1 / view()->pageScaleFactor(), 1 / view()->pageScaleFactor());

    Element* editable = frame()->selection()->rootEditableElement();
    if (!editable)
        return;

    IntPoint contentsPoint = frame()->view()->windowToContents(unscaledPoint);
    LayoutPoint localPoint(editable->convertFromPage(contentsPoint));
    VisiblePosition position = editable->renderer()->positionForPoint(localPoint);
    if (frame()->selection()->shouldChangeSelection(position))
        frame()->selection()->moveTo(position, UserTriggered);
}

VisiblePosition WebFrameImpl::visiblePositionForWindowPoint(const WebPoint& point)
{
    HitTestRequest request = HitTestRequest::Move | HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::IgnoreClipping | HitTestRequest::DisallowShadowContent;
    HitTestResult result(frame()->view()->windowToContents(IntPoint(point)));

    frame()->document()->renderView()->layer()->hitTest(request, result);

    Node* node = result.targetNode();
    if (!node || ! node->renderer() )
        return VisiblePosition();
    return node->renderer()->positionForPoint(result.localPoint());
}

int WebFrameImpl::printBegin(const WebPrintParams& printParams, const WebNode& constrainToNode, bool* useBrowserOverlays)
{
    ASSERT(!frame()->document()->isFrameSet());
    WebPluginContainerImpl* pluginContainer = 0;
    if (constrainToNode.isNull()) {
        // If this is a plugin document, check if the plugin supports its own
        // printing. If it does, we will delegate all printing to that.
        pluginContainer = pluginContainerFromFrame(frame());
    } else {
        // We only support printing plugin nodes for now.
        pluginContainer = static_cast<WebPluginContainerImpl*>(constrainToNode.pluginContainer());
    }

    if (pluginContainer && pluginContainer->supportsPaginatedPrint())
        m_printContext = adoptPtr(new ChromePluginPrintContext(frame(), pluginContainer, printParams));
    else
        m_printContext = adoptPtr(new ChromePrintContext(frame()));

    FloatRect rect(0, 0, static_cast<float>(printParams.printContentArea.width), static_cast<float>(printParams.printContentArea.height));
    m_printContext->begin(rect.width(), rect.height());
    float pageHeight;
    // We ignore the overlays calculation for now since they are generated in the
    // browser. pageHeight is actually an output parameter.
    m_printContext->computePageRects(rect, 0, 0, 1.0, pageHeight);
    if (useBrowserOverlays)
        *useBrowserOverlays = m_printContext->shouldUseBrowserOverlays();

    return m_printContext->pageCount();
}

float WebFrameImpl::getPrintPageShrink(int page)
{
    ASSERT(m_printContext && page >= 0);
    return m_printContext->getPageShrink(page);
}

float WebFrameImpl::printPage(int page, WebCanvas* canvas)
{
#if defined(SBROWSER_PRINT_IMPL)
    ASSERT(m_printContext && page >= 0 && frame() && frame()->document());

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);
    return m_printContext->spoolPage(graphicsContext, page);
#endif
	
#if ENABLE(PRINTING)
    ASSERT(m_printContext && page >= 0 && frame() && frame()->document());

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);
    return m_printContext->spoolPage(graphicsContext, page);
#else
    return 0;
#endif
}

void WebFrameImpl::printEnd()
{
    ASSERT(m_printContext);
    m_printContext->end();
    m_printContext.clear();
}

bool WebFrameImpl::isPrintScalingDisabledForPlugin(const WebNode& node)
{
    WebPluginContainerImpl* pluginContainer =  node.isNull() ? pluginContainerFromFrame(frame()) : static_cast<WebPluginContainerImpl*>(node.pluginContainer());

    if (!pluginContainer || !pluginContainer->supportsPaginatedPrint())
        return false;

    return pluginContainer->isPrintScalingDisabled();
}

bool WebFrameImpl::hasCustomPageSizeStyle(int pageIndex)
{
    return frame()->document()->styleForPage(pageIndex)->pageSizeType() != PAGE_SIZE_AUTO;
}

bool WebFrameImpl::isPageBoxVisible(int pageIndex)
{
    return frame()->document()->isPageBoxVisible(pageIndex);
}

void WebFrameImpl::pageSizeAndMarginsInPixels(int pageIndex, WebSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    IntSize size = pageSize;
    frame()->document()->pageSizeAndMarginsInPixels(pageIndex, size, marginTop, marginRight, marginBottom, marginLeft);
    pageSize = size;
}

WebString WebFrameImpl::pageProperty(const WebString& propertyName, int pageIndex)
{
    ASSERT(m_printContext);
    return m_printContext->pageProperty(frame(), propertyName.utf8().data(), pageIndex);
}

bool WebFrameImpl::find(int identifier, const WebString& searchText, const WebFindOptions& options, bool wrapWithinFrame, WebRect* selectionRect)
{
    if (!frame() || !frame()->page())
        return false;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (!options.findNext){
#if defined(SBROWSER_PLM_P130712_4102)
		m_activeMatch = 0; //resetting the active match in case of clear
#endif
        frame()->page()->unmarkAllTextMatches();
    	}
    else
        setMarkerActive(m_activeMatch.get(), false);

    if (m_activeMatch && m_activeMatch->ownerDocument() != frame()->document())
        m_activeMatch = 0;

    // If the user has selected something since the last Find operation we want
    // to start from there. Otherwise, we start searching from where the last Find
    // operation left off (either a Find or a FindNext operation).
    VisibleSelection selection(frame()->selection()->selection());
    bool activeSelection = !selection.isNone();
    if (activeSelection) {
        m_activeMatch = selection.firstRange().get();
        frame()->selection()->clear();
    }

    ASSERT(frame() && frame()->view());
    const FindOptions findOptions = (options.forward ? 0 : Backwards)
        | (options.matchCase ? 0 : CaseInsensitive)
        | (wrapWithinFrame ? WrapAround : 0)
        | (!options.findNext ? StartInSelection : 0);
    m_activeMatch = frame()->editor()->findStringAndScrollToVisible(searchText, m_activeMatch.get(), findOptions);

    if (!m_activeMatch) {
        // If we're finding next the next active match might not be in the current frame.
        // In this case we don't want to clear the matches cache.
        if (!options.findNext)
            clearFindMatchesCache();
        invalidateArea(InvalidateAll);
        return false;
    }

#if OS(ANDROID)
#if !defined (SBROWSER_PLM_P130710_4354)
    viewImpl()->zoomToFindInPageRect(frameView()->contentsToWindow(enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()))));
#endif
#endif

    setMarkerActive(m_activeMatch.get(), true);
    WebFrameImpl* oldActiveFrame = mainFrameImpl->m_currentActiveMatchFrame;
    mainFrameImpl->m_currentActiveMatchFrame = this;

    // Make sure no node is focused. See http://crbug.com/38700.
    frame()->document()->setFocusedNode(0);

    if (!options.findNext || activeSelection) {
        // This is either a Find operation or a Find-next from a new start point
        // due to a selection, so we set the flag to ask the scoping effort
        // to find the active rect for us and report it back to the UI.
        m_locatingActiveRect = true;
    } else {
        if (oldActiveFrame != this) {
            if (options.forward)
                m_activeMatchIndexInCurrentFrame = 0;
            else
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        } else {
            if (options.forward)
                ++m_activeMatchIndexInCurrentFrame;
            else
                --m_activeMatchIndexInCurrentFrame;

            if (m_activeMatchIndexInCurrentFrame + 1 > m_lastMatchCount)
                m_activeMatchIndexInCurrentFrame = 0;
            if (m_activeMatchIndexInCurrentFrame == -1)
                m_activeMatchIndexInCurrentFrame = m_lastMatchCount - 1;
        }
        if (selectionRect) {
            *selectionRect = frameView()->contentsToWindow(m_activeMatch->boundingBox());
            reportFindInPageSelection(*selectionRect, m_activeMatchIndexInCurrentFrame + 1, identifier);
        }
    }

    return true;
}

void WebFrameImpl::stopFinding(bool clearSelection)
{
    if (!clearSelection)
        setFindEndstateFocusAndSelection();
    cancelPendingScopingEffort();

    // Remove all markers for matches found and turn off the highlighting.
    frame()->document()->markers()->removeMarkers(DocumentMarker::TextMatch);
    frame()->editor()->setMarkedTextMatchesAreHighlighted(false);
    clearFindMatchesCache();

    // Let the frame know that we don't want tickmarks or highlighting anymore.
    invalidateArea(InvalidateAll);
}

void WebFrameImpl::scopeStringMatches(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    if (reset) {
        // This is a brand new search, so we need to reset everything.
        // Scoping is just about to begin.
        m_scopingInProgress = true;

        // Need to keep the current identifier locally in order to finish the
        // request in case the frame is detached during the process.
        m_findRequestIdentifier = identifier;

        // Clear highlighting for this frame.
        if (frame() && frame()->page() && frame()->editor()->markedTextMatchesAreHighlighted())
            frame()->page()->unmarkAllTextMatches();

        // Clear the tickmarks and results cache.
        clearFindMatchesCache();

        // Clear the counters from last operation.
        m_lastMatchCount = 0;
        m_nextInvalidateAfter = 0;

        m_resumeScopingFromRange = 0;

        // The view might be null on detached frames.
        if (frame() && frame()->page())
            viewImpl()->mainFrameImpl()->m_framesScopingCount++;

        // Now, defer scoping until later to allow find operation to finish quickly.
        scopeStringMatchesSoon(identifier, searchText, options, false); // false means just reset, so don't do it again.
        return;
    }

    if (!shouldScopeMatches(searchText)) {
        // Note that we want to defer the final update when resetting even if shouldScopeMatches returns false.
        // This is done in order to prevent sending a final message based only on the results of the first frame
        // since m_framesScopingCount would be 0 as other frames have yet to reset.
        finishCurrentScopingEffort(identifier);
        return;
    }

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    RefPtr<Range> searchRange(rangeOfContents(frame()->document()));

    Node* originalEndContainer = searchRange->endContainer();
    int originalEndOffset = searchRange->endOffset();

    ExceptionCode ec = 0, ec2 = 0;
    if (m_resumeScopingFromRange) {
        // This is a continuation of a scoping operation that timed out and didn't
        // complete last time around, so we should start from where we left off.
        searchRange->setStart(m_resumeScopingFromRange->startContainer(),
                              m_resumeScopingFromRange->startOffset(ec2) + 1,
                              ec);
        if (ec || ec2) {
            if (ec2) // A non-zero |ec| happens when navigating during search.
                ASSERT_NOT_REACHED();
            return;
        }
    }

    // This timeout controls how long we scope before releasing control.  This
    // value does not prevent us from running for longer than this, but it is
    // periodically checked to see if we have exceeded our allocated time.
    const double maxScopingDuration = 0.1; // seconds

    int matchCount = 0;
    bool timedOut = false;
    double startTime = currentTime();
    do {
        // Find next occurrence of the search string.
        // FIXME: (http://b/1088245) This WebKit operation may run for longer
        // than the timeout value, and is not interruptible as it is currently
        // written. We may need to rewrite it with interruptibility in mind, or
        // find an alternative.
        RefPtr<Range> resultRange(findPlainText(searchRange.get(),
                                                searchText,
                                                options.matchCase ? 0 : CaseInsensitive));
        if (resultRange->collapsed(ec)) {
            if (!resultRange->startContainer()->isInShadowTree())
                break;

            searchRange->setStartAfter(
                resultRange->startContainer()->deprecatedShadowAncestorNode(), ec);
            searchRange->setEnd(originalEndContainer, originalEndOffset, ec);
            continue;
        }

        ++matchCount;

        // Catch a special case where Find found something but doesn't know what
        // the bounding box for it is. In this case we set the first match we find
        // as the active rect.
        IntRect resultBounds = resultRange->boundingBox();
        IntRect activeSelectionRect;
        if (m_locatingActiveRect) {
            activeSelectionRect = m_activeMatch.get() ?
                m_activeMatch->boundingBox() : resultBounds;
        }

        // If the Find function found a match it will have stored where the
        // match was found in m_activeSelectionRect on the current frame. If we
        // find this rect during scoping it means we have found the active
        // tickmark.
        bool foundActiveMatch = false;
        if (m_locatingActiveRect && (activeSelectionRect == resultBounds)) {
            // We have found the active tickmark frame.
            mainFrameImpl->m_currentActiveMatchFrame = this;
            foundActiveMatch = true;
            // We also know which tickmark is active now.
            m_activeMatchIndexInCurrentFrame = matchCount - 1;
            // To stop looking for the active tickmark, we set this flag.
            m_locatingActiveRect = false;

            // Notify browser of new location for the selected rectangle.
            reportFindInPageSelection(
                frameView()->contentsToWindow(resultBounds),
                m_activeMatchIndexInCurrentFrame + 1,
                identifier);
        }

        addMarker(resultRange.get(), foundActiveMatch);

        m_findMatchesCache.append(FindMatch(resultRange.get(), m_lastMatchCount + matchCount));

        // Set the new start for the search range to be the end of the previous
        // result range. There is no need to use a VisiblePosition here,
        // since findPlainText will use a TextIterator to go over the visible
        // text nodes.
        searchRange->setStart(resultRange->endContainer(ec), resultRange->endOffset(ec), ec);

        Node* shadowTreeRoot = searchRange->shadowRoot();
        if (searchRange->collapsed(ec) && shadowTreeRoot)
            searchRange->setEnd(shadowTreeRoot, shadowTreeRoot->childNodeCount(), ec);

        m_resumeScopingFromRange = resultRange;
        timedOut = (currentTime() - startTime) >= maxScopingDuration;
    } while (!timedOut);

    // Remember what we search for last time, so we can skip searching if more
    // letters are added to the search string (and last outcome was 0).
    m_lastSearchString = searchText;

    if (matchCount > 0) {
        frame()->editor()->setMarkedTextMatchesAreHighlighted(true);

        m_lastMatchCount += matchCount;

        // Let the mainframe know how much we found during this pass.
        mainFrameImpl->increaseMatchCount(matchCount, identifier);
    }

    if (timedOut) {
        // If we found anything during this pass, we should redraw. However, we
        // don't want to spam too much if the page is extremely long, so if we
        // reach a certain point we start throttling the redraw requests.
        if (matchCount > 0)
            invalidateIfNecessary();

        // Scoping effort ran out of time, lets ask for another time-slice.
        scopeStringMatchesSoon(
            identifier,
            searchText,
            options,
            false); // don't reset.
        return; // Done for now, resume work later.
    }

    finishCurrentScopingEffort(identifier);
}

void WebFrameImpl::flushCurrentScopingEffort(int identifier)
{
    if (!frame() || !frame()->page())
        return;

    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    // This frame has no further scoping left, so it is done. Other frames might,
    // of course, continue to scope matches.
    mainFrameImpl->m_framesScopingCount--;

    // If this is the last frame to finish scoping we need to trigger the final
    // update to be sent.
    if (!mainFrameImpl->m_framesScopingCount)
        mainFrameImpl->increaseMatchCount(0, identifier);
}

void WebFrameImpl::finishCurrentScopingEffort(int identifier)
{
    flushCurrentScopingEffort(identifier);

    m_scopingInProgress = false;
    m_lastFindRequestCompletedWithNoMatches = !m_lastMatchCount;

    // This frame is done, so show any scrollbar tickmarks we haven't drawn yet.
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::cancelPendingScopingEffort()
{
    deleteAllValues(m_deferredScopingWork);
    m_deferredScopingWork.clear();

    m_activeMatchIndexInCurrentFrame = -1;

    // Last request didn't complete.
    if (m_scopingInProgress)
        m_lastFindRequestCompletedWithNoMatches = false;

    m_scopingInProgress = false;
}

void WebFrameImpl::increaseMatchCount(int count, int identifier)
{
    // This function should only be called on the mainframe.
    ASSERT(!parent());

    if (count)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount += count;

    // Update the UI with the latest findings.
    if (client())
        client()->reportFindInPageMatchCount(identifier, m_totalMatchCount, !m_framesScopingCount);
}

void WebFrameImpl::reportFindInPageSelection(const WebRect& selectionRect, int activeMatchOrdinal, int identifier)
{
    // Update the UI with the latest selection rect.
    if (client())
        client()->reportFindInPageSelection(identifier, ordinalOfFirstMatchForFrame(this) + activeMatchOrdinal, selectionRect);
}

void WebFrameImpl::resetMatchCount()
{
    if (m_totalMatchCount > 0)
        ++m_findMatchMarkersVersion;

    m_totalMatchCount = 0;
    m_framesScopingCount = 0;
}

void WebFrameImpl::sendOrientationChangeEvent(int orientation)
{
#if ENABLE(ORIENTATION_EVENTS)
    if (frame())
        frame()->sendOrientationChangeEvent(orientation);
#endif
}

void WebFrameImpl::dispatchMessageEventWithOriginCheck(const WebSecurityOrigin& intendedTargetOrigin, const WebDOMEvent& event)
{
    ASSERT(!event.isNull());
    frame()->document()->domWindow()->dispatchMessageEventWithOriginCheck(intendedTargetOrigin.get(), event, 0);
}

int WebFrameImpl::findMatchMarkersVersion() const
{
    ASSERT(!parent());
    return m_findMatchMarkersVersion;
}

void WebFrameImpl::clearFindMatchesCache()
{
    if (!m_findMatchesCache.isEmpty())
        viewImpl()->mainFrameImpl()->m_findMatchMarkersVersion++;

    m_findMatchesCache.clear();
    m_findMatchRectsAreValid = false;
}

bool WebFrameImpl::isActiveMatchFrameValid() const
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    WebFrameImpl* activeMatchFrame = mainFrameImpl->activeMatchFrame();
    return activeMatchFrame && activeMatchFrame->m_activeMatch && activeMatchFrame->frame()->tree()->isDescendantOf(mainFrameImpl->frame());
}

void WebFrameImpl::updateFindMatchRects()
{
    IntSize currentContentsSize = contentsSize();
    if (m_contentsSizeForCurrentFindMatchRects != currentContentsSize) {
        m_contentsSizeForCurrentFindMatchRects = currentContentsSize;
        m_findMatchRectsAreValid = false;
    }

    size_t deadMatches = 0;
    for (Vector<FindMatch>::iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        if (!it->m_range->boundaryPointsValid() || !it->m_range->startContainer()->inDocument())
            it->m_rect = FloatRect();
        else if (!m_findMatchRectsAreValid)
            it->m_rect = findInPageRectFromRange(it->m_range.get());

        if (it->m_rect.isEmpty())
            ++deadMatches;
    }

    // Remove any invalid matches from the cache.
    if (deadMatches) {
        Vector<FindMatch> filteredMatches;
        filteredMatches.reserveCapacity(m_findMatchesCache.size() - deadMatches);

        for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it)
            if (!it->m_rect.isEmpty())
                filteredMatches.append(*it);

        m_findMatchesCache.swap(filteredMatches);
    }

    // Invalidate the rects in child frames. Will be updated later during traversal.
    if (!m_findMatchRectsAreValid)
        for (WebFrame* child = firstChild(); child; child = child->nextSibling())
            static_cast<WebFrameImpl*>(child)->m_findMatchRectsAreValid = false;

    m_findMatchRectsAreValid = true;
}

WebFloatRect WebFrameImpl::activeFindMatchRect()
{
    ASSERT(!parent());

    if (!isActiveMatchFrameValid())
        return WebFloatRect();

    return WebFloatRect(findInPageRectFromRange(m_currentActiveMatchFrame->m_activeMatch.get()));
}

void WebFrameImpl::findMatchRects(WebVector<WebFloatRect>& outputRects)
{
    ASSERT(!parent());

    Vector<WebFloatRect> matchRects;
    for (WebFrameImpl* frame = this; frame; frame = static_cast<WebFrameImpl*>(frame->traverseNext(false)))
        frame->appendFindMatchRects(matchRects);

    outputRects = matchRects;
}

void WebFrameImpl::appendFindMatchRects(Vector<WebFloatRect>& frameRects)
{
    updateFindMatchRects();
    frameRects.reserveCapacity(frameRects.size() + m_findMatchesCache.size());
    for (Vector<FindMatch>::const_iterator it = m_findMatchesCache.begin(); it != m_findMatchesCache.end(); ++it) {
        ASSERT(!it->m_rect.isEmpty());
        frameRects.append(it->m_rect);
    }
}

int WebFrameImpl::selectNearestFindMatch(const WebFloatPoint& point, WebRect* selectionRect)
{
    ASSERT(!parent());

    WebFrameImpl* bestFrame = 0;
    int indexInBestFrame = -1;
    float distanceInBestFrame = FLT_MAX;

    for (WebFrameImpl* frame = this; frame; frame = static_cast<WebFrameImpl*>(frame->traverseNext(false))) {
        float distanceInFrame;
        int indexInFrame = frame->nearestFindMatch(point, distanceInFrame);
        if (distanceInFrame < distanceInBestFrame) {
            bestFrame = frame;
            indexInBestFrame = indexInFrame;
            distanceInBestFrame = distanceInFrame;
        }
    }

    if (indexInBestFrame != -1)
        return bestFrame->selectFindMatch(static_cast<unsigned>(indexInBestFrame), selectionRect);

    return -1;
}

int WebFrameImpl::nearestFindMatch(const FloatPoint& point, float& distanceSquared)
{
    updateFindMatchRects();

    int nearest = -1;
    distanceSquared = FLT_MAX;
    for (size_t i = 0; i < m_findMatchesCache.size(); ++i) {
        ASSERT(!m_findMatchesCache[i].m_rect.isEmpty());
        FloatSize offset = point - m_findMatchesCache[i].m_rect.center();
        float width = offset.width();
        float height = offset.height();
        float currentDistanceSquared = width * width + height * height;
        if (currentDistanceSquared < distanceSquared) {
            nearest = i;
            distanceSquared = currentDistanceSquared;
        }
    }
    return nearest;
}

int WebFrameImpl::selectFindMatch(unsigned index, WebRect* selectionRect)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < m_findMatchesCache.size());

    RefPtr<Range> range = m_findMatchesCache[index].m_range;
    if (!range->boundaryPointsValid() || !range->startContainer()->inDocument())
        return -1;

    // Check if the match is already selected.
    WebFrameImpl* activeMatchFrame = viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame;
    if (this != activeMatchFrame || !m_activeMatch || !areRangesEqual(m_activeMatch.get(), range.get())) {
        if (isActiveMatchFrameValid())
            activeMatchFrame->setMarkerActive(activeMatchFrame->m_activeMatch.get(), false);

        m_activeMatchIndexInCurrentFrame = m_findMatchesCache[index].m_ordinal - 1;

        // Set this frame as the active frame (the one with the active highlight).
        viewImpl()->mainFrameImpl()->m_currentActiveMatchFrame = this;
        viewImpl()->setFocusedFrame(this);

        m_activeMatch = range.release();
        setMarkerActive(m_activeMatch.get(), true);

        // Clear any user selection, to make sure Find Next continues on from the match we just activated.
        frame()->selection()->clear();

        // Make sure no node is focused. See http://crbug.com/38700.
        frame()->document()->setFocusedNode(0);
    }

    IntRect activeMatchRect;
    IntRect activeMatchBoundingBox = enclosingIntRect(RenderObject::absoluteBoundingBoxRectForRange(m_activeMatch.get()));

    if (!activeMatchBoundingBox.isEmpty()) {
        if (m_activeMatch->firstNode() && m_activeMatch->firstNode()->renderer())
            m_activeMatch->firstNode()->renderer()->scrollRectToVisible(activeMatchBoundingBox,
                    ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);

        // Zoom to the active match.
        activeMatchRect = frameView()->contentsToWindow(activeMatchBoundingBox);
#if !defined (SBROWSER_PLM_P130710_4354)		
        viewImpl()->zoomToFindInPageRect(activeMatchRect);
#endif
    }

    if (selectionRect)
        *selectionRect = activeMatchRect;

    return ordinalOfFirstMatchForFrame(this) + m_activeMatchIndexInCurrentFrame + 1;
}

WebString WebFrameImpl::contentAsText(size_t maxChars) const
{
    if (!frame())
        return WebString();
    Vector<UChar> text;
    frameContentAsPlainText(maxChars, frame(), &text);
    return String::adopt(text);
}

WebString WebFrameImpl::contentAsMarkup() const
{
    if (!frame())
        return WebString();
    return createFullMarkup(frame()->document());
}

WebString WebFrameImpl::renderTreeAsText(RenderAsTextControls toShow) const
{
    RenderAsTextBehavior behavior = RenderAsTextBehaviorNormal;

    if (toShow & RenderAsTextDebug)
        behavior |= RenderAsTextShowCompositedLayers | RenderAsTextShowAddresses | RenderAsTextShowIDAndClass | RenderAsTextShowLayerNesting;

    if (toShow & RenderAsTextPrinting)
        behavior |= RenderAsTextPrintingMode;

    return externalRepresentation(frame(), behavior);
}

WebString WebFrameImpl::markerTextForListItem(const WebElement& webElement) const
{
    return WebCore::markerTextForListItem(const_cast<Element*>(webElement.constUnwrap<Element>()));
}

void WebFrameImpl::printPagesWithBoundaries(WebCanvas* canvas, const WebSize& pageSizeInPixels)
{
    ASSERT(m_printContext);

    GraphicsContext graphicsContext(canvas);
    graphicsContext.setPrinting(true);

    m_printContext->spoolAllPagesWithBoundaries(graphicsContext, FloatSize(pageSizeInPixels.width, pageSizeInPixels.height));
}

WebRect WebFrameImpl::selectionBoundsRect() const
{
    return hasSelection() ? WebRect(IntRect(frame()->selection()->bounds(false))) : WebRect();
}

bool WebFrameImpl::selectionStartHasSpellingMarkerFor(int from, int length) const
{
    if (!frame())
        return false;
    return frame()->editor()->selectionStartHasMarkerFor(DocumentMarker::Spelling, from, length);
}

WebString WebFrameImpl::layerTreeAsText(bool showDebugInfo) const
{
    if (!frame())
        return WebString();

    LayerTreeFlags flags = showDebugInfo ? LayerTreeFlagsIncludeDebugInfo : 0;
    return WebString(frame()->layerTreeAsText(flags));
}

// WebFrameImpl public ---------------------------------------------------------

PassRefPtr<WebFrameImpl> WebFrameImpl::create(WebFrameClient* client)
{
    return adoptRef(new WebFrameImpl(client));
}

WebFrameImpl::WebFrameImpl(WebFrameClient* client)
    : FrameDestructionObserver(0)
// -------------------------------------------------- kikin Modification starts --------------------------------------------------------------
#if defined (SBROWSER_KIKIN)
    , m_currentlySelectedEntity(0)
#endif
// --------------------------------------------------- kikin Modification ends ---------------------------------------------------------------
    , m_frameLoaderClient(this)
    , m_client(client)
    , m_currentActiveMatchFrame(0)
    , m_activeMatchIndexInCurrentFrame(-1)
    , m_locatingActiveRect(false)
    , m_resumeScopingFromRange(0)
    , m_lastMatchCount(-1)
    , m_totalMatchCount(-1)
    , m_framesScopingCount(-1)
    , m_findRequestIdentifier(-1)
    , m_scopingInProgress(false)
    , m_lastFindRequestCompletedWithNoMatches(false)
    , m_nextInvalidateAfter(0)
    , m_findMatchMarkersVersion(0)
    , m_findMatchRectsAreValid(false)
    , m_identifier(generateFrameIdentifier())
    , m_inSameDocumentHistoryLoad(false)
#if defined(SBROWSER_LINKIFY_TEXT_SELECTION)
    , m_contentDetectionResult(WebContentDetectionResult())
#endif
{
    WebKit::Platform::current()->incrementStatsCounter(webFrameActiveCount);
    frameCount++;
}

WebFrameImpl::~WebFrameImpl()
{
    WebKit::Platform::current()->decrementStatsCounter(webFrameActiveCount);
    frameCount--;

    cancelPendingScopingEffort();
}

void WebFrameImpl::setWebCoreFrame(WebCore::Frame* frame)
{
    ASSERT(frame);
    observeFrame(frame);
}

void WebFrameImpl::initializeAsMainFrame(WebCore::Page* page)
{
    RefPtr<Frame> mainFrame = Frame::create(page, 0, &m_frameLoaderClient);
    setWebCoreFrame(mainFrame.get());

    // Add reference on behalf of FrameLoader.  See comments in
    // WebFrameLoaderClient::frameLoaderDestroyed for more info.
    ref();

    // We must call init() after m_frame is assigned because it is referenced
    // during init().
    frame()->init();
}

PassRefPtr<Frame> WebFrameImpl::createChildFrame(const FrameLoadRequest& request, HTMLFrameOwnerElement* ownerElement)
{
    RefPtr<WebFrameImpl> webframe(adoptRef(new WebFrameImpl(m_client)));

    // Add an extra ref on behalf of the Frame/FrameLoader, which references the
    // WebFrame via the FrameLoaderClient interface. See the comment at the top
    // of this file for more info.
    webframe->ref();

    RefPtr<Frame> childFrame = Frame::create(frame()->page(), ownerElement, &webframe->m_frameLoaderClient);
    webframe->setWebCoreFrame(childFrame.get());

    childFrame->tree()->setName(request.frameName());

    frame()->tree()->appendChild(childFrame);

    // Frame::init() can trigger onload event in the parent frame,
    // which may detach this frame and trigger a null-pointer access
    // in FrameTree::removeChild. Move init() after appendChild call
    // so that webframe->mFrame is in the tree before triggering
    // onload event handler.
    // Because the event handler may set webframe->mFrame to null,
    // it is necessary to check the value after calling init() and
    // return without loading URL.
    // (b:791612)
    childFrame->init(); // create an empty document
    if (!childFrame->tree()->parent())
        return 0;

    frame()->loader()->loadURLIntoChildFrame(request.resourceRequest().url(), request.resourceRequest().httpReferrer(), childFrame.get());

    // A synchronous navigation (about:blank) would have already processed
    // onload, so it is possible for the frame to have already been destroyed by
    // script in the page.
    if (!childFrame->tree()->parent())
        return 0;

    if (m_client)
        m_client->didCreateFrame(this, webframe.get());

    return childFrame.release();
}

void WebFrameImpl::didChangeContentsSize(const IntSize& size)
{
    // This is only possible on the main frame.
    if (m_totalMatchCount > 0) {
        ASSERT(!parent());
        ++m_findMatchMarkersVersion;
    }
}

#if defined (SBROWSER_PRINT_IMPL) || defined (SBROWSER_SAVEPAGE_IMPL)
void WebFrameImpl::paintForPrint(WebCanvas* canvas, const WebRect& rect)
{
	if (rect.isEmpty())
		return;
	IntRect dirtyRect(rect);
	//GraphicsContextBuilder builder(canvas);
	//GraphicsContext& graphicsContext = builder.context();
	GraphicsContext graphicsContext(canvas);
	graphicsContext.save();
	if (m_frame->document() && frameView()) {
		graphicsContext.clip(dirtyRect);
		frameView()->paintContents(&graphicsContext, dirtyRect);
		if (viewImpl()->pageOverlays())
			viewImpl()->pageOverlays()->paintWebFrame(graphicsContext);
	} else
		graphicsContext.fillRect(dirtyRect, Color::white, ColorSpaceDeviceRGB);
	graphicsContext.restore();
}
#endif

#if defined (SBROWSER_SOFTBITMAP_IMPL)
bool WebFrameImpl::isSoftBmpCaptured(){
     if (m_frame->document() && frameView()) {
        return frameView()->isSoftBmpCaptured();
     }
     return true;
}
#endif
void WebFrameImpl::createFrameView()
{
    TRACE_EVENT0("webkit", "WebFrameImpl::createFrameView");

    ASSERT(frame()); // If frame() doesn't exist, we probably didn't init properly.

    WebViewImpl* webView = viewImpl();
    bool isMainFrame = webView->mainFrameImpl()->frame() == frame();
    if (isMainFrame)
        webView->suppressInvalidations(true);

    frame()->createView(webView->size(), Color::white, webView->isTransparent(), webView->fixedLayoutSize(), isMainFrame ? webView->isFixedLayoutModeEnabled() : 0);
    if (webView->shouldAutoResize() && isMainFrame)
        frame()->view()->enableAutoSizeMode(true, webView->minAutoSize(), webView->maxAutoSize());

    if (isMainFrame)
        webView->suppressInvalidations(false);

    if (isMainFrame && webView->devToolsAgentPrivate())
        webView->devToolsAgentPrivate()->mainFrameViewCreated(this);
}

WebFrameImpl* WebFrameImpl::fromFrame(Frame* frame)
{
    if (!frame)
        return 0;
    return static_cast<FrameLoaderClientImpl*>(frame->loader()->client())->webFrame();
}

WebFrameImpl* WebFrameImpl::fromFrameOwnerElement(Element* element)
{
    // FIXME: Why do we check specifically for <iframe> and <frame> here? Why can't we get the WebFrameImpl from an <object> element, for example.
    if (!element || !element->isFrameOwnerElement() || (!element->hasTagName(HTMLNames::iframeTag) && !element->hasTagName(HTMLNames::frameTag)))
        return 0;
    HTMLFrameOwnerElement* frameElement = toFrameOwnerElement(element);
    return fromFrame(frameElement->contentFrame());
}

WebViewImpl* WebFrameImpl::viewImpl() const
{
    if (!frame())
        return 0;
    return WebViewImpl::fromPage(frame()->page());
}

WebDataSourceImpl* WebFrameImpl::dataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(dataSource());
}

WebDataSourceImpl* WebFrameImpl::provisionalDataSourceImpl() const
{
    return static_cast<WebDataSourceImpl*>(provisionalDataSource());
}

void WebFrameImpl::setFindEndstateFocusAndSelection()
{
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();

    if (this == mainFrameImpl->activeMatchFrame() && m_activeMatch.get()) {
        // If the user has set the selection since the match was found, we
        // don't focus anything.
        VisibleSelection selection(frame()->selection()->selection());
        if (!selection.isNone())
            return;

        // Try to find the first focusable node up the chain, which will, for
        // example, focus links if we have found text within the link.
        Node* node = m_activeMatch->firstNode();
        if (node && node->isInShadowTree()) {
            Node* host = node->deprecatedShadowAncestorNode();
            if (host->hasTagName(HTMLNames::inputTag) || host->hasTagName(HTMLNames::textareaTag))
                node = host;
        }
        while (node && !node->isFocusable() && node != frame()->document())
            node = node->parentNode();

        if (node && node != frame()->document()) {
            // Found a focusable parent node. Set the active match as the
            // selection and focus to the focusable node.
            frame()->selection()->setSelection(m_activeMatch.get());
            frame()->document()->setFocusedNode(node);
            return;
        }

        // Iterate over all the nodes in the range until we find a focusable node.
        // This, for example, sets focus to the first link if you search for
        // text and text that is within one or more links.
        node = m_activeMatch->firstNode();
        while (node && node != m_activeMatch->pastLastNode()) {
            if (node->isFocusable()) {
                frame()->document()->setFocusedNode(node);
                return;
            }
            node = NodeTraversal::next(node);
        }

        // No node related to the active match was focusable, so set the
        // active match as the selection (so that when you end the Find session,
        // you'll have the last thing you found highlighted) and make sure that
        // we have nothing focused (otherwise you might have text selected but
        // a link focused, which is weird).
        frame()->selection()->setSelection(m_activeMatch.get());
        frame()->document()->setFocusedNode(0);

        // Finally clear the active match, for two reasons:
        // We just finished the find 'session' and we don't want future (potentially
        // unrelated) find 'sessions' operations to start at the same place.
        // The WebFrameImpl could get reused and the m_activeMatch could end up pointing
        // to a document that is no longer valid. Keeping an invalid reference around
        // is just asking for trouble.
        m_activeMatch = 0;
    }
}

void WebFrameImpl::didFail(const ResourceError& error, bool wasProvisional)
{
    if (!client())
        return;
    WebURLError webError = error;
    if (wasProvisional)
        client()->didFailProvisionalLoad(this, webError);
    else
        client()->didFailLoad(this, webError);
}

void WebFrameImpl::setCanHaveScrollbars(bool canHaveScrollbars)
{
    frame()->view()->setCanHaveScrollbars(canHaveScrollbars);
}

void WebFrameImpl::invalidateArea(AreaToInvalidate area)
{
    ASSERT(frame() && frame()->view());
    FrameView* view = frame()->view();

    if ((area & InvalidateAll) == InvalidateAll)
        view->invalidateRect(view->frameRect());
    else {
        if ((area & InvalidateContentArea) == InvalidateContentArea) {
            IntRect contentArea(
                view->x(), view->y(), view->visibleWidth(), view->visibleHeight());
            IntRect frameRect = view->frameRect();
            contentArea.move(-frameRect.x(), -frameRect.y());
            view->invalidateRect(contentArea);
        }
    }

    if ((area & InvalidateScrollbar) == InvalidateScrollbar) {
        // Invalidate the vertical scroll bar region for the view.
        Scrollbar* scrollbar = view->verticalScrollbar();
        if (scrollbar)
            scrollbar->invalidate();
    }
}

void WebFrameImpl::addMarker(Range* range, bool activeMatch)
{
    frame()->document()->markers()->addTextMatchMarker(range, activeMatch);
}

void WebFrameImpl::setMarkerActive(Range* range, bool active)
{
    WebCore::ExceptionCode ec;
    if (!range || range->collapsed(ec))
        return;
    frame()->document()->markers()->setMarkersActive(range, active);
}

int WebFrameImpl::ordinalOfFirstMatchForFrame(WebFrameImpl* frame) const
{
    int ordinal = 0;
    WebFrameImpl* mainFrameImpl = viewImpl()->mainFrameImpl();
    // Iterate from the main frame up to (but not including) |frame| and
    // add up the number of matches found so far.
    for (WebFrameImpl* it = mainFrameImpl; it != frame; it = static_cast<WebFrameImpl*>(it->traverseNext(true))) {
        if (it->m_lastMatchCount > 0)
            ordinal += it->m_lastMatchCount;
    }
    return ordinal;
}

bool WebFrameImpl::shouldScopeMatches(const String& searchText)
{
    // Don't scope if we can't find a frame or a view.
    // The user may have closed the tab/application, so abort.
    // Also ignore detached frames, as many find operations report to the main frame.
    if (!frame() || !frame()->view() || !frame()->page() || !hasVisibleContent())
        return false;

    ASSERT(frame()->document() && frame()->view());

    // If the frame completed the scoping operation and found 0 matches the last
    // time it was searched, then we don't have to search it again if the user is
    // just adding to the search string or sending the same search string again.
    if (m_lastFindRequestCompletedWithNoMatches && !m_lastSearchString.isEmpty()) {
        // Check to see if the search string prefixes match.
        String previousSearchPrefix =
            searchText.substring(0, m_lastSearchString.length());

        if (previousSearchPrefix == m_lastSearchString)
            return false; // Don't search this frame, it will be fruitless.
    }

    return true;
}

void WebFrameImpl::scopeStringMatchesSoon(int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.append(new DeferredScopeStringMatches(this, identifier, searchText, options, reset));
}

void WebFrameImpl::callScopeStringMatches(DeferredScopeStringMatches* caller, int identifier, const WebString& searchText, const WebFindOptions& options, bool reset)
{
    m_deferredScopingWork.remove(m_deferredScopingWork.find(caller));
    scopeStringMatches(identifier, searchText, options, reset);

    // This needs to happen last since searchText is passed by reference.
    delete caller;
}

void WebFrameImpl::invalidateIfNecessary()
{
    if (m_lastMatchCount <= m_nextInvalidateAfter)
        return;

    // FIXME: (http://b/1088165) Optimize the drawing of the tickmarks and
    // remove this. This calculation sets a milestone for when next to
    // invalidate the scrollbar and the content area. We do this so that we
    // don't spend too much time drawing the scrollbar over and over again.
    // Basically, up until the first 500 matches there is no throttle.
    // After the first 500 matches, we set set the milestone further and
    // further out (750, 1125, 1688, 2K, 3K).
    static const int startSlowingDownAfter = 500;
    static const int slowdown = 750;

    int i = m_lastMatchCount / startSlowingDownAfter;
    m_nextInvalidateAfter += i * slowdown;
    invalidateArea(InvalidateScrollbar);
}

void WebFrameImpl::loadJavaScriptURL(const KURL& url)
{
    // This is copied from ScriptController::executeIfJavaScriptURL.
    // Unfortunately, we cannot just use that method since it is private, and
    // it also doesn't quite behave as we require it to for bookmarklets.  The
    // key difference is that we need to suppress loading the string result
    // from evaluating the JS URL if executing the JS URL resulted in a
    // location change.  We also allow a JS URL to be loaded even if scripts on
    // the page are otherwise disabled.

    if (!frame()->document() || !frame()->page())
        return;

    RefPtr<Document> ownerDocument(frame()->document());

    // Protect privileged pages against bookmarklets and other javascript manipulations.
    if (SchemeRegistry::shouldTreatURLSchemeAsNotAllowingJavascriptURLs(frame()->document()->url().protocol()))
        return;

    String script = decodeURLEscapeSequences(url.string().substring(strlen("javascript:")));
    ScriptValue result = frame()->script()->executeScript(script, true);

    String scriptResult;
    if (!result.getString(scriptResult))
        return;

    if (!frame()->navigationScheduler()->locationChangePending())
        frame()->document()->loader()->writer()->replaceDocument(scriptResult, ownerDocument.get());
}

void WebFrameImpl::willDetachPage()
{
    if (!frame() || !frame()->page())
        return;

    // Do not expect string scoping results from any frames that got detached
    // in the middle of the operation.
    if (m_scopingInProgress) {

        // There is a possibility that the frame being detached was the only
        // pending one. We need to make sure final replies can be sent.
        flushCurrentScopingEffort(m_findRequestIdentifier);

        cancelPendingScopingEffort();
    }
}

// SAMSUNG: Reader
#if defined(SBROWSER_READER_NATIVE)
bool WebFrameImpl::RegExpSearch(std::string SearchString, std::string SearchArray){
    std::string::size_type cutAt =0;
    while ((cutAt=SearchArray.find("|")) != std::string::npos) {
        if (cutAt > 0 ) {
            if (SearchString.find(SearchArray.substr(0, cutAt)) != std::string::npos)
                return true;
        }
        SearchArray = SearchArray.substr(cutAt+1);
    }
    if (SearchArray.length()>0) {
        if (SearchString.find(SearchArray) != std::string::npos)
            return true;
    }
    return false;
}

std::string WebFrameImpl::getActualInnerText(WebCore::Node* node){
    if (node->parentNode() && node->parentNode()->hasTagName(HTMLNames::scriptTag))
        return "";

    if (node->nodeType() == Node::TEXT_NODE)
        return node->nodeValue().utf8().data();

    std::string ActualInnerText = "";

    node = node->firstChild();

    while(node){
        ActualInnerText = ActualInnerText + getActualInnerText(node);
        node = node->nextSibling();
    }
    return ActualInnerText;
}

std::string WebFrameImpl::getInnerText(WebCore::Node* node){
    std::string textContent = "";
    Element* element = static_cast<Element*>(node);

    if (!node->textContent() && !element->innerText())
        return "";

    textContent = getActualInnerText(node);

    return textContent;
}

double WebFrameImpl::getLinkDensity(WebCore::Node* e) {
    if (e != NULL) {
        Element* element = static_cast<Element*>(e);
        RefPtr<NodeList> links = e->getElementsByTagName("a");
        double textLength = element->innerText().length();
        double linkLength = 0.0;

        for (int i=0, il=links->length(); i<il;i+=1) {
            WebCore::HTMLLinkElement *linkElement = static_cast<WebCore::HTMLLinkElement *> ( links->item ( i ) );
            linkLength += linkElement->innerText().length();
        }
        return linkLength / textLength;
    }
    return 0;
}

int WebFrameImpl::getClassWeight(WebCore::Node* node){
    int weight = 0;
    if (node != NULL){
        Element* element = static_cast<Element*>(node);

        const String classAttributeName = "class";
        const String regexPostiveString =  "article|body|content|entry|hentry|main|page|pagination|post|text|blog|story|windowclassic";
        const String regexNegativeString =  "contents|combx|comment|com-|contact|foot|footer|footnote|masthead|media|meta|outbrain|promo|related|scroll|shoutbox|sidebar|date|sponsor|shopping|tags|script|tool|widget|scbox|rail|reply|div_dispalyslide|galleryad|disqus_thread|cnn_strylftcntnt|topRightNarrow|fs-stylelist-thumbnails|replText|ttalk_layer|disqus_post_message|disqus_post_title|cnn_strycntntrgt|wpadvert|sharedaddy sd-like-enabled sd-sharing-enabled|fs-slideshow-wrapper|fs-stylelist-launch|fs-stylelist-next|fs-thumbnail-194230|reply_box|textClass errorContent|mainHeadlineBrief|mainSlideDetails|curvedContent|photo|home_|XMOD";    

        if (element->hasAttribute(classAttributeName)) {
            const String classes = element->getAttribute(classAttributeName).string();
            RegularExpression posClassRegex(regexPostiveString, WTF::TextCaseInsensitive);
            int matchLength = 0;
            int matchIndex = posClassRegex.match(classes, 0, &matchLength);
            if (matchIndex != -1)
                weight += 30;

            RegularExpression negClassRegex(regexNegativeString, WTF::TextCaseInsensitive);
            matchIndex = negClassRegex.match(classes, 0, &matchLength);
            if (matchIndex != -1)
                weight -= 25;
        }
        // check if element has an attribute of name class
        if (element->hasAttribute(WebCore::HTMLNames::idAttr)) {
            const String id = element->getAttribute(WebCore::HTMLNames::idAttr).string();
            RegularExpression posIdRegex(regexPostiveString, WTF::TextCaseInsensitive);
            int matchLength = 0;
            int matchIndex = posIdRegex.match(id, 0, &matchLength);
            if (matchIndex != -1)
                weight += 30;

            RegularExpression negIdRegex(regexNegativeString, WTF::TextCaseInsensitive);
            matchIndex = negIdRegex.match(id, 0, &matchLength);
            if (matchIndex != -1)
                weight -= 25;
        }
    }
    return weight;
}


bool WebFrameImpl::ChineseJapaneseKorean(char innerCharacter) {
    if (!innerCharacter || sizeof(innerCharacter) == 0) return false;

    unsigned long innerCharacterCode = innerCharacter;

    if (innerCharacterCode > 11904 && innerCharacterCode < 12031) return true;
    if (innerCharacterCode > 12352 && innerCharacterCode < 12543) return true;
    if (innerCharacterCode > 12736 && innerCharacterCode < 19903) return true;
    if (innerCharacterCode > 19968 && innerCharacterCode < 40959) return true;
    if (innerCharacterCode > 44032 && innerCharacterCode < 55215) return true;
    if (innerCharacterCode > 63744 && innerCharacterCode < 64255) return true;
    if (innerCharacterCode > 65072 && innerCharacterCode < 65103) return true;
    if (innerCharacterCode > 131072 && innerCharacterCode < 173791) return true;
    if (innerCharacterCode > 194560 && innerCharacterCode < 195103) return true;

    return false;
}

void WebFrameImpl::initializeNode(WebCore::Node* node) {

    Element* element = static_cast<Element*>(node);
    double score = 0;
    const char* tagName = element-> tagName().utf8().data();

    if (strcmp(tagName,"DIV") == 0) {
        score = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
        score += 5;
        element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(score));
    } else if (strcmp(tagName,"ARTICLE") == 0) {
        score = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
        score += 25;
        element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(score));
    } else if ((strcmp(tagName,"PRE") == 0) || (strcmp(tagName,"TD") == 0)
            || (strcmp(tagName,"BLOCKQUOTE") == 0) ) {
        score = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
        score += 3;
        element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(score));
    } else if ((strcmp(tagName,"ADDRESS") == 0) || (strcmp(tagName,"UL") == 0)
            || (strcmp(tagName,"DL") == 0) || (strcmp(tagName,"DD") == 0)
            || (strcmp(tagName,"DT") == 0) || (strcmp(tagName,"LI") == 0)
            || (strcmp(tagName,"FORM") == 0)) {
        score = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
        score -= 3;
        element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(score));
    } else if ((strcmp(tagName,"H1") == 0) || (strcmp(tagName,"H2") == 0)
             || (strcmp(tagName,"H3") == 0) || (strcmp(tagName,"H4") == 0)
             || (strcmp(tagName,"H5") == 0) || (strcmp(tagName,"H6") == 0)
             || (strcmp(tagName,"TH") == 0)) {
        score = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
        score -= 5;
        element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(score));
    }
    double totalWeightValue = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt() + getClassWeight(node);
    element->setAttribute(WebCore::HTMLNames::readabilityAttr,WTF::String::number(totalWeightValue).utf8().data());
}


String WebFrameImpl::GetStyle(WebCore::Node* node, WTF::String CssProperty){
    Element* element = static_cast<Element*>(node);

    String strValue = "";

    if (frame()->document()->domWindow()->getComputedStyle(element, "")) {
        RefPtr<WebCore::CSSStyleDeclaration> cssStyleDec = (frame()->document()->domWindow()->getComputedStyle(element, ""));
        strValue = cssStyleDec->getPropertyValue(CssProperty);
    }
    return strValue;
}

bool WebFrameImpl::isVisibleNode(WebCore::Node* CandidateNode) {
    unsigned count = 0;
    while (CandidateNode != NULL) {
        if ((GetStyle(CandidateNode, "display")=="none") || (GetStyle(CandidateNode, "left")== "100%") || (GetStyle(CandidateNode, "clip")=="rect(0px 0px 0px 0px)")) {
            return false;
        }
        CandidateNode = CandidateNode->parentNode();
        count++;
        if(count>=4)
            break;
    }
    return true;

}

bool WebFrameImpl::checkIfNodeIsHidden(WebCore::Node* node){
    Element* element = static_cast<Element*>(node);
    RefPtr<ClientRect> rect = element->getBoundingClientRect();
    if (rect->height() == 0 && rect->width() == 0) {
        return true;
    }
    return false;
}


// SAMSUNG: Reader
bool WebFrameImpl::RecognizeArticle(int mode){
    //select 1 : simple patch
    //select 2 : javascript -> native
    unsigned select = mode;

    if (select == 1) {

        //simple RecognizeArticle
        std::string homepage ="?mview=desktop|?ref=smartphone|apple.com|query=|search?|?from=mobile|signup|twitter|facebook|youtube|?f=mnate|linkedin|romaeo|chrome:|gsshop|gdive|?nytmobile=0|?CMP=mobile_site|?main=true|home-page|anonymMain";

        HTMLElement* mainBody = NULL;

        if (m_frame)
           mainBody =  m_frame->document()->body();

        if (!mainBody)
            return false;

        String url = m_frame->document()->baseURI().string();
        std::string baseurl = url.utf8().data();
#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"baseurl : "<<baseurl;
#endif

        std::string::size_type findOffset = 0;
        if (baseurl.length()>=8) {
            findOffset = baseurl.find( "/", 7 );
            if (std::string::npos == findOffset)
                findOffset=0;
        }
        findOffset++;
        String hostName = url.substring(0,(unsigned)findOffset);
#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"hostName : "<<hostName.utf8().data();;
#endif
        if (url==hostName) {
#if defined(SBROWSER_READER_DEBUG)
            VLOG(0)<<"same hostname!";
#endif
            return false;
        }

        if (RegExpSearch(baseurl, homepage)) {
#if defined(SBROWSER_READER_DEBUG)
            VLOG(0)<<"Find homepage!";
#endif
            return false;
        }

#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"RecognizeArticle checkpoint 2";
#endif
        HTMLElement* page = mainBody;


#if defined(SBROWSER_READER_DEBUG)
        clock_t begin2, end2;
        begin2 = clock();
#endif
        WebCore::Node* node = NULL;
        int brCountCurr = 0;
        int maxBrCount = 0;
        RefPtr<WebCore::NodeList> brElements = page->getElementsByTagName("br");
        for (unsigned nodeIndex = 0; nodeIndex < brElements->length(); nodeIndex++) {
            node = static_cast<WebCore::Node*>(brElements->item(nodeIndex));
            while (node->nextSibling()) {
                node = node->nextSibling();
                if (node->hasTagName(WebCore::HTMLNames::brTag))
                    brCountCurr++;
            }

            if (brCountCurr > maxBrCount)
                maxBrCount = brCountCurr;
            brCountCurr = 0;
        }

        int pCountCurr = 0;
        int maxPCount = 0;
        RefPtr<WebCore::NodeList> pElements = page->getElementsByTagName("p");
        for (unsigned nodeIndex = 0; nodeIndex < pElements->length(); nodeIndex++){
            node = static_cast<WebCore::Node*>(pElements->item(nodeIndex));
            while (node->nextSibling()){
                node = node->nextSibling();
                if (node->hasTagName(WebCore::HTMLNames::pTag))
                    pCountCurr++;
            }

            if (pCountCurr > maxPCount)
                maxPCount = pCountCurr;
            pCountCurr = 0;
        }

        RefPtr<WebCore::NodeList> preElements = page->getElementsByTagName("pre");
        int maxPreCount = preElements->length();


#if defined(SBROWSER_READER_DEBUG)
        end2 = clock();
        VLOG(0)<<"p and br search Time : "<<1000.0*((double)(end2 - begin2)/CLOCKS_PER_SEC)<<"ms";
        VLOG(0)<<"maxBrCount : "<<maxBrCount;
        VLOG(0)<<"maxPCount : "<<maxPCount;
        VLOG(0)<<"maxPreCount : "<<maxPreCount;
#endif

        if (maxBrCount <= 1 && maxPCount <= 1 && maxPreCount==0 )
            return false;



#if defined(SBROWSER_READER_DEBUG)
        clock_t begin, end;
        begin = clock();
#endif

        double mainBodyTextLength = page->innerText().length();

#if defined(SBROWSER_READER_DEBUG)
        end = clock();
        VLOG(0)<<"textLength Time : "<<1000.0*((double)(end - begin))/CLOCKS_PER_SEC<<"ms";
#endif


        if(mainBodyTextLength < 400)
            return false;


        double linkTextLength = 1;

#if defined(SBROWSER_READER_DEBUG)
        clock_t begin1, end1;
        begin1 = clock();
#endif

        RefPtr<WebCore::NodeList> linkElements = page->getElementsByTagName("a");
        for (unsigned nodeIndex = 0; nodeIndex < linkElements->length(); nodeIndex++) {
            node = static_cast<WebCore::Node*>(linkElements->item(nodeIndex));
            Element* element = static_cast<Element*>(node);
            if (node->isFocusable()) {
                double eachLength = element->innerText().length();
                linkTextLength = linkTextLength + eachLength;
            }
        }

#if defined(SBROWSER_READER_DEBUG)
        end1 = clock();
        VLOG(0)<<"link Time : "<<1000.0*((double)(end1 - begin1)/CLOCKS_PER_SEC)<<"ms";
#endif

        if (mainBodyTextLength==0.0)
            return false;

        double linkDensity =  linkTextLength/mainBodyTextLength;

#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"mainBodyTextLength : "<<mainBodyTextLength;
        VLOG(0)<<"linkTextLength : "<<linkTextLength;
        VLOG(0)<<"linkDensity : "<<linkDensity;
#endif

        if ((mainBodyTextLength>4000 && linkDensity < 0.63) || (mainBodyTextLength>3000 && linkDensity < 0.58) ||
            (mainBodyTextLength>2500 && linkDensity < 0.6) || (linkDensity < 0.5)) {
            return true;
        } else if ((linkDensity < 0.68 && linkDensity > 0.5) && (maxBrCount >= 5 || maxPCount >= 5)) {
            return true;
        } else {
            return false;
        }
        //simple RecognizeArticle
    }
    else if (select==2) {

        std::string divToPElements ="a|blockquote|dl|div|img|ol|p|pre|table|ul|script|article|form";
        const String regexUnlikelyCandidatesString ="combx|comment|community|disqus|extra|foot|header|menu|remark|rss|shoutbox|sidebar|sponsor|ad-break|agegate|pagination|pager|popup|tweet|twitter";
        const String regexOkMayBeCandidateString ="and|article|body|column|main|shadow";
        const String classAttributeName = "class";
        std::string homepage ="?mview=desktop|?ref=smartphone|apple.com|query=|search?|?from=mobile|signup|twitter|facebook|youtube|?f=mnate|linkedin|romaeo|chrome:|?mobile|gsshop|gdive|?nytmobile=0|?CMP=mobile_site|?main=true|home-page|anonymMain";

        bool CJK = false;
        HTMLElement* mainBody = NULL;

        if (m_frame)
           mainBody =  m_frame->document()->body();

        if (!mainBody)
            return false;

        HTMLElement* page = mainBody;

        String url = m_frame->document()->baseURI().string();
        std::string baseurl = url.utf8().data();
#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"baseurl : "<<baseurl;
#endif


        std::string::size_type findOffset = 0;
        if (baseurl.length()>=8) {
            findOffset = baseurl.find( "/", 7 );
            if (std::string::npos == findOffset)
                findOffset=0;
        }
        findOffset++;
        String hostName = url.substring(0,(unsigned)findOffset);
#if defined(SBROWSER_READER_DEBUG)
        VLOG(0)<<"hostName : "<<hostName.utf8().data();
#endif
        if (url==hostName) {
#if defined(SBROWSER_READER_DEBUG)
            VLOG(0)<<"same hostname!";
#endif
            return false;
        }

        if (RegExpSearch(baseurl, homepage)) {
#if defined(SBROWSER_READER_DEBUG)
            VLOG(0)<<"Find homepage!";
#endif
            return false;
        }

#if defined(SBROWSER_READER_DEBUG)
        clock_t begin1, end1;
        begin1 = clock();
#endif


        RefPtr<Element> RecogDiv = m_frame->document()->createElement(WebCore::HTMLNames::divTag, false);

        RecogDiv->setAttribute(WebCore::HTMLNames::idAttr, "recog_div");
        RecogDiv->setAttribute(WebCore::HTMLNames::styleAttr, "display:none;");

        RefPtr<WebCore::NodeList> allElements = page->getElementsByTagName("*");

        WebCore::Node* node = NULL;
        Vector <WebCore::Node*> nodesToScore;


        for (unsigned nodeIndex = 0; nodeIndex < allElements->length(); nodeIndex++) {

            bool isUnlikelyCandidate = false;
            bool isOkItMayBeCandidate = false;

            node = static_cast<WebCore::Node*>(allElements->item(nodeIndex));
            Element* element = static_cast<Element*>(node);

            const String classes = element->getAttribute(classAttributeName).string();
            const String id = element->getAttribute(WebCore::HTMLNames::idAttr).string();

            RegularExpression unlikelyClassRegex(regexUnlikelyCandidatesString, WTF::TextCaseInsensitive);
            RegularExpression unlikelyIdRegex(regexUnlikelyCandidatesString, WTF::TextCaseInsensitive);
            int matchLength = 0;
            int classMatchIndex = unlikelyClassRegex.match(classes, 0, &matchLength);
            int idMatchIndex = unlikelyIdRegex.match(id, 0, &matchLength);

            if (classMatchIndex != -1 || idMatchIndex != -1)
                isUnlikelyCandidate = true;

            RegularExpression okMayBeClassRegex(regexOkMayBeCandidateString, WTF::TextCaseInsensitive);
            RegularExpression okMayBeIdRegex(regexOkMayBeCandidateString, WTF::TextCaseInsensitive);

            int classOkMatchIndex = okMayBeClassRegex.match(classes, 0, &matchLength);
            int idOkMatchIndex = okMayBeIdRegex.match(id, 0, &matchLength);
            if (classOkMatchIndex == -1 || idOkMatchIndex == -1)
                isOkItMayBeCandidate = true;

            if (isUnlikelyCandidate && !isOkItMayBeCandidate && !(node->hasTagName(WebCore::HTMLNames::bodyTag))) {
                continue;
            }


            if ((GetStyle(node, "display"))=="none"  || (GetStyle(node, "clip"))=="rect(0px 0px 0px 0px)") {
                continue;
            }

            if (checkIfNodeIsHidden(node)) {
                continue;
            }

            if (node->hasTagName(WebCore::HTMLNames::pTag) || node->hasTagName(WebCore::HTMLNames::ulTag)
                 || (node->hasTagName(WebCore::HTMLNames::tdTag) && (node->getElementsByTagName("table")->length() == 0))
                 || node->hasTagName(WebCore::HTMLNames::preTag)) {
                nodesToScore.append(node);
            }
            if (node->nodeType() == Node::COMMENT_NODE){
            }
            if (node->hasTagName(WebCore::HTMLNames::divTag)) {
                HTMLElement * divElement = static_cast<HTMLElement *>(node);
                std::string elementInnerHTML = divElement->innerHTML().utf8().data();
                if (!RegExpSearch(elementInnerHTML, divToPElements) && node->parentNode() != NULL) {

                    bool isUnlikelyParentCandidate = false;
                    bool isOkItMayBeParentCandidate = false;

                    WebCore::Node* ParentNode = node->parentNode();
                    Element* parentElement = static_cast<Element*>(ParentNode);

                    const String parentClasses = parentElement->getAttribute(classAttributeName).string();
                    const String parentId = parentElement->getAttribute(WebCore::HTMLNames::idAttr).string();

                    RegularExpression parentUnlikelyClassRegex(regexUnlikelyCandidatesString, WTF::TextCaseInsensitive);
                    RegularExpression parentUnlikelyIdRegex(regexUnlikelyCandidatesString, WTF::TextCaseInsensitive);
                    int matchLength = 0;
                    int parentClassMatchIndex = parentUnlikelyClassRegex.match(parentClasses, 0, &matchLength);
                    int parentIdMatchIndex = parentUnlikelyIdRegex.match(parentId, 0, &matchLength);

                    if (parentClassMatchIndex != -1 || parentIdMatchIndex != -1)
                        isUnlikelyParentCandidate = true;

                    RegularExpression parentOkMayBeClassRegex(regexOkMayBeCandidateString, WTF::TextCaseInsensitive);
                    RegularExpression parentOkMayBeIdRegex(regexOkMayBeCandidateString, WTF::TextCaseInsensitive);

                    int parentClassOkMatchIndex = parentOkMayBeClassRegex.match(parentClasses, 0, &matchLength);
                    int parentIdOkMatchIndex = parentOkMayBeIdRegex.match(parentId, 0, &matchLength);
                    if (parentClassOkMatchIndex == -1 || parentIdOkMatchIndex == -1)
                        isOkItMayBeParentCandidate = true;

                    if (isUnlikelyParentCandidate && !isOkItMayBeParentCandidate && !(node->hasTagName(WebCore::HTMLNames::bodyTag))) {
                        continue;
                    }

                    nodesToScore.append(node);
                }
                else {
                    PassRefPtr<NodeList> childNodesList = node->childNodes();
                    for (unsigned int i = 0; i < childNodesList->length() ; i++) {
                        WebCore::Node* ChildNode = childNodesList->item(i);

                        if (ChildNode->nodeType() == Node::TEXT_NODE) {
                            nodesToScore.append(ChildNode);
                        }
                    }
                }
            }
        }

#if defined(SBROWSER_READER_DEBUG)
        end1 = clock();
        VLOG(0)<<"first loop Time : "<<1000*((double)(end1 - begin1)/CLOCKS_PER_SEC)<<"ms";

        clock_t begin2, end2;
        begin2 = clock();
#endif


        Vector <WebCore::Node*> CandidateNodes;

        for (unsigned nodeIndex = 0; nodeIndex < nodesToScore.size(); nodeIndex++){
            WebCore::Node* ParentNode = nodesToScore.at(nodeIndex)->parentNode();
            WebCore::Node* GrandParentNode = ParentNode? ParentNode->parentNode():NULL;
            std::string InnerText = getInnerText(nodesToScore[nodeIndex]);
            Element* parentElement = static_cast<Element*>(ParentNode);
            Element* grandParentElement = NULL;
            if (GrandParentNode)
                grandParentElement =  static_cast<Element*>(GrandParentNode);

            if (!ParentNode)
                continue;

            if (InnerText.length() < 30)
                continue;

            if (strcmp(parentElement->fastGetAttribute(WebCore::HTMLNames::readabilityAttr).string().utf8().data(),"")==0) {
                initializeNode(ParentNode);
                CandidateNodes.append(ParentNode);
            }
            if (grandParentElement && strcmp(grandParentElement->fastGetAttribute(WebCore::HTMLNames::readabilityAttr).string().utf8().data(),"")==0) {
                initializeNode(GrandParentNode);
                CandidateNodes.append(GrandParentNode);
            }

            double contentScore = 0;

            contentScore+=1;

            Vector<String> InnerTextSplitList;
            const char* it = InnerText.c_str();
            WTF::String(it,InnerText.length()).split(',', InnerTextSplitList); //allowEmptyEntries is false in the default scenario

            contentScore += InnerTextSplitList.size();

            int innerTextLength = 0;

            for (unsigned i = 0; i < InnerText.length(); i++) {
                if (ChineseJapaneseKorean(InnerText[i])) {
                    innerTextLength ++;
                    CJK = true;
                }
            }

            if (CJK) {
                contentScore += std::min(floor((float)(InnerText.length())/100),(float)3);
                contentScore = contentScore * 3;
            }
            else {
                if (InnerText.length() < 25)
                    continue;
                contentScore += std::min((float)floor((float)(InnerText.length())/100),(float)3);
            }

            if (parentElement){
                double ParentNodeScore = parentElement->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
                if (strcmp(parentElement->fastGetAttribute(WebCore::HTMLNames::readabilityAttr).string().utf8().data(),"")!=0) {
                    ParentNodeScore += contentScore;
                    static_cast<Element*>(ParentNode)->setAttribute(WebCore::HTMLNames::readabilityAttr, WTF::String::number(ParentNodeScore));
                }
            }
            if (grandParentElement) {
                double GrandParentNodeScore = grandParentElement->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
                if (grandParentElement && strcmp(grandParentElement->fastGetAttribute(WebCore::HTMLNames::readabilityAttr).string().utf8().data(),"")!=0) {
                    GrandParentNodeScore += contentScore/2;
                    static_cast<Element*>(GrandParentNode)->setAttribute(WebCore::HTMLNames::readabilityAttr, WTF::String::number(GrandParentNodeScore));
                }
            }
        }

#if defined(SBROWSER_READER_DEBUG)
        end2 = clock();
        VLOG(0)<<"second loop Time : "<<1000*((double)(end2 - begin2)/CLOCKS_PER_SEC)<<"ms";

        clock_t begin3, end3;
        begin3 = clock();
#endif

        WebCore::Node* topCandidate = NULL;
        Element* element = NULL;
        for (unsigned nodeIndex = 0; nodeIndex < CandidateNodes.size(); nodeIndex++) {
            double topCandidateScore;
            WebCore::Node* CandidateNode = CandidateNodes.at(nodeIndex);
            Element* candidateElement = static_cast<Element*>(CandidateNode);
            double CandidateNodeScore = candidateElement->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
            if (topCandidate)
                topCandidateScore = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();

            CandidateNodeScore = CandidateNodeScore * (1-getLinkDensity(CandidateNode));

            static_cast<Element*>(CandidateNode)->setAttribute(WebCore::HTMLNames::readabilityAttr, WTF::String::number(CandidateNodeScore));

            if (!topCandidate || CandidateNodeScore > topCandidateScore){
                topCandidate = CandidateNode;
                element = static_cast<Element*>(topCandidate);
            }
        }

#if defined(SBROWSER_READER_DEBUG)
        end3 = clock();
        VLOG(0)<<"third loop Time : "<<1000*((double)(end3 - begin3)/CLOCKS_PER_SEC)<<"ms";

        clock_t begin4, end4;
        begin4 = clock();
#endif

        int NeighbourCandidates = 0;
        for (unsigned nodeIndex = 0; nodeIndex < CandidateNodes.size(); nodeIndex++) {
            double topCandidateScore = 0.0f;
            WebCore::Node* CandidateNode = CandidateNodes.at(nodeIndex);
            Element* candidateElement = static_cast<Element*>(CandidateNode);
            double CandidateNodeScore = candidateElement->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
            if (topCandidate)
                topCandidateScore = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();

            if ((CandidateNodeScore >= topCandidateScore*0.85) && (CandidateNode != topCandidate)) {
                NeighbourCandidates++;
            }
        }

#if defined(SBROWSER_READER_DEBUG)
        end4 = clock();
        VLOG(0)<<"fourth loop Time : "<<1000*((double)(end4 - begin4)/CLOCKS_PER_SEC)<<"ms";
#endif
        int numberOfTrs = 0;

        if (topCandidate) {
            if (topCandidate->hasTagName(WebCore::HTMLNames::trTag) || topCandidate->hasTagName (WebCore::HTMLNames::tbodyTag)) {
                RefPtr<WebCore::NodeList> topcandidateTR = topCandidate->getElementsByTagName("tr");
                numberOfTrs = topcandidateTR->length();
            }
        }
        if (NeighbourCandidates > 2) {
            //disabling reader icon
            return false;
        }
        else if (!isVisibleNode(topCandidate) && NeighbourCandidates == 0) {
            // control will come here if there is no other nodes which can be considered as top candidate, & topcandidate is not visible to user.
            return false;
        }
        else if ((getLinkDensity(topCandidate) > 0.5)) {
            //disabling reader icon due to higher link density in topCandidate
            return false;
        }
        else if (topCandidate == NULL || topCandidate->hasTagName(WebCore::HTMLNames::bodyTag)
            || topCandidate->hasTagName(WebCore::HTMLNames::formTag)){
            return false;
            //disabling reader icon as invalid topCandidate
        }
        else {
            Vector<String> TopCandidateInnerTextSplitList;
            String elInnerText = static_cast<Element*>(topCandidate)->innerText();
            elInnerText.split(',', TopCandidateInnerTextSplitList);
            int splitLength = TopCandidateInnerTextSplitList.size();
            double readerScore;
            readerScore = element->getAttribute(WebCore::HTMLNames::readabilityAttr).string().toUInt();
            int readerTrs = numberOfTrs;
            int readerTextLength = element->innerText().length();
            int readerPlength = topCandidate->getElementsByTagName("p")->length();
#if defined(SBROWSER_READER_DEBUG)
            VLOG(0)<<"Reader Score "<< readerScore << "-textLength : "<<readerTextLength<<" Trs : "<<readerTrs<<", Plength : "<<readerPlength<<", splitLength : "<<splitLength;
#endif
            if ((readerScore >= 40 && readerTrs < 3 )
                || (readerScore >= 20 && readerScore < 30 && readerTextLength >900 && readerPlength >=2 && readerTrs < 3 && !CJK)
                || (readerScore >= 20 && readerScore < 30 && readerTextLength >1900 && readerPlength >=0 && readerTrs < 3 && !CJK)
                || (readerScore > 15 && readerScore <=40  && splitLength >=100 && readerTrs < 3)
                || (readerScore >= 100 && readerTextLength >2000  && splitLength >=250 && readerTrs > 200))
            {
                if (readerScore >= 40 && readerTextLength < 100) {
                    return false;
                }
                else {
                    return true;
                }
            }
            else {
                return false;
            }
        }
        return false;
    }else {
        return false;
    }
}
#endif

// -------------------------------------------------- kikin Modification starts --------------------------------------------------------------
#if defined (SBROWSER_KIKIN)

// static
VisibleSelection WebFrameImpl::getKikinSelection(Frame* frame, const VisibleSelection& selection)
{
    WebFrameImpl* webFrame = fromFrame(frame);
    if (!webFrame)
        return selection;

    WebFrameImpl* mainWebFrame = webFrame->viewImpl()->mainFrameImpl();
    if (!mainWebFrame)
        return selection;

    // reset the currently selected entity
    if (mainWebFrame->m_currentlySelectedEntity) {
        mainWebFrame->m_currentlySelectedEntity->resetWordsAround();
    }

    mainWebFrame->m_currentlySelectedEntity = 0;

    // if selection is not a range, don't do anything
    if (!selection.isRange())
        return selection;

    VisibleSelection kikinSelection(selection);
    RefPtr<kikin::HtmlEntity> entity = kikin::HtmlAnalyzer::analyze(selection.firstRange());

    if (entity) {
        VisibleSelection selection;
        if (WebFrameImpl::getSelectionFromKikinEntity(entity, selection)) {
            kikinSelection = selection;
        }
                                               
        // save the selection.
        entity->saveWordsAround();                      
        mainWebFrame->m_currentlySelectedEntity = entity;
    }

    return kikinSelection;
}

// static
bool WebFrameImpl::getSelectionFromKikinEntity(PassRefPtr<kikin::HtmlEntity> entity, VisibleSelection& selection) {
    if (entity) {
        Range* firstRange = entity->getFirstRange()->getRange();
        Range* lastRange = entity->getLastRange()->getRange();

        if (firstRange->startContainer() != lastRange->endContainer() || firstRange->startOffset() != lastRange->endOffset()) {
            // For some reason in WebKit if text is selected using range in the paragraph which has
            // first-letter property, text is selected offset by one character. For e.g.
            // 1. Go to desktop version of http://www.faz.net/aktuell/politik/ausland/asien/nordkorea-washington-warnt-pjoengjang-vor-weiteren-provokationen-12144205.html
            // 2. Select the text in first paragraph where first letter 'D' in sentence 'Die amerikanische Regierung hat
            //    ihre Warnung an das Regime in Pjöngjang vor weiteren Provokationen verschärft' is spanned in two lines
            // 3. Text is always going to be offset by one character.

            // FIX: Check if node has the property for first-letter. If so, then ignore the selection for first-letter

            VisiblePosition startPosition;
            RenderStyle* startContainerStyle = firstRange->startContainer()->renderStyle();
            if (startContainerStyle && startContainerStyle->hasPseudoStyle(FIRST_LETTER)) {
                startPosition = VisiblePosition(Position(firstRange->startContainer(), (firstRange->startOffset() - 1),
                        Position::PositionIsOffsetInAnchor), DOWNSTREAM);
            } else {
                startPosition = VisiblePosition(Position(firstRange->startContainer(), firstRange->startOffset(),
                        Position::PositionIsOffsetInAnchor), DOWNSTREAM);
            }

            VisiblePosition endPosition;
            RenderStyle* endContainerStyle = lastRange->endContainer()->renderStyle();
            if (endContainerStyle && endContainerStyle->hasPseudoStyle(FIRST_LETTER)) {
                endPosition = VisiblePosition(Position(lastRange->endContainer(), (lastRange->endOffset() - 1),
                        Position::PositionIsOffsetInAnchor), DOWNSTREAM);
            } else {
                endPosition = VisiblePosition(Position(lastRange->endContainer(), lastRange->endOffset(),
                        Position::PositionIsOffsetInAnchor), DOWNSTREAM);
            }

            // create the selection
            selection = VisibleSelection(startPosition, endPosition);
            return true;
        }
    }
    
    return false;
}
                          
WTF::String WebFrameImpl::retrieveTitle() {

    if (viewImpl() && viewImpl()->mainFrameImpl() && viewImpl()->mainFrameImpl()->frame() && viewImpl()->mainFrameImpl()->frame()->document() != 0) {
        return viewImpl()->mainFrameImpl()->frame()->document()->title();
    }

    return WTF::String();    
}

WTF::String WebFrameImpl::retrieveKeywords() {

    if (!viewImpl() || !viewImpl()->mainFrameImpl() || !viewImpl()->mainFrameImpl()->frame() || !viewImpl()->mainFrameImpl()->frame()->document() == 0) {
        return WTF::String();    
    }
    
    RefPtr<NodeList> list = viewImpl()->mainFrameImpl()->frame()->document()->getElementsByTagName(HTMLNames::metaTag.localName());
    unsigned len = list->length();
    
    for (unsigned i = 0; i < len; i++) {
        WebCore::HTMLMetaElement* meta = static_cast<WebCore::HTMLMetaElement*>(list->item(i));
        if (meta->name().lower() == "keywords") {
            return meta->content();
        }
    }

    return WTF::String();
}

WTF::String WebFrameImpl::retrieveDocumentText() {

    if (!frame() || !frame()->document()) {
        return WTF::String();
        
    }

    WebCore::HTMLElement* body = frame()->document()->body();
    if (!body) {
        return WTF::String();
    }

    String innerText = body->innerText().replace("\r\n", " ")
        .replace("\r", " ")
        .replace("\n", " ")
        .replace("\t", " ")
        .replace("  ", " ");
        
    // If there are more than 10000 characters in the document than
    // try to send the document text nearby the selection.
    if (innerText.length() > 10000) {

        // If we try to get the 10000 characters by iterating through nodes, then we will
        // slow the page and process of fetching the kikin results. That's why we are using
        // a optimal solution to guess the nearby text by getting the index of prehl, selection and posthl.
        // There is less probability that the 50 characters before the selection +
        // selection + 50 characters after the selection would be repeated again in the
        // document.
        
        WTF::String selectedAndNearbyText = retrievePreviousText() + getSelection() + retrievePostText();
        int length = selectedAndNearbyText.length();

        // If there is no selection, then return first 10000 characters on the page.
        if (length > 0) {
            // get the start index of the selected and nearby text
            int startIndex = innerText.find(selectedAndNearbyText);

            // if we could not find the selected and nearby text in the document then
            // return first 10000 characters on the page
            if (startIndex > 0) {

                // try to get approx. 5000 characters before and after the selection.
                int charactersOnEitherSide = (10000 - length) / 2;
                int startPos = startIndex - charactersOnEitherSide;
                unsigned endPos = startIndex + length + charactersOnEitherSide;

                // if selection is within the first 5000 characters of the page,
                // then send first 10000 characters.
                if (startPos < 0) {
                    return innerText.left(10000);
                } else if (endPos > innerText.length()) {
                    // if selection is within the last 5000 characters of the page,
                    // then send last 10000 characters of the page.
                    return innerText.right(10000);
                } else {
                    // send approx. 5000 characters before and after the selection.
                    return innerText.substring(startPos, 10000);
                }
            }
        }

        // send first 10000 characters on the page.
        return innerText.left(10000);
    }

    // send the whole text if there are less than 10000 characters on the page.
    return innerText;
}

WTF::String WebFrameImpl::getInnerText(WebCore::HTMLElement* element) {
    if (element) {
        String textContent = element->textContent();
        if (!textContent.isEmpty()) {
            // Remove the text for all script tags
            RefPtr<WebCore::NodeList> list = element->getElementsByTagName(HTMLNames::scriptTag.localName());
            unsigned length = list->length();

            for (unsigned i = 0; i < length; i++) {
                WebCore::HTMLElement* script = static_cast<WebCore::HTMLElement*>(list->item(i));
                textContent = textContent.replace(script->innerText(), "");
            }

            return textContent.stripWhiteSpace();
        }
    }

    return WTF::String();
}

WTF::String WebFrameImpl::retrieveFirstH1() {

    if (!viewImpl() || !viewImpl()->mainFrameImpl() || !viewImpl()->mainFrameImpl()->frame() || !viewImpl()->mainFrameImpl()->frame()->document()) {
        return WTF::String();
    }

    RefPtr<NodeList> list = viewImpl()->mainFrameImpl()->frame()->document()->getElementsByTagName(HTMLNames::h1Tag.localName());
    if (list && list->length() > 0) {
        WebCore::HTMLElement* h1Element = static_cast<WebCore::HTMLElement*>(list->item(0));
        return getInnerText(h1Element);
    }

    return WTF::String("");
}

WTF::String WebFrameImpl::retrieveFirstH2() {

    if (!viewImpl() || !viewImpl()->mainFrameImpl() || !viewImpl()->mainFrameImpl()->frame() || !viewImpl()->mainFrameImpl()->frame()->document()) {
        return WTF::String();
    }

    RefPtr<NodeList> list = viewImpl()->mainFrameImpl()->frame()->document()->getElementsByTagName(HTMLNames::h2Tag.localName());
    if (list->length() > 0) {
        WebCore::HTMLElement* h2 = static_cast<WebCore::HTMLElement*>(list->item(0));
        return getInnerText(h2);
    }

    return WTF::String("");
}

WTF::String WebFrameImpl::retrieveReferrer()
{
    if (!viewImpl() || !viewImpl()->mainFrameImpl() || !viewImpl()->mainFrameImpl()->frame() || !viewImpl()->mainFrameImpl()->frame()->document()) {
        return WTF::String();
    }
    
    return viewImpl()->mainFrameImpl()->frame()->document()->referrer();
}

WTF::String WebFrameImpl::retrievePreviousText() {

    if (m_currentlySelectedEntity) {
        return m_currentlySelectedEntity->getPreviousText();
    }

    return WTF::String();
}

WTF::String WebFrameImpl::retrievePostText() {
    
    if (m_currentlySelectedEntity) {
        return m_currentlySelectedEntity->getPostText();
    }

    return WTF::String();
}

WTF::String WebFrameImpl::getSelection() {

    if (m_currentlySelectedEntity) {
        return m_currentlySelectedEntity->getText();
    }

    return WTF::String();
}

void WebFrameImpl::getSelectionContext(const bool shouldUpdateSelectionContext, std::map<std::string, std::string>* selectionContext)
{
    // check if kikin is disabled at run-time & display message:
    if( m_frame && m_frame->page() && m_frame->page() && m_frame->page()->settings() &&
       !m_frame->page()->settings()->isKikinSearchEnabled() ) {
            // this function should not have been invoked when kikin was disabled at run-time
	        LOG(INFO)<<"WebFrameImpl::getSelectionContext invoked while kikin is disabled at run-time.\n";
	        return;
	}

    // Update the selection context if requested. (A selection context update is 
    // requested when user modifies the selection using selection handles.)
    if (shouldUpdateSelectionContext)
        refreshSelection();

    // Add page specific meta data
    std::string content(retrieveDocumentText().utf8().data());
    std::string selection(getSelection().utf8().data());
    std::string textBeforeSelection(retrievePreviousText().utf8().data());
    std::string textAfterSelection(retrievePostText().utf8().data());
    std::string keywords(retrieveKeywords().utf8().data());
    std::string title(retrieveTitle().utf8().data());
    std::string h1(retrieveFirstH1().utf8().data());
    std::string h2(retrieveFirstH2().utf8().data());
    std::string referrer(retrieveReferrer().utf8().data());

    selectionContext->insert(pair<std::string, std::string>("content", content));
    selectionContext->insert(pair<std::string, std::string>("selection", selection));
    selectionContext->insert(pair<std::string, std::string>("textBeforeSelection", textBeforeSelection));
    selectionContext->insert(pair<std::string, std::string>("textAfterSelection", textAfterSelection));
    selectionContext->insert(pair<std::string, std::string>("keywords", keywords));
    selectionContext->insert(pair<std::string, std::string>("title", title));
    selectionContext->insert(pair<std::string, std::string>("h1", h1));
    selectionContext->insert(pair<std::string, std::string>("h2", h2));
    selectionContext->insert(pair<std::string, std::string>("referrer", referrer));

    // Add selection specific meta data
    if (m_currentlySelectedEntity) {
        std::string selectionType(m_currentlySelectedEntity->getType_WTF().utf8().data());
        std::string selectionLanguage(m_currentlySelectedEntity->getLanguage_WTF().utf8().data());
        std::string touchedWord(m_currentlySelectedEntity->getTouchedWord_WTF().utf8().data());
        std::string tag(m_currentlySelectedEntity->getTag().utf8().data());
        std::string alt(m_currentlySelectedEntity->getAlt().utf8().data());
        std::string href(m_currentlySelectedEntity->getHref().utf8().data());
        std::string value(m_currentlySelectedEntity->getValue_WTF().utf8().data());

        selectionContext->insert(pair<std::string, std::string>("selectionType", selectionType));
        selectionContext->insert(pair<std::string, std::string>("selectionLanguage", selectionLanguage));
        selectionContext->insert(pair<std::string, std::string>("touchedWord", touchedWord));
        selectionContext->insert(pair<std::string, std::string>("tag", tag));
        selectionContext->insert(pair<std::string, std::string>("alt", alt));
        selectionContext->insert(pair<std::string, std::string>("href", href));
        selectionContext->insert(pair<std::string, std::string>("value", value));
    }
}

void WebFrameImpl::updateSelection(const std::string& oldQuery, const std::string& newQuery)
{
    if (m_currentlySelectedEntity) {
        m_currentlySelectedEntity->modifySelectionByText(
            WTF::String(oldQuery.c_str()),
            WTF::String(newQuery.c_str()),
            viewImpl()->mainFrameImpl()->frame()->document());
            
        VisibleSelection selection;
        if (WebFrameImpl::getSelectionFromKikinEntity(m_currentlySelectedEntity, selection)) {
            frame()->selection()->setSelection(selection, WordGranularity);
        }
    }
}

void WebFrameImpl::refreshSelection()
{
    // check if we have a valid frame
    if (!frame()) 
        return;
        
    // get the selection controller for exisiting frame
    FrameSelection* frameSelection = frame()->selection();
    if (frameSelection != NULL) {
    
        VisibleSelection selection = frameSelection->selection();
        
        // if selection is not valid, then do not change anything
        if (selection.isNone() || !selection.isRange())
            return;
        
        // create kikin entity from existing selection    
        RefPtr<kikin::HtmlRange> range = kikin::HtmlRange::create(selection.firstRange(), selection.start().containerNode(), "text");

        RefPtr<kikin::list::Ranges> ranges = kikin::list::Ranges::create();
        ranges->append(range.release());

        RefPtr<kikin::HtmlEntity> entity = kikin::HtmlEntity::create(ranges, "text", "");
        if (entity)
            entity->saveWordsAround();

        // update current selection
        m_currentlySelectedEntity = entity;
    }
}

void WebFrameImpl::restoreSelection()
{
    if (m_currentlySelectedEntity) {
        VisibleSelection selection;
        if (WebFrameImpl::getSelectionFromKikinEntity(m_currentlySelectedEntity, selection)) {
            frame()->selection()->setSelection(selection, WordGranularity);
        }
    }   
}

void WebFrameImpl::clearSelection()
{
    // clear the selected entity
    // otherwise we were leaking the memory.
    if (m_currentlySelectedEntity) {
        m_currentlySelectedEntity = 0;
    }
}

#endif
// -------------------------------------------------- kikin Modification ends --------------------------------------------------------------
#if defined(SBROWSER_KEYLAG)
void WebFrameImpl::suspendInputJS()
{
    WebCore::Frame* main_frame = m_frame;  
    if( main_frame != NULL ){
        main_frame->page()->suspendJavaScript();
    }
}

void WebFrameImpl::resumeInputJS()
{
    WebCore::Frame* main_frame = m_frame;  
    if( main_frame != NULL ){
        main_frame->page()->resumeJavaScript();
    }
}
#endif
} // namespace WebKit
