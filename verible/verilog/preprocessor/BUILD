# This package contains libraries related to SystemVerilog preprocessing.

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//verible/verilog:__subpackages__",
        # TODO(b/130113490): standalone preprocessor tool
    ],
    features = ["layering_check"],
)

cc_library(
    name = "verilog-preprocess",
    srcs = ["verilog-preprocess.cc"],
    hdrs = ["verilog-preprocess.h"],
    deps = [
        "//verible/common/lexer:token-generator",
        "//verible/common/lexer:token-stream-adapter",
        "//verible/common/text:macro-definition",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:container-util",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_test(
    name = "verilog-preprocess_test",
    srcs = ["verilog-preprocess_test.cc"],
    deps = [
        ":verilog-preprocess",
        "//verible/common/text:macro-definition",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-stream-view",
        "//verible/common/util:container-util",
        "//verible/common/util:file-util",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "//verible/verilog/parser:verilog-lexer",
        "//verible/verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
