{
    'targets': [
        {
            'target_name': 'nuclearnet',
            'sources': [
                'src/binding.cpp',
                'src/NetworkBinding.cpp',
                'src/NetworkListener.cpp',
                'src/nuclear/src/extension/network/NUClearNetwork.cpp',
                'src/nuclear/src/util/platform.cpp',
                'src/nuclear/src/util/network/get_interfaces.cpp',
                'src/nuclear/src/util/network/resolve.cpp',
                'src/nuclear/src/util/serialise/xxhash.cpp'
            ],
            'cflags': [],
            'include_dirs': [
                '<!@(node -p "require(\'node-addon-api\').include")',
                'src/nuclear/src/include'
            ],
            "defines": [
                # Restrict NAPI to v6 (to support Node v10)
                # Changing this should have a corresponding change to "engines" in package.json
                # See https://nodejs.org/api/n-api.html#node-api-version-matrix for which
                # versions of NAPI support which versions of Node
                "NAPI_VERSION=6"
            ],
            'msvs_settings': {
                'VCCLCompilerTool': {
                    'ExceptionHandling': 1
                }
            },
            'conditions': [
                [
                    'OS=="linux"', {
                        'ccflags': [
                            '-std=c++14'
                            '-fPIC',
                            '-fext-numeric-literals',
                            '-fexceptions'
                        ],
                        'ccflags!': [
                            '-fno-exceptions'
                        ],
                        'cflags_cc': [
                            '-std=c++14',
                            '-fext-numeric-literals'
                        ],
                        'cflags_cc!': [
                            '-fno-exceptions',
                            '-fno-rtti'
                        ],
                    }
                ],
                [
                    'OS=="mac"', {
                        'ccflags': [
                            '-std=c++14'
                            '-fPIC',
                            '-fext-numeric-literals',
                            '-fexceptions'
                        ],
                        'ccflags!': [
                            '-fno-exceptions'
                        ],
                        'cflags_cc': [
                            '-std=c++14',
                            '-fext-numeric-literals'
                        ],
                        'cflags_cc!': [
                            '-fno-exceptions',
                            '-fno-rtti'
                        ],
                        'xcode_settings': {
                            'MACOSX_DEPLOYMENT_TARGET': '10.9',
                            'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                            'GCC_ENABLE_CPP_RTTI': 'YES',
                            'OTHER_CPLUSPLUSFLAGS': ['-std=c++14', '-stdlib=libc++'],
                            'OTHER_LDFLAGS': ['-stdlib=libc++']
                        }
                    }
                ],
                [
                    'OS=="win"', {
                        'defines': [ '_HAS_EXCEPTIONS=1' ]
                    }
                ]
            ]
        }
    ]
}
