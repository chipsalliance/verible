# This library contains tools for transforming Verilog code.

default_visibility = [
    "//verible/verilog/tools/obfuscator:__subpackages__",
    "//verible/verilog/tools/preprocessor:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
    features = ["layering_check"],
)

cc_library(
    name = "obfuscate",
    srcs = ["obfuscate.cc"],
    hdrs = ["obfuscate.h"],
    deps = [
        "//verible/common/strings:obfuscator",
        "//verible/common/strings:random",
        "//verible/common/text:token-info",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/verilog/analysis:verilog-equivalence",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "obfuscate_test",
    srcs = ["obfuscate_test.cc"],
    deps = [
        ":obfuscate",
        "//verible/common/strings:obfuscator",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "strip-comments",
    srcs = ["strip-comments.cc"],
    hdrs = ["strip-comments.h"],
    deps = [
        "//verible/common/strings:comment-utils",
        "//verible/common/strings:range",
        "//verible/common/text:token-info",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/common/util:spacer",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
    ],
)

cc_test(
    name = "strip-comments_test",
    srcs = ["strip-comments_test.cc"],
    deps = [
        ":strip-comments",
        "@abseil-cpp//absl/strings:string_view",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
