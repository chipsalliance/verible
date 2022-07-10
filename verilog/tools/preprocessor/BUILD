"""Tool for preprocessing Verilog
"""

licenses(["notice"])

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")

cc_binary(
    name = "verible-verilog-preprocessor",
    srcs = ["verilog_preprocessor.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:subcommand",
        "//verilog/transform:strip_comments",
        "//verilog/preprocessor:verilog_preprocess",
        "//verilog/parser:verilog_lexer",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "verilog_preprocessor_test",
    size = "small",
    srcs = ["verilog_preprocessor_test.sh"],
    args = ["$(location :verible-verilog-preprocessor)"],
    data = [":verible-verilog-preprocessor"],
    deps = [],
)
