"""This package contains language-agnostic tools in the Verible project."""

licenses(["notice"])

cc_binary(
    name = "verible-patch-tool",
    srcs = ["patch_tool.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/strings:patch",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:subcommand",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test(
    name = "patch_tool_test",
    size = "small",
    srcs = ["patch_tool_test.sh"],
    args = ["$(location :verible-patch-tool)"],
    data = [":verible-patch-tool"],
    deps = [],
)

# This script is intended to run post-install and expect to be co-located with:
#   //common/tools:verible-patch-tool
filegroup(
    name = "verible-transform-interactive",
    srcs = ["verible-transform-interactive.sh"],
    visibility = ["//:__pkg__"],  # for release
)

sh_test(
    name = "verible-transform-interactive-test",
    size = "small",
    srcs = ["verible-transform-interactive-test.sh"],
    args = [
        "$(location :verible-transform-interactive)",
        "$(location :verible-patch-tool)",
    ],
    data = [
        ":verible-patch-tool",
        ":verible-transform-interactive",
    ],
    deps = [],
)
