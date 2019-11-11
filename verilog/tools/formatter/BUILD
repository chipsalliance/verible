# 'verilog_format' is a SystemVerilog source code formatter.

licenses(["notice"])

cc_binary(
    name = "verilog_format",
    srcs = ["verilog_format.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/text:text_structure",
        "//common/util:file_util",
        "//common/util:logging",
        "//common/util:status",
        "//verilog/analysis:verilog_analyzer",
        "//verilog/formatting:format_style",
        "//verilog/formatting:formatter",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/strings",
    ],
)
