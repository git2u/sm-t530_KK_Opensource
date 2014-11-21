
#ifndef NET_HTTP_SESSION_CACHE_SESSIONCACHE_URL_REQUEST_JOB_H_
#define NET_HTTP_SESSION_CACHE_SESSIONCACHE_URL_REQUEST_JOB_H_


#if defined(SBROWSER_SESSION_CACHE)
#include <string>

#include "base/memory/weak_ptr.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request_http_job.h"

namespace net {

class SessionCacheResponseInfo;
class HttpFilterContext;

// A net::URLRequestJob derivative that knows how to return a response stored
// in the session cache.
class NET_EXPORT SessionCacheURLRequestJob : 
	//public net::URLRequestHttpJob
	public net::URLRequestJob
                                              {
 public:
  SessionCacheURLRequestJob(net::URLRequest* request, net::NetworkDelegate* network_delegate);
  ~SessionCacheURLRequestJob();


  // net::URLRequestJob's Kill method is made public so the users of this
  // class in the appcache namespace can call it.
  virtual void Kill() OVERRIDE;

  // Returns true if the job has been started by the net library.
  bool has_been_started() const {
    return has_been_started_;
  }

  // Returns true if the job has been killed.
  bool has_been_killed() const {
    return has_been_killed_;
  }

  // Returns true if the cache entry was not found in the disk cache.
  bool cache_entry_not_found() const {
    return cache_entry_not_found_;
  }
protected:
  class SessionCacheFilterContext;
 private:

	

  void MaybeBeginDelivery();
  void BeginDelivery();

  const net::HttpResponseInfo* http_info() const;

  // net::URLRequestJob methods, see url_request_job.h for doc comments
  virtual void Start() OVERRIDE;
  virtual net::LoadState GetLoadState() const OVERRIDE;
  virtual bool GetCharset(std::string* charset) OVERRIDE;
  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE;
  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE;
	virtual Filter* SetupFilter() const OVERRIDE;

  // Sets extra request headers for Job types that support request headers.
  // This is how we get informed of range-requests.
  virtual void SetExtraRequestHeaders(
	  const net::HttpRequestHeaders& headers) OVERRIDE{
  }

  // FilterContext methods
  virtual bool GetMimeType(std::string* mime_type) const OVERRIDE;
  virtual int GetResponseCode() const OVERRIDE;

  bool has_been_started_;
  bool has_been_killed_;
  bool cache_entry_not_found_;
  scoped_refptr<SessionCacheResponseInfo> info_;
	bool response_read_in_progress_;
	int64 remaining_bytes_;
  base::WeakPtrFactory<SessionCacheURLRequestJob> weak_factory_;
  scoped_ptr<SessionCacheFilterContext> filter_context_;
};

}  // namespace net

#endif 

#endif  // NET_HTTP_SESSION_CACHE_SESSIONCACHE_URL_REQUEST_JOB_H_
