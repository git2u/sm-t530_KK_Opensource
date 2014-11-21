#ifndef SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_
#define SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_

#include "chrome/browser/android/google_location_settings_helper.h"

class SbrGoogleLocationSettingsHelperImpl
    : public GoogleLocationSettingsHelper {
 public:
  // GoogleLocationSettingsHelper implementation:
  virtual std::string GetAcceptButtonLabel() OVERRIDE;
  virtual void ShowGoogleLocationSettings() OVERRIDE;
  virtual bool IsMasterLocationSettingEnabled() OVERRIDE;
  virtual bool IsGoogleAppsLocationSettingEnabled() OVERRIDE;

 protected:
  SbrGoogleLocationSettingsHelperImpl();
  virtual ~SbrGoogleLocationSettingsHelperImpl();

 private:
  friend class GoogleLocationSettingsHelper;

  DISALLOW_COPY_AND_ASSIGN(SbrGoogleLocationSettingsHelperImpl);
};

#endif  // SBR_GOOGLE_LOCATION_SETTINGS_HELPER_H_
