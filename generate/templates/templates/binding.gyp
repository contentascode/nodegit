{
  "targets": [
    {
      "target_name": "nodegit",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "xcode_settings": { "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
        "CLANG_CXX_LIBRARY": "libc++",
        "MACOSX_DEPLOYMENT_TARGET": "10.7",
      },
      "msvs_settings": {
        "VCCLCompilerTool": { "ExceptionHandling": 1 },
      },

      "dependencies": [
        "<!(node -p 'require(\"node-addon-api\").gyp')",
        "vendor/libgit2.gyp:libgit2"
      ],

      "variables": {
        "coverage%": 0
      },
      "sources": [
        "src/async_baton.cc",
        "src/lock_master.cc",
        "src/nodegit.cc",
        "src/init_ssh2.cc",
        "src/promise_completion.cc",
        "src/wrapper.cc",
        "src/functions/copy.cc",
        "src/functions/free.cc",
        "src/convenient_patch.cc",
        "src/convenient_hunk.cc",
        "src/filter_registry.cc",
        "src/git_buf_converter.cc",
        "src/str_array_converter.cc",
        "src/thread_pool.cc",
        {% each %}
          {% if type != "enum" %}
            "src/{{ name }}.cc",
          {% endif %}
        {% endeach %}
      ],

      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "vendor/libv8-convert",
        "vendor/libssh2/include",
        "vendor/openssl/openssl/include",

      ],

      "cflags": [
        "-Wall"
      ],

      "conditions": [
        [
          "coverage==1", {
            "cflags": [
              "-ftest-coverage",
              "-fprofile-arcs"
            ],
            "link_settings": {
              "libraries": [
                "-lgcov"
              ]
            },
          }
        ],
        [
          "OS=='mac'", {
            "xcode_settings": {
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "MACOSX_DEPLOYMENT_TARGET": "10.7",

              "WARNING_CFLAGS": [
                "-Wno-unused-variable",
                "-Wint-conversions",
                "-Wmissing-field-initializers",
                "-Wno-c++11-extensions"
              ]
            }
          }
        ],
        [
          "OS=='win'", {
            "defines": [
              "_HAS_EXCEPTIONS=1"
            ],
            "msvs_settings": {
              "VCCLCompilerTool": {
                "AdditionalOptions": [
                  "/EHsc"
                ]
              },
              "VCLinkerTool": {
                "AdditionalOptions": [
                  "/FORCE:MULTIPLE"
                ]
              }
            }
          }
        ],
        [
          "OS=='linux' or OS=='mac'", {
            "libraries": [
              "-lcurl"
            ]
          }
        ],
        [
          "OS=='linux' or OS.endswith('bsd')", {
            "cflags": [
              "-std=c++11"
            ]
          }
        ]
      ]
    }
  ]
}
