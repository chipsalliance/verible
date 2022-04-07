# This package contains libraries related to SystemVerilog preprocessing.

licenses(["notice"])

package(
    default_visibility = [
        "//verilog:__subpackages__",
        # TODO(b/130113490): standalone preprocessor tool
    ],
)

cc_library(
    name = "verilog_preprocess",
    srcs = ["verilog_preprocess.cc"],
    hdrs = ["verilog_preprocess.h"],
    deps = [
        "//common/lexer:token_generator",
        "//common/lexer:token_stream_adapter",
        "//common/text:macro_definition",
        "//common/text:token_info",
        "//common/text:token_stream_view",
        "//common/util:container_util",
        "//common/util:logging",
        "//verilog/parser:verilog_parser",
        "//verilog/parser:verilog_token_enum",
        "//verilog/parser:verilog_lexer",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "verilog_preprocess_test",
    srcs = ["verilog_preprocess_test.cc"],
    deps = [
        ":verilog_preprocess",
        "//common/text:macro_definition",
        "//common/text:token_info",
        "//common/util:container_util",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/parser:verilog_token_enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
