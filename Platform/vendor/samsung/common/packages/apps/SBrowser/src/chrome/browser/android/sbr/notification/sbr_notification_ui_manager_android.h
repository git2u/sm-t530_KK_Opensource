
#ifndef CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_NOTIFICATION_UI_MANAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_NOTIFICATION_UI_MANAGER_ANDROID_H_
#pragma once

#include <deque>
#include <string>
#include <vector>

#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/timer.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "components/user_prefs/pref_registry_syncable.h"

#include "googleurl/src/gurl.h"
#include "base/android/jni_helper.h"

class Notification;
class Profile;
class QueuedNotification;

// The notification manager manages use of the desktop for notifications.
// It maintains a queue of pending notifications when space becomes constrained.
class NotificationUIManagerImpl
    : public NotificationUIManager,
      public content::NotificationObserver {
 public:
 
   NotificationUIManagerImpl();
  virtual ~NotificationUIManagerImpl();

    // Registers preferences.
     // Registers user preferences.

  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* prefs);

  // Adds a notification to be displayed. Virtual for unit test override.
  virtual void Add(const Notification& notification,
                   Profile* profile);

virtual bool DoesIdExist(const std::string& notification_id) ;
  // Removes any notifications matching the supplied ID, either currently
  // displayed or in the queue.  Returns true if anything was removed.
  virtual bool CancelById(const std::string& notification_id);

  // Removes any notifications matching the supplied source origin
  // (which could be an extension ID), either currently displayed or in the
  // queue.  Returns true if anything was removed.
  virtual bool CancelAllBySourceOrigin(const GURL& source_origin);
  virtual bool CancelAllByProfile(Profile* profile);
  // Cancels all pending notifications and closes anything currently showing.
  // Used when the app is terminating.
  void CancelAll();

  // Retrieves an ordered list of all queued notifications.
  // Used only for automation/testing.
  void GetQueuedNotificationsForTesting(
      std::vector<const Notification*>* notifications);

 protected:
  // Attempts to pass a notification from a waiting queue to the sublass
  // for presentation. Subclass can return 'false' if it can not show
  // notificaiton right away. In this case it should invoke
  // CheckAndShowNotificaitons() later.
  //virtual bool ShowNotification(const Notification& notification,
      //                          Profile* profile) ;

 // Replace an existing notification of the same id with this one if applicable;
 // subclass returns 'true' if the replacement happened.
 //virtual bool UpdateNotification(const Notification& notification) ;

 // Attempts to display notifications from the show_queue. Invoked by subclasses
 // if they previously returned 'false' from ShowNotifications, which may happen
 // when there is no room to show another notification. When room appears, the
 // subclass should call this method to cause an attempt to show more
 // notifications from the waiting queue.
 void CheckAndShowNotifications();

 private:
  // content::NotificationObserver override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  

  // Attempts to display notifications from the show_queue.
  void ShowNotifications();

  // Replace an existing notification with this one if applicable;
  // returns true if the replacement happened.
  bool TryReplacement(const Notification& notification);

  // Checks the user state to decide if we want to show the notification.
  void CheckUserState();

  // A queue of notifications which are waiting to be shown.
  typedef std::deque<QueuedNotification*> NotificationDeque;
  NotificationDeque show_queue_;

  // Registrar for the other kind of notifications (event signaling).
  content::NotificationRegistrar registrar_;

  // Used by screen-saver and full-screen handling support.
  bool is_user_active_;
  base::RepeatingTimer<NotificationUIManagerImpl> user_state_check_timer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationUIManagerImpl);
};

bool RegisterNotificationUIManagerImpl(JNIEnv* env);
#endif  // CHROME_BROWSER_ANDROID_SBR_NOTIFICATION_NOTIFICATION_UI_MANAGER_ANDROID_H_
