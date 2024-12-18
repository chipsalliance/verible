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
    srcs = ["patch-tool.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    deps = [
        "//verible/common/strings:patch",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:status-macros",
        "//verible/common/util:subcommand",
        "//verible/common/util:user-interaction",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:string_view",
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
#   //verible/common/tools:verible-patch-tool
filegroup(
    name = "verible-transform-interactive",
    srcs = ["verible-transform-interactive.sh"],
    visibility = ["//:__pkg__"],  # for release
)

sh_test_with_runfiles_lib(
    name = "verible-transform-interactive_test",
    size = "small",
    srcs = ["verible-transform-interactive_test.sh"],
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
