    // Copyright (c) 2012 The Chromium Authors. All rights reserved.
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_SBR_CONTENT_SETTINGS_H_
#define CONTENT_BROWSER_ANDROID_SBR_CONTENT_SETTINGS_H_



#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

  namespace content {

    class SbrContentSettings  : public content::WebContentsObserver {
		
     public:
      SbrContentSettings(JNIEnv* env, jobject obj);
	  
      virtual ~SbrContentSettings();

      // Synchronizes the Java settings from native settings.
      void SyncFromNative(JNIEnv* env, jobject obj);

      // Synchronizes the native settings from Java settings.
      void SyncToNative(JNIEnv* env, jobject obj);
     void SetWebContents(JNIEnv* env, jobject obj, jint web_contents);


     private:
      struct FieldIds;

      void SyncFromNativeImpl();
      void SyncToNativeImpl();
	

      // WebContentsObserver overrides:
      virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;
      virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

      // Java field references for accessing the values in the Java object.
      scoped_ptr<FieldIds> field_ids_;

      // The Java counterpart to this class.
     JavaObjectWeakGlobalRef sbr_content_settings_;
    };

    bool RegisterSbrContentSettings(JNIEnv* env);

    }  // namespace content

   #endif  


