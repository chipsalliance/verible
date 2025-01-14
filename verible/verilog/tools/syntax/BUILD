# 'verilog_syntax' is a program for checking Verilog/SystemVerilog syntax.

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-syntax",
    srcs = ["verilog-syntax.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//verible/common/strings:mem-block",
        "//verible/common/text:concrete-syntax-tree",
        "//verible/common/text:parser-verifier",
        "//verible/common/text:text-structure",
        "//verible/common/text:token-info",
        "//verible/common/text:token-info-json",
        "//verible/common/util:enum-flags",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:iterator-range",
        "//verible/common/util:logging",
        "//verible/verilog/CST:verilog-tree-json",
        "//verible/verilog/CST:verilog-tree-print",
        "//verible/verilog/analysis:json-diagnostics",
        "//verible/verilog/analysis:verilog-analyzer",
        "//verible/verilog/analysis/checkers:verilog-lint-rules",
        "//verible/verilog/parser:verilog-parser",
        "//verible/verilog/parser:verilog-token",
        "//verible/verilog/parser:verilog-token-classifications",
        "//verible/verilog/parser:verilog-token-enum",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:singleheader-json",
    ],
)

sh_binary(
    name = "triage-parser",
    srcs = ["triage_parser.sh"],
)

sh_test_with_runfiles_lib(
    name = "verilog-syntax_test",
    size = "small",
    srcs = ["verilog_syntax_test.sh"],
    args = ["$(location :verible-verilog-syntax)"],
    data = [":verible-verilog-syntax"],
    deps = [],
)
