{
  'targets': [
    {
      'target_name': 'images',
      'product_name': 'skia_images',
      'type': 'static_library',
      'standalone_static_library': 1,
      'dependencies': [
        'libjpeg.gyp:*',
        'libwebp.gyp:libwebp',
        'utils.gyp:utils',
      ],
      'export_dependent_settings': [
        'libjpeg.gyp:*',
      ],
      'include_dirs': [
        '../include/config',
        '../include/core',
        '../include/images',
        '../include/lazy',
        # for access to SkErrorInternals.h
        '../src/core/',
        # for access to SkImagePriv.h
        '../src/image/',
      ],
      'sources': [
        '../include/images/SkImageDecoder.h',
        '../include/images/SkImageEncoder.h',
        '../include/images/SkImageRef.h',
        '../include/images/SkImageRef_GlobalPool.h',
        '../src/images/SkJpegUtility.h',
        '../include/images/SkMovie.h',
        '../include/images/SkPageFlipper.h',

        '../src/images/bmpdecoderhelper.cpp',
        '../src/images/bmpdecoderhelper.h',

        '../src/images/SkBitmapRegionDecoder.cpp',

        '../src/images/SkImageDecoder.cpp',
        '../src/images/SkImageDecoder_FactoryDefault.cpp',
        '../src/images/SkImageDecoder_FactoryRegistrar.cpp',
        # If decoders are added/removed to/from (all/individual)
        # platform(s), be sure to update SkImageDecoder.cpp:force_linking
        # so the right decoders will be forced to link.
        '../src/images/SkImageDecoder_libbmp.cpp',
        '../src/images/SkImageDecoder_libgif.cpp',
        '../src/images/SkImageDecoder_libico.cpp',
        '../src/images/SkImageDecoder_libjpeg.cpp',
        '../src/images/SkImageDecoder_libpng.cpp',
        '../src/images/SkImageDecoder_libwebp.cpp',
        '../src/images/SkImageDecoder_wbmp.cpp',
        '../src/images/SkImageEncoder.cpp',
        '../src/images/SkImageEncoder_Factory.cpp',
        '../src/images/SkImageEncoder_argb.cpp',
        '../src/images/SkImageRef.cpp',
        '../src/images/SkImageRefPool.cpp',
        '../src/images/SkImageRefPool.h',
        '../src/images/SkImageRef_ashmem.h',
        '../src/images/SkImageRef_ashmem.cpp',
        '../src/images/SkImageRef_GlobalPool.cpp',
        '../src/images/SkImages.cpp',
        '../src/images/SkJpegUtility.cpp',
        '../src/images/SkMovie.cpp',
        '../src/images/SkMovie_gif.cpp',
        '../src/images/SkPageFlipper.cpp',
        '../src/images/SkScaledBitmapSampler.cpp',
        '../src/images/SkScaledBitmapSampler.h',

        '../src/ports/SkImageDecoder_CG.cpp',
        '../src/ports/SkImageDecoder_WIC.cpp',
      ],
      'conditions': [
        [ 'skia_os == "win"', {
          'sources!': [
            '../src/images/SkImageDecoder_FactoryDefault.cpp',
            '../src/images/SkImageDecoder_libgif.cpp',
            '../src/images/SkImageDecoder_libpng.cpp',
            '../src/images/SkMovie_gif.cpp',
          ],
          'link_settings': {
            'libraries': [
              'windowscodecs.lib',
            ],
          },
        },{ #else if skia_os != win
          'sources!': [
            '../src/ports/SkImageDecoder_WIC.cpp',
          ],
        }],
        [ 'skia_os in ["mac", "ios"]', {
          'sources!': [
            '../src/images/SkImageDecoder_FactoryDefault.cpp',
            '../src/images/SkImageDecoder_libpng.cpp',
            '../src/images/SkImageDecoder_libgif.cpp',
            '../src/images/SkMovie_gif.cpp',
          ],
        },{ #else if skia_os != mac
          'sources!': [
            '../src/ports/SkImageDecoder_CG.cpp',
          ],
        }],
        [ 'skia_os in ["linux", "freebsd", "openbsd", "solaris"]', {
          # Any targets that depend on this target should link in libpng, libgif, and
          # our code that calls it.
          # See http://code.google.com/p/gyp/wiki/InputFormatReference#Dependent_Settings
          'link_settings': {
            'sources': [
              '../src/images/SkImageDecoder_libpng.cpp',
            ],
            'libraries': [
              '-lgif',
              '-lpng',
              '-lz',
            ],
          },
          # end libpng/libgif stuff
        }],
        # FIXME: NaCl should be just like linux, etc, above, but it currently is separated out
        # to remove gif. Once gif is supported by naclports, this can be merged into the above
        # condition.
        [ 'skia_os == "nacl"', {
          'sources!': [
            '../src/images/SkImageDecoder_libgif.cpp',
            '../src/images/SkMovie_gif.cpp',
          ],
          'link_settings': {
            'sources': [
              '../src/images/SkImageDecoder_libpng.cpp',
            ],
            'libraries': [
              '-lpng',
              '-lz',
            ],
          },
        }],
        [ 'skia_os == "android"', {
          'include_dirs': [
             '../src/utils',
          ],
          'dependencies': [
             'android_deps.gyp:gif',
             'android_deps.gyp:png',
          ],
        },{ #else if skia_os != android
          'sources!': [
            '../src/images/SkImageRef_ashmem.h',
            '../src/images/SkImageRef_ashmem.cpp',
          ],
        }],
        [ 'skia_os == "ios"', {
           'include_dirs': [
             '../include/utils/mac',
           ],
        }],
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '../include/images',
          '../include/lazy',
        ],
      },
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
