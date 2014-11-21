// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(SBROWSER_SESSION_CACHE)
#include <vector>

#include "net/sbr/session_cache_url_request_job.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/sbr/session_cache_storage.h"
#include "net/url_request/url_request_job.h"

namespace net {

//inspired from HttpFilterContext :)
class SessionCacheURLRequestJob::SessionCacheFilterContext : public FilterContext {
 public:
  explicit SessionCacheFilterContext(SessionCacheURLRequestJob* job);
  virtual ~SessionCacheFilterContext();

  // FilterContext implementation.
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetURL(GURL* gurl) const;
  virtual base::Time GetRequestTime() const;
  virtual bool IsCachedContent() const{ return true; }
  virtual bool IsDownload() const { return false; }
  virtual bool IsSdchResponse() const { return false; }
  virtual int64 GetByteReadCount() const;
  virtual int GetResponseCode() const;
  virtual void RecordPacketStats(StatisticSelector statistic) const{}

  void ResetSdchResponseToFalse(){ }

 private:
  SessionCacheURLRequestJob* job_;

  DISALLOW_COPY_AND_ASSIGN(SessionCacheFilterContext);
};

SessionCacheURLRequestJob::SessionCacheFilterContext::SessionCacheFilterContext(SessionCacheURLRequestJob* job)
    : job_(job) {
  DCHECK(job_);
}

SessionCacheURLRequestJob::SessionCacheFilterContext::~SessionCacheFilterContext() {
}

bool SessionCacheURLRequestJob::SessionCacheFilterContext::GetMimeType(
    std::string* mime_type) const {
  return job_->GetMimeType(mime_type);
}

bool SessionCacheURLRequestJob::SessionCacheFilterContext::GetURL(GURL* gurl) const {
  if (!job_->request())
    return false;
  *gurl = job_->request()->url();
  return true;
}

base::Time SessionCacheURLRequestJob::SessionCacheFilterContext::GetRequestTime() const {
  return job_->request() ? job_->request()->request_time() : base::Time();
}

int64 SessionCacheURLRequestJob::SessionCacheFilterContext::GetByteReadCount() const {
  return job_->filter_input_byte_count();
}

int SessionCacheURLRequestJob::SessionCacheFilterContext::GetResponseCode() const {
  return job_->GetResponseCode();
}

//----------SessionCacheURLRequestJob starts here ------------------

SessionCacheURLRequestJob::SessionCacheURLRequestJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate)
    //: net::URLRequestHttpJob(request)
	: net::URLRequestJob(request, network_delegate)
      ,has_been_started_(false)
	  , has_been_killed_(false)
      ,cache_entry_not_found_(false)
	  ,response_read_in_progress_(false)
	  ,remaining_bytes_(0)
	  ,weak_factory_(this)
	  ,filter_context_(new SessionCacheFilterContext(this))
{
}

SessionCacheURLRequestJob::~SessionCacheURLRequestJob() {
	//DestroyFilters();
}

void SessionCacheURLRequestJob::MaybeBeginDelivery() {
  if (has_been_started()) {
    // Start asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SessionCacheURLRequestJob::BeginDelivery,
                   weak_factory_.GetWeakPtr()));
  }
}

void SessionCacheURLRequestJob::BeginDelivery() {
  if (has_been_killed())
    return;

  content::ResourceRequestInfoImpl* req_info =
      content::ResourceRequestInfoImpl::ForRequest(request_);
  const net::SessionCacheID cache_id(req_info->GetRouteID(),
                                     req_info->GetChildID());
  SessionCacheResponseInfo* response = 
                            SessionCacheStorage::GetInstance()
                                ->LoadResponse(request_->url(), cache_id);
  info_ = response;
  NotifyHeadersComplete();
}

const net::HttpResponseInfo* SessionCacheURLRequestJob::http_info() const {
  if (!info_.get())
    return NULL;
  return info_->http_response_info();
}


// net::URLRequestJob overrides ------------------------------------------------
//impl taken from URLRequestHttpJob
void SessionCacheURLRequestJob::Start() {
  DCHECK(!has_been_started());
  has_been_started_ = true;
  MaybeBeginDelivery();
}

void SessionCacheURLRequestJob::Kill() {
  if (!has_been_killed_) {
    has_been_killed_ = true;
    net::URLRequestJob::Kill();
    weak_factory_.InvalidateWeakPtrs();
  }
}

net::LoadState SessionCacheURLRequestJob::GetLoadState() const {
	//FIXME:NOT_IMPLEMENTED
    return net::LOAD_STATE_IDLE;
}

bool SessionCacheURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

bool SessionCacheURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

void SessionCacheURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int SessionCacheURLRequestJob::GetResponseCode() const {
  if (!http_info())
    return -1;
  return http_info()->headers->response_code();
}

bool SessionCacheURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size,
                                        int *bytes_read) {
	if(!response_read_in_progress_){
		//read has started
		response_read_in_progress_ = true;
		remaining_bytes_ = info_->response_body_length();
		GrowableIOBuffer* respBuf = reinterpret_cast<GrowableIOBuffer*>(info_->response_body());
		respBuf->set_offset(0);
	}

	int64 read_bytes = 0;
	read_bytes = (remaining_bytes_ < buf_size) ? remaining_bytes_ : buf_size;
	if(read_bytes){
		GrowableIOBuffer* respBuf = reinterpret_cast<GrowableIOBuffer*>(info_->response_body());
		respBuf->set_offset(info_->response_body_length() - remaining_bytes_);
		memcpy(buf->data(), info_->response_body()->data(), read_bytes);
	}
	else
		response_read_in_progress_ = false;

	remaining_bytes_ -= read_bytes;

	//SetStatus(net::URLRequestStatus(net::URLRequestStatus::SUCCESS, 0));
	//update the value of the number of bytes read
	*bytes_read = read_bytes;

	//not required!!
	//if (read_bytes > 0) {
	//	SetStatus(URLRequestStatus());
	//}

	//NotifyReadComplete(read_bytes);

	return true;
}

Filter* SessionCacheURLRequestJob::SetupFilter() const {
  if (!info_)
    return NULL;

  std::vector<Filter::FilterType> encoding_types;
  std::string encoding_type;
  HttpResponseHeaders* headers = info_->http_response_info()->headers;
  void* iter = NULL;
  while (headers->EnumerateHeader(&iter, "Content-Encoding", &encoding_type)) {
    encoding_types.push_back(Filter::ConvertEncodingToType(encoding_type));
  }

  //if (filter_context_->IsSdchResponse()) {
  //  // We are wary of proxies that discard or damage SDCH encoding.  If a server
  //  // explicitly states that this is not SDCH content, then we can correct our
  //  // assumption that this is an SDCH response, and avoid the need to recover
  //  // as though the content is corrupted (when we discover it is not SDCH
  //  // encoded).
  //  std::string sdch_response_status;
  //  iter = NULL;
  //  while (headers->EnumerateHeader(&iter, "X-Sdch-Encode",
  //                                  &sdch_response_status)) {
  //    if (sdch_response_status == "0") {
  //      filter_context_->ResetSdchResponseToFalse();
  //      break;
  //    }
  //  }
  //}

  // Even if encoding types are empty, there is a chance that we need to add
  // some decoding, as some proxies strip encoding completely. In such cases,
  // we may need to add (for example) SDCH filtering (when the context suggests
  // it is appropriate).
  Filter::FixupEncodingTypes(*filter_context_, &encoding_types);

  return !encoding_types.empty()
      ? Filter::Factory(encoding_types, *filter_context_) : NULL;
}

}  // namespace net
#endif 
