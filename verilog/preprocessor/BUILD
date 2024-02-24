# This package contains libraries related to SystemVerilog preprocessing.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verilog:__subpackages__",
        # TODO(b/130113490): standalone preprocessor tool
    ],
    features = ["layering_check"],
)

cc_library(
    name = "verilog-preprocess",
    srcs = ["verilog_preprocess.cc"],
    hdrs = ["verilog_preprocess.h"],
    deps = [
        "//common/lexer:token-generator",
        "//common/lexer:token-stream-adapter",
        "//common/text:macro-definition",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:container-util",
        "//common/util:logging",
        "//common/util:status-macros",
        "//verilog/analysis:verilog-filelist",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "verilog-preprocess_test",
    srcs = ["verilog_preprocess_test.cc"],
    deps = [
        ":verilog-preprocess",
        "//common/text:macro-definition",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-stream-view",
        "//common/util:container-util",
        "//common/util:file-util",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis:verilog-filelist",
        "//verilog/analysis:verilog-project",
        "//verilog/parser:verilog-lexer",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
