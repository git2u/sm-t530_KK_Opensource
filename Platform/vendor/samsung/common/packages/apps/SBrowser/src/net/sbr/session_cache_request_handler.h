
#ifndef NET_HTTP_SESSION_CACHE_REQUEST_HANDLER_H_
#define NET_HTTP_SESSION_CACHE_REQUEST_HANDLER_H_

#if defined(SBROWSER_SESSION_CACHE)
#include "base/compiler_specific.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"
#include "base/supports_user_data.h"

//namespace net {
//class URLRequest;
//class URLRequestJob;
//}  // namespace net

namespace net {

class SessionCacheURLRequestJob;
class SessionCacheStorage;

class NET_EXPORT SessionCacheRequestHandler
    : public base::SupportsUserData::Data //change for chrome 28!!

{
 public:
  virtual ~SessionCacheRequestHandler();

  // These are called on each request intercept opportunity.
  SessionCacheURLRequestJob* MaybeLoadResource(net::URLRequest* request
										, net::NetworkDelegate* network_delegate);
  SessionCacheURLRequestJob* MaybeLoadFallbackForRedirect(net::URLRequest* request
														, net::NetworkDelegate* network_delegate,
                                                      const GURL& location);
  SessionCacheURLRequestJob* MaybeLoadResponse(net::URLRequest* request, net::NetworkDelegate* network_delegate);

  void GetExtraResponseInfo(int64* cache_id, GURL* manifest_url);

 private:
	 // do we need this???
  friend class SessionCacheInterceptor;

  bool ShouldServiceRequest(net::URLRequest* request);

  SessionCacheRequestHandler();


  // Data members -----------------------------------------------
  // True if a cache entry this handler attempted to return was
  // not found in the disk cache. Once set, the handler will take
  // no action on all subsequent intercept opportunities, so the
  // request and any redirects will be handled by the network library.
  bool cache_entry_not_found_;

  // The job we use to deliver a response.
  scoped_refptr<SessionCacheURLRequestJob> job_;

  DISALLOW_COPY_AND_ASSIGN(SessionCacheRequestHandler);
};

}  // namespace net
#endif 
#endif  // NET_HTTP_SESSION_CACHE_REQUEST_HANDLER_H_
