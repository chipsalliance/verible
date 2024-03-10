"""This package contains language-agnostic tools in the Verible project."""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = [
        "//:__subpackages__",
    ],
    features = ["layering_check"],
)

exports_files([
    "jcxxgen.bzl",
])

cc_binary(
    name = "verible-patch-tool",
    srcs = ["patch_tool.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    deps = [
        "//common/strings:patch",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:status-macros",
        "//common/util:subcommand",
        "//common/util:user-interaction",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "patch-tool_test",
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

sh_test_with_runfiles_lib(
    name = "verible-transform-interactive_test",
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
