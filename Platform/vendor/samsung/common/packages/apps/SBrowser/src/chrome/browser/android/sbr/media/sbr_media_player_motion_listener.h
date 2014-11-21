
#ifndef  CHROME_BROWSER_ANDROID_SBR_MEDIA_SBR_MEDIA_PLAYER_MOTION_LISTENER_H_
#define CHROME_BROWSER_ANDROID_SBR_MEDIA_SBR_MEDIA_PLAYER_MOTION_LISTENER_H_
#pragma once

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class MediaPlayerBridge;

class SbrMediaPlayerMotionListener {
 public:
  SbrMediaPlayerMotionListener(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      base::WeakPtr<MediaPlayerBridge> media_player);
  virtual ~SbrMediaPlayerMotionListener();

  // Create a Java MediaPlayerListener object.
  void CreateMediaPlayerMotionListener(jobject context, jobject media_player);

  void PauseMedia(JNIEnv* /* env */, jobject /* obj */);

  void RegisterReceiver();
  void UnRegisterReceiver();

  static bool RegisterSbrMediaPlayerMotionListener(JNIEnv* env);
  
 private:
  // The message loop where |media_player_| lives.
  scoped_refptr<base::MessageLoopProxy> message_loop_;

  // The MediaPlayerBridge object all the callbacks should be send to.
  base::WeakPtr<MediaPlayerBridge> media_player_;

  base::android::ScopedJavaGlobalRef<jobject> j_media_player_motion_listener_;

  DISALLOW_COPY_AND_ASSIGN(SbrMediaPlayerMotionListener);
};

} // namespace media

#endif // CHROME_BROWSER_ANDROID_SBR_NET_SBR_CUSTOM_HTTP_REQUEST_HEADER_H_
