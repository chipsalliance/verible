# 'verilog_style_tester' is a program for checking internal style compliance state

licenses(["notice"])

cc_binary(
    name = "verible-verilog-style-tester",
    srcs = ["verilog_style_tester.cc"],
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//common/text:concrete_syntax_tree",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//verilog/formatting:style_compliance_report",
        "//verilog/formatting:formatter",
        "//verilog/formatting:lowrisc_format_style",
        "//verilog/formatting:formatter_lowrisc_style_test_cases",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
