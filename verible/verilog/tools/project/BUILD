"""This package contains multi-tool for working with SystemVerilog projects.
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-project",
    srcs = ["project-tool.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//:__subpackages__"],
    deps = [
        "//verible/common/util:init-command-line",
        "//verible/common/util:logging",
        "//verible/common/util:status-macros",
        "//verible/common/util:subcommand",
        "//verible/verilog/analysis:dependencies",
        "//verible/verilog/analysis:symbol-table",
        "//verible/verilog/analysis:verilog-filelist",
        "//verible/verilog/analysis:verilog-project",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
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
