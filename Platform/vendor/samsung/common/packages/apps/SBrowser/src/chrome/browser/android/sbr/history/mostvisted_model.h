
#ifndef CHROME_BROWSER_ANDROID_SBR_HISTORY_MOSTVISITED_MODLE_H
#define CHROME_BROWSER_ANDROID_SBR_HISTORY_MOSTVISITED_MODLE_H


#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/time_format.h"
#include "base/android/jni_helper.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/browser/web_ui.h"
#include <string>


class MostVisitedModel {

  public:
  MostVisitedModel(JNIEnv* env, jobject obj);

  ~MostVisitedModel();

  int GetVisitCountForURL(JNIEnv* env, jobject obj, jstring url);

  void Destroy(JNIEnv* env, jobject obj);

  private:
  JavaObjectWeakGlobalRef weak_java_mostvisited_model;

};
bool RegisterMostVisitedModel(JNIEnv* env);
#endif //define CHROME_BROWSER_ANDROID_SBR_HISTORY_MOSTVISITED_MODLE_H
