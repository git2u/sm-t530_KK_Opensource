// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_

namespace autofill {

// An interface the PersonalDataManager uses to notify its clients (observers)
// when it has finished loading personal data from the web database.  Register
// observers via PersonalDataManager::AddObserver.
class PersonalDataManagerObserver {
 public:
  // Notifies the observer that the PersonalDataManager changed in some way.
  virtual void OnPersonalDataChanged() = 0;
  #if defined (SBROWSER_SBRNATIVE_IMPL)
  virtual void OnPersonalDataChangeFailed() = 0;
  #endif
  virtual void OnInsufficientFormData() {}

 protected:
  virtual ~PersonalDataManagerObserver() {}
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_PERSONAL_DATA_MANAGER_OBSERVER_H_
