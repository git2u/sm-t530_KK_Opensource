# Copyright (C) 2012 kikin Inc.
#
# All Rights Reserved.
#
# Author: Kapil Goel

#Build kikin library

{
    'includes': [
        '../WebKit/Source/core/core.gypi',
        '../WebKit/Source/wtf/wtf.gypi'
    ],
    'targets': [
        {
            'target_name': 'libkikin',
            'type': 'static_library',
            'variables': { 
            	'enable_wexit_time_destructors': 1,
        	},
            'dependencies': [
                '../WebKit/Source/core/core.gyp/core.gyp:webcore',
                '../WebKit/Source/Platform/Platform.gyp/Platform.gyp:webkit_platform', # actually WebCore should depend on this
                '../cld/cld.gyp:cld',
            ],
            'include_dirs': [
                '.',
                '../cld'
            ],
            'defines': [
                'WEBKIT_IMPLEMENTATION=1',
            ],
            'sources': [
                'kikin/annotation/html/AddressMicroformat.cpp',
                'kikin/annotation/html/AddressMicroformat.h',
                'kikin/html/HtmlAnalyzer.cpp',
                'kikin/html/HtmlAnalyzer.h',
                'kikin/html/HtmlEntity.cpp',
                'kikin/html/HtmlEntity.h',
                'kikin/html/HtmlRange.cpp',
                'kikin/html/HtmlRange.h',
                'kikin/html/HtmlToken.cpp',
                'kikin/html/HtmlToken.h',
                'kikin/html/HtmlTokenizer.cpp',
                'kikin/html/HtmlTokenizer.h',
                'kikin/html/character/HtmlTokenizer.cpp',
                'kikin/html/character/HtmlTokenizer.h',
                'kikin/html/cn/HtmlTokenizer.cpp',
                'kikin/html/cn/HtmlTokenizer.h',
                'kikin/list/ClientRects.cpp',
                'kikin/list/ClientRects.h',
                'kikin/list/Ranges.cpp',
                'kikin/list/Ranges.h',
                'kikin/list/Tokens.cpp',
                'kikin/list/Tokens.h',
                'kikin/util/KikinUtils.cpp',
                'kikin/util/KikinUtils.h',
                'kikin/LanguageDetector.cpp',
                'kikin/LanguageDetector.h'
            ],
            'copies': [
		        {
		          	'destination': '<(SHARED_LIB_DIR)',
		          	'files': [
		            	'libs/libkikin_common.so',
		            	'libs/libkikin.so',
		          	],
		        }
		    ],
            'link_settings': {
                'libraries': [
                    '../../../third_party/kikin/libs/libkikin_common.so',
      		        '../../../third_party/kikin/libs/libkikin.so',
                ],
            },
            'ldflags': [
                '-lpthread',
                '-ldl'
            ],
            'cflags': [
                '-fvisibility=hidden'
            ],
            'cflags_cc': [
                '-fvisibility=hidden',
            ],
		}
    ]
}
