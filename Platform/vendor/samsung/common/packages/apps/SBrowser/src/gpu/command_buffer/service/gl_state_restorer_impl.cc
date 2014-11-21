// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_state_restorer_impl.h"

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"

namespace gpu {

GLStateRestorerImpl::GLStateRestorerImpl(
    base::WeakPtr<gles2::GLES2Decoder> decoder)
    : decoder_(decoder) {
}

GLStateRestorerImpl::~GLStateRestorerImpl() {
}
#if defined (SBROWSER_GRAPHICS_MAKE_VIR_CURRNT )
bool GLStateRestorerImpl::IsInitialized() {
  DCHECK(decoder_.get());
  return decoder_->initialized();
}
#endif
void GLStateRestorerImpl::RestoreState() {
  DCHECK(decoder_.get());
  decoder_->RestoreState();
}

}  // namespace gpu
