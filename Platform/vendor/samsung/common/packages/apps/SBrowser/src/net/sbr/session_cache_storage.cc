
#if defined(SBROWSER_SESSION_CACHE)

#include "net/sbr/session_cache_storage.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/command_line.h"
#include "content/public/browser/browser_thread.h"
#include "chrome/common/chrome_switches.h"



namespace net {

// static
SessionCacheStorage* SessionCacheStorage::GetInstance()
{
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return Singleton<SessionCacheStorage>::get();
}

SessionCacheStorage::SessionCacheStorage()
  : session_cache_enabled_(false)
  , session_cache_storage_size_(0)
{
  // disabling setting session cache storage through command line
  // it will be off by default and then from debug menu it will be set on/off
#if 0
  //we should pick up the enable/disable settings from the command line!!!
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  if (cmd_line.HasSwitch(switches::kEnableSBrowserSessionCache))
    EnableSessionCache(true);
#endif
  LOG(INFO) << "SessionCacheStorage created with setting : " << session_cache_enabled_;

  in_mem_session_cache_.reset(new SessionCache);
  mru_cache_entries_list_.reset(new MRUListAsDq);
}

SessionCacheStorage::~SessionCacheStorage()
{
  PurgeAllEntries();
}

bool SessionCacheStorage::IsResponseCached(const GURL& response_url, SessionCacheID id)
{
  SessionCacheEntries* cache_entries = GetCacheEntriesForID(id);
  if (cache_entries == NULL)
    return false;

  SessionCacheEntries::iterator itr_entry =
      cache_entries->find(response_url);

  if (itr_entry != cache_entries->end()){
    if (itr_entry->second->expired()){
      RemoveCacheEntry(response_url, id);
      LOG(INFO) << "---Session Cache::IsResponseCached : Entry expired hence removed---";
      return false;
    }
    LOG(INFO) << "---Session Cache::IsResponseCached : Entry found ---";
    return true;
  }
  LOG(INFO) << "---Session Cache::IsResponseCached : Entry Not found ---";
  return false;
}

scoped_refptr<SessionCacheResponseInfo> SessionCacheStorage::LoadResponse(
    const GURL& response_url, SessionCacheID id)
{
  SessionCacheEntries* cache_entries = GetCacheEntriesForID(id);
  if (cache_entries == NULL)
    return NULL;

  SessionCacheEntries::iterator itr_entry =
      cache_entries->find(response_url);

  // return the first live entry
  if (itr_entry != cache_entries->end()){
    if(!itr_entry->second->expired()){
      // This was recently accessed so send it to the
      // top of the MRU list
      UpdateMRUList(itr_entry->second);
      LOG(INFO) << "---Session Cache : Loading response from session cache---";
      return itr_entry->second;
    }
    else{
      LOG(INFO) << "---Session Cache : Response found but expired, hence removing it---";
      RemoveCacheEntry(response_url, id);
    }
  }
  LOG(INFO) << "---Session Cache : Response not found in session cache---";
  return NULL;
}

void SessionCacheStorage::AddToCache(const GURL& response_url,
                                     scoped_refptr<SessionCacheResponseInfo> entry_info,
                                     SessionCacheID id)
{
  if(!session_cache_enabled_)
    return;

  scoped_refptr<SessionCacheResponseInfo> entry = LoadResponse(response_url, id);
  if (entry)
    RemoveCacheEntry(response_url, id);
  LOG(INFO) << "Session Cache Statistics : Adding entry for url : " << response_url;
  // If entry was not found in SessionCache that means its a new entry
  InsertCacheEntry(response_url, entry_info, id, !entry);
  LOG(INFO) << "Total Session Cache Size : " << session_cache_storage_size_ <<" bytes";
}

void SessionCacheStorage::DoomCacheEntries(SessionCacheID id)
{
  if(!session_cache_enabled_)
    return;
  LOG(INFO) << "Session Cache - DOOM for tab : routeid : " << id.first << " childid: " << id.second;
  SessionCacheEntries* cache_entries = GetCacheEntriesForID(id);
  if (cache_entries == NULL)
    return;

  SessionCacheEntries::iterator itr = cache_entries->begin();
  while (itr != cache_entries->end()) {
    scoped_refptr<SessionCacheResponseInfo> sri = itr->second;
    RemoveMRUEntry(sri);
    ++itr;
  }
  in_mem_session_cache_->erase(id);

  LOG(INFO) << "---Session Cache Statistics : Doomed entries for tab---";
  LOG(INFO) << "Total Session Cache Size : " << session_cache_storage_size_ <<" bytes";
  LOG(INFO) << "--------------------------------------------------------";

  delete cache_entries;
}

void SessionCacheStorage::PurgeAllEntries()
{
  //clear all the mru entries, the actual mem deletion will happen next
  if (mru_cache_entries_list_)
    mru_cache_entries_list_->clear();

  SessionCache::iterator itrCache = in_mem_session_cache_->begin();
  while (itrCache != in_mem_session_cache_->end()) {
    // Delete SessionCacheEntries as it is the only raw pointer stored
    // SessionCacheResponseInfo are stored in scoped_refptr
    delete itrCache->second;
    ++itrCache;
  }
  in_mem_session_cache_->clear();
  session_cache_storage_size_ = 0;
  LOG(INFO) << "---Session cache purging completed ---";
}

void SessionCacheStorage::EnableSessionCache(bool enable)
{
  if(session_cache_enabled_ == enable)
    return; //nothing to do
  session_cache_enabled_ = enable;
  LOG(INFO) << "SessionCacheStorage::EnableSessionCache: " << session_cache_enabled_;
  if (!session_cache_enabled_)
    PurgeAllEntries();
}

void SessionCacheStorage::UpdateCacheStorageData(
    long cache_entry_response_body_length) {
  LOG(INFO) << "---Session Cache Statistics---";
  LOG(INFO) << "Session Cache : Response body length to be added : " << cache_entry_response_body_length <<" bytes";
  LOG(INFO) << "Session Cache : Total Session Cache Size : " << session_cache_storage_size_ <<" bytes";
  LOG(INFO) << "------------------------------";

  // Add the stats as we are still in possession of some mem, but
  // the cache_compact_timer_ will soon make more space.
  session_cache_storage_size_ += cache_entry_response_body_length;

  if (CacheStorageLimitReached() && !cache_compact_timer_.IsRunning()) {
    // Fire the cache_compact_timer_ now.
    // Queueing it one timer so that we don't end up stopping some
    // imp action.
    LOG(INFO) << "---Session Cache::UpdateCacheStorageData :: Memory compaction timer being fired---";
    cache_compact_timer_.Start(FROM_HERE,
                               base::TimeDelta::FromMinutes(0),
                               this,
                               &SessionCacheStorage::MemCompactionTimerKicksIn);
  }
}

void SessionCacheStorage::MemCompactionTimerKicksIn()
{
    LOG(INFO) << "---Session Cache Statistics : Memory compaction fired---";
    LOG(INFO) << "Total Session Cache Size : " << session_cache_storage_size_ <<" bytes";
    LOG(INFO) << "--------------------------------------------------------";

  // Calculate the number of bytes to be freed. Target is to free 25% of the
  // items from the list.
  int number_of_bytes_to_free = (KMaxSessionCacheStoreLimit * 25) / 100;

  // here first remove all the dead entries, those which are
  // marked for deletion, then check how many more entries need to be removed
  // and then proceed to prune the list from the end till the targets are met
  number_of_bytes_to_free -= RemoveExpiredEntries();

  // Now clear the MRU list entries to make space for the rest of the entries
  RemoveLRUEntries(number_of_bytes_to_free);

  LOG(INFO) << "---Session Cache Statistics : Memory compaction done---";
  LOG(INFO) << "Total Session Cache Size : " << session_cache_storage_size_ <<" bytes";
  LOG(INFO) << "--------------------------------------------------------";

}

void SessionCacheStorage::UpdateMRUList(
    scoped_refptr<SessionCacheResponseInfo> sri)
{
  MRUListAsDq::iterator itrt = mru_cache_entries_list_->begin();
  for(; itrt != mru_cache_entries_list_->end(); ++itrt) {
    if((*itrt).get() == sri.get()) {
      mru_cache_entries_list_->erase(itrt);
      break;
    }
  }
  InsertMRUListItem(sri);
}

SessionCacheStorage::SessionCacheEntries* SessionCacheStorage::GetCacheEntriesForID(SessionCacheID id)
{
  if(!session_cache_enabled_)
    return NULL;

  SessionCache::iterator found =
      in_mem_session_cache_->find(id);

  if (found != in_mem_session_cache_->end())
    return found->second;
  return NULL;
}

void SessionCacheStorage::InsertCacheEntry(GURL response_url,
                                           scoped_refptr<SessionCacheResponseInfo> entry_info,
                                           SessionCacheID id,
                                           bool new_entry)
{
  DCHECK(session_cache_enabled_);
  SessionCacheEntries* cache_entries = GetCacheEntriesForID(id);

  // This is first entry for SessionCacheID
  if (cache_entries == NULL) {
    cache_entries = new SessionCacheEntries;
    in_mem_session_cache_->insert(SessionCacheItem(id, cache_entries));
  }
  cache_entries->insert(SessionCacheEntry(response_url, entry_info));

  // update the MRU list only if its a new_entry
  // old_entry would alread moved on top of MRU list from UpdateMRUList
  if (new_entry)
    InsertMRUListItem(entry_info);
}

void SessionCacheStorage::RemoveCacheEntry(SessionCacheEntries* cache_entries,
                                           GURL url)
{
  SessionCacheEntries::iterator itr = cache_entries->find(url);
  DCHECK(itr != cache_entries->end());
  scoped_refptr<SessionCacheResponseInfo> sri = itr->second;
  cache_entries->erase(url);

  RemoveMRUEntry(sri);
}

void SessionCacheStorage::RemoveCacheEntry(GURL url, SessionCacheID id)
{
  DCHECK(session_cache_enabled_);
  SessionCacheEntries* cache_entries = GetCacheEntriesForID(id);
  DCHECK(cache_entries);
  RemoveCacheEntry(cache_entries, url);
  // FIXME: Should we delete the SessionCacheEntries map from Session
  // if this was the last entry???
}

void SessionCacheStorage::RemoveCacheEntry(MRUListAsDq::iterator itr)
{
  scoped_refptr<SessionCacheResponseInfo> entry = *itr;
  mru_cache_entries_list_->erase(itr);

  SessionCacheEntries* cache_entries = GetCacheEntriesForID(entry->id());
  DCHECK(cache_entries);
  cache_entries->erase(entry->resource_url());

  // Reduce the session cache storage size as the entry is removed
  session_cache_storage_size_ -= entry->response_body_length();
}

void SessionCacheStorage::RemoveMRUEntry(
        scoped_refptr<SessionCacheResponseInfo> sri)
{
#ifndef NDEBUG
  bool found = false;
#endif

  for (MRUListAsDq::iterator i = mru_cache_entries_list_->begin();
       i != mru_cache_entries_list_->end(); ++i) {
    if ((*i).get() == sri.get()) {
        mru_cache_entries_list_->erase(i);
#ifndef NDEBUG
        found = true;
#endif
        break;
    }
  }
#ifndef NDEBUG  
  DCHECK(found);
#endif
  // Reduce the session cache storage size as the entry is removed
  session_cache_storage_size_ -= sri->response_body_length();
}

bool SessionCacheStorage::CacheStorageLimitReached()
{
  // FIXME: Consider header length for memory limit calculations,
  // currently only response body length is considered
  return session_cache_storage_size_ >
          ((KMaxSessionCacheStoreLimit  * KCacheCompactionTriggerPercent) / 100);
}

int SessionCacheStorage::RemoveExpiredEntries()
{
  int num_of_reclaimed_bytes = 0;
  SessionCache::iterator itrCache = in_mem_session_cache_->begin();
  while (itrCache != in_mem_session_cache_->end()) {
      SessionCacheEntries* cache_entries = itrCache->second;
      SessionCacheEntries::iterator cached_entry = cache_entries->begin();

      // FIXME: Remove the use of vector to avoid extra for loop
      std::vector<GURL> expired_entries;
      while (cached_entry != cache_entries->end()) {
        scoped_refptr<SessionCacheResponseInfo> sri =
                cached_entry->second;
        if (sri->expired()) {
            num_of_reclaimed_bytes += sri->response_body_length();

            // Copy the expired entries in a vector that would be removed later
            expired_entries.push_back(sri->resource_url());
        }
        ++cached_entry;
      }

      for (unsigned i = 0; i < expired_entries.size(); ++i) {
          RemoveCacheEntry(cache_entries, expired_entries[i]);
      }
      expired_entries.clear();
      ++itrCache;
  }
  return num_of_reclaimed_bytes;
}

void SessionCacheStorage::RemoveLRUEntries(int number_of_bytes_to_be_reclaimed)
{
  if (number_of_bytes_to_be_reclaimed <= 0)
    return;

  int num_of_bytes_reclaimed = 0;
  MRUListAsDq::reverse_iterator itrt = mru_cache_entries_list_->rbegin();

  std::vector<scoped_refptr<SessionCacheResponseInfo> > lruEntries;
  while (itrt != mru_cache_entries_list_->rend()) {
    scoped_refptr<SessionCacheResponseInfo> entry = *itrt;
    // check if we have to free more bytes
    // >if yes then remove the entry from MRU list and free the mem.
    if (number_of_bytes_to_be_reclaimed <= num_of_bytes_reclaimed)
        break;

    //update the byte count
    num_of_bytes_reclaimed += entry->response_body_length();
    //go ahead
    ++itrt;
    lruEntries.push_back(entry);
  }
  for (unsigned i = 0; i < lruEntries.size(); ++i) {
      scoped_refptr<SessionCacheResponseInfo> sri = lruEntries[i];
      RemoveCacheEntry(sri->resource_url(), sri->id());
  }
  lruEntries.clear();
}

}//namespace net
#endif
