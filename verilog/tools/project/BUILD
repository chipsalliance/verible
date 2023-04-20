"""This package contains multi-tool for working with SystemVerilog projects.
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

cc_binary(
    name = "verible-verilog-project",
    srcs = ["project_tool.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/util:init-command-line",
        "//common/util:logging",
        "//common/util:status-macros",
        "//common/util:subcommand",
        "//verilog/analysis:dependencies",
        "//verilog/analysis:symbol-table",
        "//verilog/analysis:verilog-project",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "project-tool_test",
    size = "small",
    srcs = ["project_tool_test.sh"],
    args = ["$(location :verible-verilog-project)"],
    data = [":verible-verilog-project"],
    deps = [],
)
