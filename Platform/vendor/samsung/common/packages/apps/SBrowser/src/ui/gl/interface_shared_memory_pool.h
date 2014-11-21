// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_INTERFACE_SHARED_MEMORY_POOL_H_
#define UI_GL_INTERFACE_SHARED_MEMORY_POOL_H_

#if defined(SBROWSER_IPC_SHARED_MEMORY_TRACKING)

class SharedMemoryPoolInterface {
 public:
  SharedMemoryPoolInterface() {};
  virtual ~SharedMemoryPoolInterface() {};

  virtual void NotifyMMapFailure() = 0;
};

#endif
#endif  // UI_GL_INTERFACE_SHARED_MEMORY_POOL_H_

