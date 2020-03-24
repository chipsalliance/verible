# This library contains tools for transforming Verilog code.

licenses(["notice"])

default_visibility = [
    "//verilog/tools/obfuscator:__subpackages__",
]

package(
    default_visibility = default_visibility,
)

cc_library(
    name = "obfuscate",
    srcs = ["obfuscate.cc"],
    hdrs = ["obfuscate.h"],
    deps = [
        "//common/strings:obfuscator",
        "//common/text:token_info",
        "//common/util:logging",
        "//verilog/analysis:verilog_equivalence",
        "//verilog/parser:verilog_lexer",
        "//verilog/parser:verilog_token_enum",
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
        "@com_google_googletest//:gtest_main",
    ],
)
