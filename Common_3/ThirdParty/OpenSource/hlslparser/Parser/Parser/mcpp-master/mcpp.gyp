{
  'targets': [
    {
      'target_name': 'mcpp',
      'product_prefix' : 'lib',
      'type': 'static_library',
      'sources':  [
        'directive.c',
        'eval.c',
        'expand.c',
        'main.c',
        'mbchar.c',
        'support.c',
        'system.c',
      ],
      'win_delay_load_hook' : 'false',
      'include_dirs' : ['.'],
      'defines' : [
        'HAVE_CONFIG_H',
        'MCPP_LIB=1'
      ],
      'configurations': {
        'Release': {
          'msvs_settings': {
            'VCCLCompilerTool': {
                'RuntimeLibrary': '0',
                'ExceptionHandling': '1',
                'RuntimeTypeInfo' : 'true'
              },
          },
          'msvs_disabled_warnings': [
            4018,
            4090,
            4101,
            4102,
            4133,
            4146,
            4244,
            4267
          ]
        },
        'Debug': {
          'msvs_settings': {
            'VCCLCompilerTool': {
                'RuntimeLibrary': '1',
                'ExceptionHandling': '1',
                'RuntimeTypeInfo' : 'true'
              },
          },
          'msvs_disabled_warnings': [
            4018,
            4090,
            4101,
            4102,
            4133,
            4146,
            4244,
            4267
          ]
        }
      },
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            "MACOSX_DEPLOYMENT_TARGET":"10.9",
            'OTHER_CFLAGS': [
              '-fno-common',
              '-stdlib=libstdc++',
              '-w'
            ]
          }
        }],
        ['OS=="linux"', {
          'cflags' : [
            '-fPIC',
            '-w'
          ]
        }],
        ['OS=="win"', {
          'defines' : [
              '_WIN32_WINNT=0x600',
              'WIN32_LEAN_AND_MEAN'
          ]
        }]
      ]
    }
  ]
}
