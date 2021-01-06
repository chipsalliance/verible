"""This package contains multi-tool for working with SystemVerilog projects.
"""

licenses(["notice"])

cc_binary(
    name = "verible-verilog-project",
    srcs = ["project_tool.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//common/util:subcommand",
        "//verilog/analysis:dependencies",
        "//verilog/analysis:symbol_table",
        "//verilog/analysis:verilog_project",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test(
    name = "project_tool_test",
    size = "small",
    srcs = ["project_tool_test.sh"],
    args = ["$(location :verible-verilog-project)"],
    data = [":verible-verilog-project"],
    deps = [],
)
