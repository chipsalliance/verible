"""This package contains language-agnostic tools in the Verible project."""

licenses(["notice"])

cc_binary(
    name = "verible-patch-tool",
    srcs = ["patch_tool.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/strings:compare",
        "//common/strings:patch",
        "//common/util:container_iterator_range",
        "//common/util:file_util",
        "//common/util:init_command_line",
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
