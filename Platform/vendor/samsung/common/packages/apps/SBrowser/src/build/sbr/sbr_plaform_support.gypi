
{
  'targets': [
# SAMSUNG_CHANGE : smartcard_certificate >>>
    {
      'target_name': 'smartcard_certificate',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
	   '<(android_src)/vendor/samsung/common/frameworks/secSmartcardAuth/include',
        ],
      },
      'link_settings': {
        'conditions': [
          ['target_arch=="arm"', {
            'libraries': [
              '<(android_src)/vendor/samsung/common/frameworks/secSmartcardAuth/lib/armeabi/libSamsungPkcs11Wrapper.so',
              '<(android_src)/vendor/samsung/common/frameworks/secSmartcardAuth/lib/armeabi/libsecpkcs11_engine.so',
            ],
          }],
          ['target_arch=="x86"', {
            'libraries': [
              '<(android_src)/vendor/samsung/common/frameworks/secSmartcardAuth/lib/x86/libSamsungPkcs11Wrapper.so',
              '<(android_src)/vendor/samsung/common/frameworks/secSmartcardAuth/lib/x86/libsecpkcs11_engine.so',
            ],
          }],
        ],
      },
    },
# SAMSUNG_CHANGE : smartcard_certificate <<<
# CSC Feature change >>>
    {
      'target_name': 'csc_feature',
      'type': 'none',
      'toolsets': ['host', 'target'],
      'direct_dependent_settings': {
        'include_dirs': [
#          '<(android_src)/vendor/samsung/feature/CscFeature/libsecnativefeature',
#          '<(android_src)/vendor/samsung/feature/CscFeature',
              '<(android_src)/vendor/samsung/feature/CscFeature',
              '<(android_src)/vendor/samsung/feature/CscFeature/libsecnativefeature',
        ],
      },
      'link_settings': {
        'libraries': [
          '<(android_product_out)/system/lib/libsecnativefeature.so',
        ],
      },
    },
# CSC Feature change >>>
  ],
}

