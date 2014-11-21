
#ifndef NET_HTTP_SESSION_CACHE_SESSION_CACHE_STORAGE_H_
#define NET_HTTP_SESSION_CACHE_SESSION_CACHE_STORAGE_H_

#if defined(SBROWSER_SESSION_CACHE)

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/http/http_response_info.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "base/memory/singleton.h"
#include "base/timer.h"
#include "base/time.h"

namespace net {

// Currently we will store entries till the cache hits 3 MB.
// After that based on MRU, entries will be kicked out to make around
// 25% space again in the cache.
const int KMaxSessionCacheStoreLimit = 3 * 1024 * 1024;
// testing  - 100kb
//const int KMaxSessionCacheStoreLimit = 100 * 1024;

// An entry can stay in the cache for max 30 mins, after which it will
// start its journey to the nether world.
const double KMaxSessionCacheEntryAge = 30 * 60; // this is seconds, as ToDouble returns it in seconds
//for testing - 2 minutes
//const double KMaxSessionCacheEntryAge = 2 * 60; // this is seconds, as ToDouble returns it in seconds

// We will not wait for the cache to fill in, we have a watermark set at 98%.
// Touch 98% and the compaction routine will kick in and remove entries to
// reclaim 25% space back.
// The compaction will trigger from the MRU tail, as those are the Least used
// entries and as they are farthest from the most recently used item, hence
// they make the ideal candidates for purges. We should also have a param to mark
// an entries distance from the current top item, thus leaving out the entries
// adjacent to the current live entry.
const double KCacheCompactionTriggerPercent = 98;

// Uniquely identifies a tab with a pair of routing id and child id
typedef std::pair<int, int> SessionCacheID;

class SessionCacheResponseInfo;

//FIXME : SESSIONCACHERESPONSEINFO NEEDS A SEPARATE CLASS FILE.!!!

class NET_EXPORT SessionCacheResponseInfo
    : public base::RefCounted<SessionCacheResponseInfo> {
 public:

  // SessionCacheResponseInfo takes ownership of the http_info.
  SessionCacheResponseInfo(const GURL& resource_url,
                           net::HttpResponseInfo* http_info,
                           SessionCacheID id,
                           int64 nav_history_entry_index = -1)
    : resource_url_(resource_url)
    , http_response_info_(http_info)
    , id_(id)
    , nav_history_entry_index_(nav_history_entry_index)
    , response_body_(new GrowableIOBuffer())
    , response_body_len_(0)
    , mark_for_clearing_(false)
    , load_time_stamp_(base::Time::NowFromSystemTime())
  {
  }


  const GURL& resource_url() const { return resource_url_; }
  const net::HttpResponseInfo* http_response_info() const {
    return http_response_info_.get();
  }
  int response_body_length() const { return response_body_len_; }
  IOBuffer* response_body() { return response_body_.get(); }

  void collateResponseBody(IOBuffer* data, int data_len) {
    GrowableIOBuffer* buf = reinterpret_cast<GrowableIOBuffer*>(response_body_.get());
    buf->SetCapacity(response_body_len_ + data_len);
    buf->set_offset(response_body_len_);
    memcpy(buf->data(), data->data(), data_len);
    response_body_len_ += data_len;
  }

  void markResponseBodyForClearing() {
    mark_for_clearing_ = true;
  }

  bool isResponseBodyMarkedForClearing() const {
    return mark_for_clearing_;
  }

  //use with care!!
  void clearCachedResponse() {
    // Setting the capacity to zero, will delete the old data
    response_body_->SetCapacity(0);
    response_body_len_ = 0;
    mark_for_clearing_ = false;
    // Update the time stamp as well, as we have new response which 
    // should persist for the full expiry duration
    load_time_stamp_ = base::Time::NowFromSystemTime();
  }

  bool expired() const {
    return (base::Time::NowFromSystemTime().ToDoubleT() -
            load_time_stamp_.ToDoubleT()) > KMaxSessionCacheEntryAge;
  }

  SessionCacheID id() const { return id_; }

 private:
  friend class base::RefCounted<SessionCacheResponseInfo>;
  ~SessionCacheResponseInfo() { }

  const GURL resource_url_;
  const scoped_ptr<net::HttpResponseInfo> http_response_info_;
  SessionCacheID id_;
  const int64 nav_history_entry_index_;
  scoped_refptr<GrowableIOBuffer> response_body_;
  int response_body_len_;
  bool mark_for_clearing_;
  base::Time load_time_stamp_;
};

// FIXME:25-Jun-2013
// This should be an interface which can then be implemented by either
// an in-memory implementation or disk based implementation.
// But for release 0 we are making a simple class which stores
// a hash map of the http responses for the url requested.

class NET_EXPORT SessionCacheStorage {
 public:
  static SessionCacheStorage* GetInstance();
  
