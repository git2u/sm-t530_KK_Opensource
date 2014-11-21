
#ifndef NET_SESSION_CACHE_SESSION_CACHE_INTERCEPTOR_H_
#define NET_SESSION_CACHE_SESSION_CACHE_INTERCEPTOR_H_

#if defined(SBROWSER_SESSION_CACHE)
#include "base/memory/singleton.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

namespace net {

class SessionCacheRequestHandler;
//class AppCacheService;

// An interceptor to hijack requests and potentially service them out of
// the session cache.
class NET_EXPORT SessionCacheInterceptor
    : public net::URLRequest::Interceptor {
 public:
  static void EnsureRegistered() {
    CHECK(GetInstance());
  }

  // Must be called to make a request eligible for retrieval from an appcache.
  static void SetExtraRequestInfo(net::URLRequest* request);

  static SessionCacheInterceptor* GetInstance();

 protected:
  // Override from net::URLRequest::Interceptor:
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request, net::NetworkDelegate* network_delegate) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request, net::NetworkDelegate* network_delegate) OVERRIDE;
  virtual net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request
	, net::NetworkDelegate* network_delegate
	, const GURL& location) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SessionCacheInterceptor>;

  SessionCacheInterceptor();

  virtual ~SessionCacheInterceptor();

  static void SetHandler(net::URLRequest* request,
                         SessionCacheRequestHandler* handler);
  static SessionCacheRequestHandler* GetHandler(net::URLRequest* request);

  static SessionCacheRequestHandler* createRequestHandler(net::URLRequest* request);

  DISALLOW_COPY_AND_ASSIGN(SessionCacheInterceptor);
};

}  // namespace net
#endif 
#endif  // NET_SESSION_CACHE_SESSION_CACHE_INTERCEPTOR_H_
