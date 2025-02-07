/*
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "modules/filesystem/LocalFileSystem.h"

#include "WebFileSystemCallbacksImpl.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebPermissionClient.h"
#include "WebViewImpl.h"
#include "WebWorkerBase.h"
#include "WorkerAllowMainThreadBridgeBase.h"
#include "WorkerFileSystemCallbacksBridge.h"
#include "core/dom/CrossThreadTask.h"
#include "core/dom/Document.h"
#include "core/workers/WorkerContext.h"
#include "core/workers/WorkerThread.h"
#include "modules/filesystem/ErrorCallback.h"
#include "modules/filesystem/FileSystemCallback.h"
#include "modules/filesystem/FileSystemCallbacks.h"
#include "modules/filesystem/FileSystemType.h"
#include <public/WebFileError.h>
#include <public/WebFileSystem.h>
#include <public/WebFileSystemType.h>
#include <wtf/text/WTFString.h>
#include <wtf/Threading.h>

using namespace WebKit;

namespace WebCore {

LocalFileSystem& LocalFileSystem::localFileSystem()
{
    AtomicallyInitializedStatic(LocalFileSystem*, localFileSystem = new LocalFileSystem(""));
    return *localFileSystem;
}

namespace {

enum CreationFlag {
    OpenExisting,
    CreateIfNotPresent
};

static const char allowFileSystemMode[] = "allowFileSystemMode";
static const char openFileSystemMode[] = "openFileSystemMode";

// This class is used to route the result of the WebWorkerBase::allowFileSystem
// call back to the worker context.
class AllowFileSystemMainThreadBridge : public WorkerAllowMainThreadBridgeBase {
public:
    static PassRefPtr<AllowFileSystemMainThreadBridge> create(WebCore::WorkerContext* workerContext, WebWorkerBase* webWorkerBase, const String& mode)
    {
        return adoptRef(new AllowFileSystemMainThreadBridge(workerContext, webWorkerBase, mode));
    }

private:
    AllowFileSystemMainThreadBridge(WebCore::WorkerContext* workerContext, WebWorkerBase* webWorkerBase, const String& mode)
        : WorkerAllowMainThreadBridgeBase(workerContext, webWorkerBase)
    {
        postTaskToMainThread(adoptPtr(new AllowParams(mode)));
    }

    virtual bool allowOnMainThread(WebCommonWorkerClient* commonClient, AllowParams*)
    {
        ASSERT(isMainThread());
        return commonClient->allowFileSystem();
    }
};

bool allowFileSystemForWorker()
{
    WorkerScriptController* controller = WorkerScriptController::controllerForContext();
    WorkerContext* workerContext = controller->workerContext();
    WebCore::WorkerThread* workerThread = workerContext->thread();
    WorkerRunLoop& runLoop = workerThread->runLoop();
    WebCore::WorkerLoaderProxy* workerLoaderProxy = &workerThread->workerLoaderProxy();

    // Create a unique mode just for this synchronous call.
    String mode = allowFileSystemMode;
    mode.append(String::number(runLoop.createUniqueId()));

    RefPtr<AllowFileSystemMainThreadBridge> bridge = AllowFileSystemMainThreadBridge::create(workerContext, workerLoaderProxy->toWebWorkerBase(), mode);

    // Either the bridge returns, or the queue gets terminated.
    if (runLoop.runInMode(workerContext, mode) == MessageQueueTerminated) {
        bridge->cancel();
        return false;
    }

    return bridge->result();
}

void openFileSystemForWorker(WebCommonWorkerClient* commonClient, WebFileSystemType type, long long size, bool create, WebFileSystemCallbacksImpl* callbacks, FileSystemSynchronousType synchronousType)
{
    WorkerScriptController* controller = WorkerScriptController::controllerForContext();
    WorkerContext* workerContext = controller->workerContext();
    WebCore::WorkerThread* workerThread = workerContext->thread();
    WorkerRunLoop& runLoop = workerThread->runLoop();
    WebCore::WorkerLoaderProxy* workerLoaderProxy =  &workerThread->workerLoaderProxy();

    // Create a unique mode for this openFileSystem call.
    String mode = openFileSystemMode;
    mode.append(String::number(runLoop.createUniqueId()));

    RefPtr<WorkerFileSystemCallbacksBridge> bridge = WorkerFileSystemCallbacksBridge::create(workerLoaderProxy, workerContext, callbacks);
    bridge->postOpenFileSystemToMainThread(commonClient, type, size, create, mode);

    if (synchronousType == SynchronousFileSystem) {
        if (runLoop.runInMode(workerContext, mode) == MessageQueueTerminated)
            bridge->stop();
    }
}

} // namespace

static void openFileSystemNotAllowed(ScriptExecutionContext*, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    callbacks->didFail(WebKit::WebFileErrorAbort);
}

static void deleteFileSystemNotAllowed(ScriptExecutionContext*, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    callbacks->didFail(WebKit::WebFileErrorAbort);
}

static void openFileSystemHelper(ScriptExecutionContext* context, FileSystemType type, PassOwnPtr<AsyncFileSystemCallbacks> callbacks, FileSystemSynchronousType synchronousType, long long size, CreationFlag create)
{
    bool allowed = true;
    ASSERT(context);
    if (context->isDocument()) {
        Document* document = static_cast<Document*>(context);
        WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
        WebKit::WebViewImpl* webView = webFrame->viewImpl();
        if (webView->permissionClient() && !webView->permissionClient()->allowFileSystem(webFrame))
            allowed = false;
        else
            webFrame->client()->openFileSystem(webFrame, static_cast<WebFileSystemType>(type), size, create == CreateIfNotPresent, new WebFileSystemCallbacksImpl(callbacks));
    } else {
        WorkerContext* workerContext = static_cast<WorkerContext*>(context);
        WebWorkerBase* webWorker = static_cast<WebWorkerBase*>(workerContext->thread()->workerLoaderProxy().toWebWorkerBase());
        if (!allowFileSystemForWorker())
            allowed = false;
        else
            openFileSystemForWorker(webWorker->commonClient(), static_cast<WebFileSystemType>(type), size, create == CreateIfNotPresent, new WebFileSystemCallbacksImpl(callbacks, context, synchronousType), synchronousType);
    }

    if (!allowed) {
        // The tasks are expected to be called asynchronously.
        context->postTask(createCallbackTask(&openFileSystemNotAllowed, callbacks));
    }
}

void LocalFileSystem::readFileSystem(ScriptExecutionContext* context, FileSystemType type, PassOwnPtr<AsyncFileSystemCallbacks> callbacks, FileSystemSynchronousType synchronousType)
{
    openFileSystemHelper(context, type, callbacks, synchronousType, 0, OpenExisting);
}

void LocalFileSystem::requestFileSystem(ScriptExecutionContext* context, FileSystemType type, long long size, PassOwnPtr<AsyncFileSystemCallbacks> callbacks, FileSystemSynchronousType synchronousType)
{
    openFileSystemHelper(context, type, callbacks, synchronousType, size, CreateIfNotPresent);
}

void LocalFileSystem::deleteFileSystem(ScriptExecutionContext* context, FileSystemType type, PassOwnPtr<AsyncFileSystemCallbacks> callbacks)
{
    ASSERT(context);
    ASSERT_WITH_SECURITY_IMPLICATION(context->isDocument());

    Document* document = static_cast<Document*>(context);
    WebFrameImpl* webFrame = WebFrameImpl::fromFrame(document->frame());
    WebKit::WebViewImpl* webView = webFrame->viewImpl();
    if (webView->permissionClient() && !webView->permissionClient()->allowFileSystem(webFrame)) {
        context->postTask(createCallbackTask(&deleteFileSystemNotAllowed, callbacks));
        return;
    }

    webFrame->client()->deleteFileSystem(webFrame, static_cast<WebFileSystemType>(type), new WebFileSystemCallbacksImpl(callbacks));
}

} // namespace WebCore
