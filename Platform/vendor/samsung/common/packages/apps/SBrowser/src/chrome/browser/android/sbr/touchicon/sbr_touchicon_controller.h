
#ifndef TOUCHICON_CONTROLLER_H_
#define TOUCHICON_CONTROLLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "content/public/browser/url_data_source.h"
#include "base/android/jni_helper.h"
#include "chrome/browser/favicon/favicon_handler.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"



class Profile;

// TouchIconController is the gateway between network-level chrome:
// requests for favicons and the history backend that serves these.
class SbrTouchiconController : public content::URLDataSource {
 public:
  // Defines the type of icon the FaviconController will provide.
  //enum IconType {
  //  FAVICON,
    // Any available icon in the priority of TOUCH_ICON_PRECOMPOSED, TOUCH_ICON,
    // FAVICON, and default favicon.
  //  ANY
  //};


  // |type| is the type of icon this FaviconController will provide.
  SbrTouchiconController(JNIEnv* env, jobject obj, Profile* profile/*, IconType type*/);

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                int render_process_id,
                                int render_view_id,
                                 const content::URLDataSource::GotDataCallback& 
                                 callback) OVERRIDE;

  virtual std::string GetMimeType(const std::string&) const OVERRIDE;

  virtual bool ShouldReplaceExistingSource() const OVERRIDE;
 
  // Callback for the "Destroy" message.
  void Destroy(JNIEnv* env, jobject obj);

  void GetTouchicon(JNIEnv* env, jobject obj, jstring jurl);

 private:
  

  // Defines the allowed pixel sizes for requested favicons.
  enum TouchIconSize {
    SIZE_16,
    SIZE_32,
    SIZE_64,
    NUM_SIZES
  };
  
 protected:
  struct TouchIconRequest {
    TouchIconRequest();
    TouchIconRequest(const content::URLDataSource::GotDataCallback& cb,
                const std::string& path,
                int size,
                ui::ScaleFactor scale);
    ~TouchIconRequest();

    content::URLDataSource::GotDataCallback callback;
    std::string request_path;
    int size_in_dip;
    ui::ScaleFactor scale_factor;
  };
  
  // Called when the Touchicon data is missing to perform additional checks to
  // locate the resource.
  // |request| contains information for the failed request.
  // Returns true if the missing resource is found.
  virtual bool HandleMissingResource(const TouchIconRequest& request);
 virtual std::string GetSource() const OVERRIDE;

  Profile* profile_;


    CancelableTaskTracker cancelable_task_tracker_;

  // Raw PNG representations of favicons of each size to show when the favicon
  // database doesn't have a favicon for a webpage. Indexed by IconSize values.
  scoped_refptr<base::RefCountedMemory> default_touchicons_[NUM_SIZES];

  // The history::IconTypes of icon that this FaviconSource handles.
  int icon_types_;

  // Map from request ID to size requested (in pixels). TODO(estade): Get rid of
  // this map when we properly support multiple favicon sizes.
  std::map<int, int> request_size_map_;

  std::map<int, std::string> request_url_map;

 
  JavaObjectWeakGlobalRef weak_java_touchicon_controller;

  int map_index;

  void  OnTouchiconDataAvailable (  const TouchIconRequest& request, const history::FaviconBitmapResult& bitmap_result);
   // Sends the 16x16 DIP 1x default Touchicon.
  void SendDefaultResponse(
      const content::URLDataSource::GotDataCallback& callback);
  // Sends the default favicon.
  void SendDefaultResponse(const TouchIconRequest& icon_request);

  void SendTouchicon(std::string url,  base::RefCountedMemory* bytes);

  virtual ~SbrTouchiconController();
};
// Registers the TouchiconController native method.
bool RegisterSbrTouchiconController(JNIEnv* env);
#endif  // TOUCHICON_CONTROLLER_H_