  static void EnableSessionCacheFromOtherThread(bool enable){
    SessionCacheStorage::GetInstance()->EnableSessionCache(enable);
  }

 public:

  bool IsResponseCached(const GURL& response_url, SessionCacheID id);

  scoped_refptr<SessionCacheResponseInfo> LoadResponse(
          const GURL& response_url, SessionCacheID id);

  void AddToCache(const GURL& response_url,
                  scoped_refptr<SessionCacheResponseInfo> entry_info,
                  SessionCacheID id);

  void DoomCacheEntries(SessionCacheID id);

  void PurgeAllEntries();

  void CompactCacheToSizeLimit();

  bool IsSessionCacheEnabled() const { return session_cache_enabled_; }

  void EnableSessionCache(bool enable);

  void UpdateCacheStorageData(long cache_entry_response_body_length);

 private:
  SessionCacheStorage();
  ~SessionCacheStorage();

  void MemCompactionTimerKicksIn();

  typedef std::map<GURL, scoped_refptr<SessionCacheResponseInfo> > SessionCacheEntries;
  typedef std::pair<GURL, scoped_refptr<SessionCacheResponseInfo> > SessionCacheEntry;
  typedef std::deque<scoped_refptr<SessionCacheResponseInfo> > MRUListAsDq;

  SessionCacheEntries* GetCacheEntriesForID(SessionCacheID);
  void InsertCacheEntry(GURL response_url,
                        scoped_refptr<SessionCacheResponseInfo> entry_info,
                        SessionCacheID id,
                        bool new_entry);
  void RemoveCacheEntry(SessionCacheEntries* cache_entries, GURL url);
  void RemoveCacheEntry(GURL response_url, SessionCacheID id);
  // Removes the least recently used cache entry from MRU list
  // as well as SessionCache. This is invoked if SessionCache reaches a certain
  // memory threshold defined by CacheStorageLimitReached()
  void RemoveCacheEntry(MRUListAsDq::iterator itr);

  void InsertMRUListItem(scoped_refptr<SessionCacheResponseInfo> sri) {
    mru_cache_entries_list_->push_front(sri);
  }

  void UpdateMRUList(scoped_refptr<SessionCacheResponseInfo> sri);

  void RemoveMRUEntry(scoped_refptr<SessionCacheResponseInfo> sri);
  bool CacheStorageLimitReached();

  int RemoveExpiredEntries();
  void RemoveLRUEntries(int number_of_bytes_to_be_reclaimed);

  friend struct DefaultSingletonTraits<SessionCacheStorage>;

  DISALLOW_COPY_AND_ASSIGN(SessionCacheStorage);

  typedef std::map<SessionCacheID, SessionCacheEntries*> SessionCache;
  typedef std::pair<SessionCacheID, SessionCacheEntries*> SessionCacheItem;
  scoped_ptr<SessionCache> in_mem_session_cache_;

  bool session_cache_enabled_;

  scoped_ptr<MRUListAsDq> mru_cache_entries_list_;

  base::OneShotTimer<SessionCacheStorage> cache_compact_timer_;
  long session_cache_storage_size_;
};

}  // namespace net
#endif
#endif  // NET_HTTP_SESSION_CACHE_SESSION_CACHE_STORAGE_H_
