# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'conditions': [
        ['enable_sbrowser_apk==1', {
        'conditions': [
          ['enable_platform_zlib==1', {  # platform zlib
            'target_name': 'zlib',
            'type': 'none',
            'direct_dependent_settings': {
              'defines': [
                'USE_PLATFORM_ZLIB',
              ],
              'include_dirs': [
                'zlib/',
              ],
            },
            'link_settings': {
              'libraries': [
                '<(android_product_out)/system/lib/libz.so',
              ],
            },
            'toolsets': ['target', 'host'],
          },{  # chromium zlib
            'target_name': 'zlib',
            'type': 'static_library',
            'sources': [
            'adler32.c',
            'compress.c',
            'crc32.c',
            'crc32.h',
            'deflate.c',
            'deflate.h',
            'gzclose.c',
            'gzguts.h',
            'gzlib.c',
            'gzread.c',
            'gzwrite.c',
            'infback.c',
            'inffast.c',
            'inffast.h',
            'inffixed.h',
            'inflate.c',
            'inflate.h',
            'inftrees.c',
            'inftrees.h',
            'mozzconf.h',
            'trees.c',
            'trees.h',
            'uncompr.c',
            'zconf.h',
            'zlib.h',
            'zutil.c',
            'zutil.h',
          ], 
          'include_dirs': [
            '.',
          ],
          'direct_dependent_settings': {
            'include_dirs': [
              '.',
            ],
          },
         'conditions': [
           ['OS!="win"', {
             'product_name': 'chrome_zlib',
           }], ['OS=="android"', {
             'toolsets': ['target', 'host'],
           }],
         ],
        },
      ],],},  
      {  # chromium zlib
      'target_name': 'zlib',
      'type': 'static_library',
      'sources': [
        'adler32.c',
        'compress.c',
        'crc32.c',
        'crc32.h',
        'deflate.c',
        'deflate.h',
        'gzclose.c',
        'gzguts.h',
        'gzlib.c',
        'gzread.c',
        'gzwrite.c',
        'infback.c',
        'inffast.c',
        'inffast.h',
        'inffixed.h',
        'inflate.c',
        'inflate.h',
        'inftrees.c',
        'inftrees.h',
        'mozzconf.h',
        'trees.c',
        'trees.h',
        'uncompr.c',
        'zconf.h',
        'zlib.h',
        'zutil.c',
        'zutil.h',
      ],
      'include_dirs': [
        '.',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS!="win"', {
          'product_name': 'chrome_zlib',
        }], ['OS=="android"', {
          'toolsets': ['target', 'host'],
        }],
      ],
    },
  ],],},
    {
      'target_name': 'minizip',
      'type': 'static_library',
      'sources': [
        'contrib/minizip/ioapi.c',
        'contrib/minizip/ioapi.h',
        'contrib/minizip/iowin32.c',
        'contrib/minizip/iowin32.h',
        'contrib/minizip/unzip.c',
        'contrib/minizip/unzip.h',
        'contrib/minizip/zip.c',
        'contrib/minizip/zip.h',
      ],
      'dependencies': [
        'zlib',
      ],
      'include_dirs': [
        '.',
        '../..',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '.',
        ],
      },
      'conditions': [
        ['OS!="win"', {
          'sources!': [
            'contrib/minizip/iowin32.c'
          ],
        }],
        ['OS=="android"', {
          'toolsets': ['target', 'host'],
        }],
        ['OS=="mac" or OS=="ios" or os_bsd==1 or OS=="android"', {
          # Mac, Android and the BSDs don't have fopen64, ftello64, or
          # fseeko64. We use fopen, ftell, and fseek instead on these
          # systems.
          'defines': [
            'USE_FILE32API'
          ],
        }],
        ['clang==1', {
          'xcode_settings': {
            'WARNING_CFLAGS': [
              # zlib uses `if ((a == b))` for some reason.
              '-Wno-parentheses-equality',
            ],
          },
          'cflags': [
            '-Wno-parentheses-equality',
          ],
        }],
      ],
    },
    {
      'target_name': 'zip',
      'type': 'static_library',
      'dependencies': [
        'minizip',
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'google/zip.cc',
        'google/zip.h',
        'google/zip_internal.cc',
        'google/zip_internal.h',
        'google/zip_reader.cc',
        'google/zip_reader.h',
      ],
    },
  ],
}
