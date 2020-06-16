# 'verilog_format' is a SystemVerilog source code formatter.
#

licenses(["notice"])

cc_binary(
    name = "verible-verilog-format",
    srcs = ["verilog_format.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
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

# For a limited time, we provide the old name to be compatible with the
# old installation. At some point, this script will print a deprecation warning
# and be removed entirely later.
filegroup(
    name = "verilog_format-deprecated",
    srcs = [
        "verilog_format",
        ":verible-verilog-format",
    ],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "git-verilog-format",
    srcs = [
        "diff-to-changed-lines.awk",
        "git-verible-verilog-format.sh",
    ],
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
