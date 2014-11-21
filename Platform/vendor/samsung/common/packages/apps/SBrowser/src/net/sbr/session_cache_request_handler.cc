
#if defined(SBROWSER_SESSION_CACHE)
#include "net/sbr/session_cache_request_handler.h"

#include "content/browser/loader/resource_request_info_impl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/sbr/session_cache_url_request_job.h"
#include "net/sbr/session_cache_storage.h"
#include "net/url_request/url_request_context.h"
#include "net/sbr/session_cache_storage.h"

namespace net {

SessionCacheRequestHandler::SessionCacheRequestHandler()
    :cache_entry_not_found_(false) {
}

SessionCacheRequestHandler::~SessionCacheRequestHandler() {
}

bool SessionCacheRequestHandler::ShouldServiceRequest(
    net::URLRequest* request) {
		// we also need information if this is a fresh load
		// or bf nav, but that we will get
		if(request->url().SchemeIs("http"))
			return true;
		return false;
}

SessionCacheURLRequestJob* SessionCacheRequestHandler::MaybeLoadResource(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
	
	if (!ShouldServiceRequest(request) || cache_entry_not_found_)
		return NULL;

    content::ResourceRequestInfoImpl* req_info =
            content::ResourceRequestInfoImpl::ForRequest(request);
    const net::SessionCacheID cache_id(req_info->GetRouteID(),
                                       req_info->GetChildID());
    if (SessionCacheStorage::GetInstance()->IsResponseCached(request->url(), cache_id)) {
        job_ = new SessionCacheURLRequestJob(request, network_delegate);
	}
	else
		cache_entry_not_found_ = true;
	return job_;

}

SessionCacheURLRequestJob* SessionCacheRequestHandler::MaybeLoadFallbackForRedirect(
    net::URLRequest* request, net::NetworkDelegate* network_delegate, const GURL& location) {
	//FIXME:NOT_IMPLEMENTED
	return NULL;
}

SessionCacheURLRequestJob* SessionCacheRequestHandler::MaybeLoadResponse(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) {
	
    content::ResourceRequestInfoImpl* req_info =
            content::ResourceRequestInfoImpl::ForRequest(request);
    const net::SessionCacheID cache_id(req_info->GetRouteID(),
                                       req_info->GetChildID());
    if(SessionCacheStorage::GetInstance()->IsResponseCached(request->url(), cache_id)) {
        job_ = new SessionCacheURLRequestJob(request, network_delegate);
	}
	return job_;
}

}  // namespace net
#endif 
