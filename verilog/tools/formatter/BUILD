# 'verilog_format' is a SystemVerilog source code formatter.
#

licenses(["notice"])

cc_binary(
    name = "verible-verilog-format",
    srcs = ["verilog_format.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/formatting:align",
        "//common/strings:position",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:interval_set",
        "//common/util:logging",
        "//verilog/formatting:format_style",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

# This script is intended to run post-install and expect to be co-located with:
#   //verilog/tools/formatter:verible-verilog-format
#   //common/tools:verible-patch-tool
filegroup(
    name = "git-verilog-format",
    srcs = ["git-verible-verilog-format.sh"],
    visibility = ["//:__pkg__"],  # for release
)

# This script is intended to run post-install and expect to be co-located with:
#   //verilog/tools/formatter:verible-verilog-format
#   //common/tools:verible-patch-tool
#   //common/tools:verible-transform-interactive
filegroup(
    name = "verible-verilog-format-changed-lines-interactive",
    srcs = ["verible-verilog-format-changed-lines-interactive.sh"],
    visibility = ["//:__pkg__"],  # for release
)

sh_binary(
    name = "diff_formatter",
    srcs = ["diff_formatter.sh"],
)

sh_binary(
    name = "triage_formatter",
    srcs = ["triage_formatter.sh"],
)

sh_test(
    name = "format_file_test",
    size = "small",
    srcs = ["format_file_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_file_lex_error_test",
    size = "small",
    srcs = ["format_file_lex_error_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_file_syntax_error_test",
    size = "small",
    srcs = ["format_file_syntax_error_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_file_lines_test",
    size = "small",
    srcs = ["format_file_lines_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_file_badlines_test",
    size = "small",
    srcs = ["format_file_badlines_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_inplace_test",
    size = "small",
    srcs = ["format_inplace_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_stdin_test",
    size = "small",
    srcs = ["format_stdin_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test(
    name = "format_stdin_inplace_test",
    size = "small",
    srcs = ["format_stdin_inplace_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)
