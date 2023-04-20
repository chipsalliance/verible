"""Tool for preprocessing Verilog
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

cc_binary(
    name = "verible-verilog-preprocessor",
    srcs = ["verilog_preprocessor.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:status-macros",
        "//common/util:subcommand",
        "//verilog/analysis:flow-tree",
        "//verilog/analysis:verilog-filelist",
        "//verilog/analysis:verilog-project",
        "//verilog/parser:verilog-lexer",
        "//verilog/preprocessor:verilog-preprocess",
        "//verilog/transform:strip-comments",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog-preprocessor_test",
    size = "small",
    srcs = ["verilog_preprocessor_test.sh"],
    args = ["$(location :verible-verilog-preprocessor)"],
    data = [":verible-verilog-preprocessor"],
    deps = [],
)
