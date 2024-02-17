# This library contains tools for transforming Verilog code.

default_visibility = [
    "//verilog/tools/obfuscator:__subpackages__",
    "//verilog/tools/preprocessor:__subpackages__",
]

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = default_visibility,
)

cc_library(
    name = "obfuscate",
    srcs = ["obfuscate.cc"],
    hdrs = ["obfuscate.h"],
    deps = [
        "//common/strings:obfuscator",
        "//common/strings:random",
        "//common/text:token-info",
        "//common/util:logging",
        "//common/util:status-macros",
        "//verilog/analysis:verilog-equivalence",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "obfuscate_test",
    srcs = ["obfuscate_test.cc"],
    deps = [
        ":obfuscate",
        "//common/strings:obfuscator",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "strip-comments",
    srcs = ["strip_comments.cc"],
    hdrs = ["strip_comments.h"],
    deps = [
        "//common/strings:comment-utils",
        "//common/strings:range",
        "//common/text:token-info",
        "//common/util:iterator-range",
        "//common/util:spacer",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "strip-comments_test",
    srcs = ["strip_comments_test.cc"],
    deps = [
        ":strip-comments",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
