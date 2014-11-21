# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{  
'conditions': [
   ['enable_sbrowser_apk==1',{
	'conditions': [			
		['enable_sbrowser_native_tex_upload==1', {
			'include_dirs': [ ## JKK added
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
}
