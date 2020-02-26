# 'verilog_format' is a SystemVerilog source code formatter.
#

licenses(["notice"])

cc_binary(
    name = "verilog_format",
    srcs = ["verilog_format.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/text:text_structure",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:interval_set",
        "//common/util:logging",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/formatting:format_style",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_binary(
    name = "diff_formatter",
    srcs = ["diff_formatter.sh"],
)

sh_binary(
    name = "triage_formatter",
    srcs = ["triage_formatter.sh"],
)
