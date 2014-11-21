# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'include_dirs': [
    '..',
  ],
  'conditions': [
   	['enable_sbrowser_apk==1',{
	  'conditions': [			
		['enable_sbrowser_native_tex_upload==1', {
			'include_dirs': [
			    '<(DEPTH)/../../../../../../frameworks/native/include/',
			    '<(DEPTH)/../../../../../../hardware/libhardware/include/',
			    '<(DEPTH)/../../../../../../hardware/libhardware/include/hardware',
			    '<(DEPTH)/../../../../../../system/core/include/',
			    '<(DEPTH)/../../../../../../frameworks/base/native/include/',
			    '<(DEPTH)/../../../../../../frameworks/base/opengl/include/',
			    '<(DEPTH)/../../../../../../frameworks/base/opengl/include/',
			    '<(DEPTH)/../../../../../../frameworks/base/include/',
			  ],
		}],
  	    ],
	}],
  ],
  'all_dependent_settings': {
    'include_dirs': [
      '..',
    ],
  },
  'dependencies': [
    '../third_party/khronos/khronos.gyp:khronos_headers',
  ],
  'sources': [
    'command_buffer/client/atomicops.cc',
    'command_buffer/client/atomicops.h',
    'command_buffer/client/cmd_buffer_helper.cc',
    'command_buffer/client/cmd_buffer_helper.h',
    'command_buffer/client/fenced_allocator.cc',
    'command_buffer/client/fenced_allocator.h',
    'command_buffer/client/hash_tables.h',
    'command_buffer/client/mapped_memory.cc',
    'command_buffer/client/mapped_memory.h',
    'command_buffer/client/ring_buffer.cc',
    'command_buffer/client/ring_buffer.h',
    'command_buffer/client/transfer_buffer.cc',
    'command_buffer/client/transfer_buffer.h',
  ],
}
