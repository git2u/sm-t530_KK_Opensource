
#if defined(SBROWSER_SESSION_CACHE)
#include "net/sbr/session_cache_interceptor.h"
#include "net/sbr/session_cache_request_handler.h"
#include "net/sbr/session_cache_url_request_job.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/sbr/session_cache_storage.h"
#include "net/url_request/url_request_context.h"

namespace net {

// static
SessionCacheInterceptor* SessionCacheInterceptor::GetInstance() {
  return Singleton<SessionCacheInterceptor>::get();
}

void SessionCacheInterceptor::SetHandler(
    net::URLRequest* request, SessionCacheRequestHandler* handler) {
  request->SetUserData(GetInstance(), handler);  // request takes ownership
}

SessionCacheRequestHandler* SessionCacheInterceptor::GetHandler(
    net::URLRequest* request) {
  return reinterpret_cast<SessionCacheRequestHandler*>(
      request->GetUserData(GetInstance()));
}

void SessionCacheInterceptor::SetExtraRequestInfo(net::URLRequest* request) {
  // Make the SessionCacheRequestHandler object and then set it as the request handler
  SessionCacheRequestHandler* handler = createRequestHandler(request);
  if (handler)
    SetHandler(request, handler);
}

SessionCacheRequestHandler* SessionCacheInterceptor::createRequestHandler(
	net::URLRequest* request)
{
	//based on criteria make a request handler
	//but we don't have any criteria as of now, so when ever we 
	//get a request to create a requesthandler we make it
	SessionCacheRequestHandler* handler = new SessionCacheRequestHandler;
	return handler;
}

SessionCacheInterceptor::SessionCacheInterceptor() {
  net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
}

SessionCacheInterceptor::~SessionCacheInterceptor() {
  net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
}

net::URLRequestJob* SessionCacheInterceptor::MaybeIntercept(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
	if(!request->is_back_forward_navigation() || 
        !SessionCacheStorage::GetInstance()->IsSessionCacheEnabled())
		return NULL;

	SessionCacheRequestHandler* handler = GetHandler(request);
	if (!handler)
		return NULL;
	return handler->MaybeLoadResource(request, network_delegate);
}

net::URLRequestJob* SessionCacheInterceptor::MaybeInterceptRedirect(
    net::URLRequest* request,
	net::NetworkDelegate* network_delegate,
    const GURL& location) {
	if(!request->is_back_forward_navigation() || 
        !SessionCacheStorage::GetInstance()->IsSessionCacheEnabled())
		return NULL;

	SessionCacheRequestHandler* handler = GetHandler(request);
	if (!handler)
		return NULL;
	return handler->MaybeLoadFallbackForRedirect(request,network_delegate, location);
}

net::URLRequestJob* SessionCacheInterceptor::MaybeInterceptResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
/*
    if(!SessionCacheStorage::GetInstance()->IsSessionCacheEnabled())
		return NULL;
	if(request->response_headers()->HasHeaderValue("cache-control", "no-store")){
		// we will now store the latest response in the session
		// so that next time it can be served for the request
		//create the response info
		net::HttpResponseInfo* rinfo = new net::HttpResponseInfo(request->response_info());
        if(!SessionCacheStorage::GetInstance()->IsReponseCachedForURL(request->url())){
			SessionCacheResponseInfo* info = new SessionCacheResponseInfo(
				request->url()
				,rinfo);
            SessionCacheStorage::GetInstance()->AddToCache(request->url(), info);
		}
	}
*/
	return NULL;
}

}  // namespace net
#endif 
