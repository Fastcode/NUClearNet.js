{
    "targets": [
        {
            "target_name": "nuclearnet",
            "sources": [
                "src/nuclear/src/extension/network/NUClearNetwork.cpp"
            ],
            "cflags": [
                "-std=c++14"
            ],
            "include_dirs": [
                "<!(node -e \"require('nan')\")",
                "src/nuclear/src/nuclear/nuclear_bits"
            ],
            "conditions": [
                [
                    "OS=='linux'", {
                        "include_dirs": [
                        ],
                        "ccflags": [
                            "-fPIC"
                        ],
                        "libraries": [
                        ],
                    }
                ],
                [
                    'OS=="mac"',
                    {
                        "include_dirs": [
                        ],
                        "libraries": [
                        ],
                        "xcode_settings": {
                            "MACOSX_DEPLOYMENT_TARGET": '10.9',
                            "OTHER_CPLUSPLUSFLAGS": [
                                '-stdlib=libc++'
                            ]
                        }
                    }
                ]
            ]
        }
    ]
}
