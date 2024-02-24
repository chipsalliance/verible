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
    srcs = ["verilog_syntax.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/strings:mem-block",
        "//common/text:concrete-syntax-tree",
        "//common/text:parser-verifier",
        "//common/text:text-structure",
        "//common/text:token-info",
        "//common/text:token-info-json",
        "//common/util:enum-flags",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:iterator-range",
        "//common/util:logging",
        "//verilog/CST:verilog-tree-json",
        "//verilog/CST:verilog-tree-print",
        "//verilog/analysis:json-diagnostics",
        "//verilog/analysis:verilog-analyzer",
        "//verilog/analysis/checkers:verilog-lint-rules",
        "//verilog/parser:verilog-parser",
        "//verilog/parser:verilog-token",
        "//verilog/parser:verilog-token-classifications",
        "//verilog/parser:verilog-token-enum",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@jsonhpp",
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
