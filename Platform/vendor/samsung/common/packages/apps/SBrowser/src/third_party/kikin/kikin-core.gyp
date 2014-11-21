# Copyright (c) 2012 kikin Inc. 
# All Rights Reserved. 
# Author: Kapil Goel
{
  'targets': [
    {
      'target_name': 'kikin_core_javalib',
      'type': 'none',
      'variables': {
        'jar_path': 'libs/kikin-core.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    }
  ],
}
