"""This package contains language-agnostic tools in the Verible project."""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//common/tools:jcxxgen.bzl", "jcxxgen")

exports_files([
    "jcxxgen.bzl",
])

cc_binary(
    name = "verible-patch-tool",
    srcs = ["patch_tool.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/strings:patch",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:status_macros",
        "//common/util:subcommand",
        "//common/util:user_interaction",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
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

sh_test_with_runfiles_lib(
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

cc_binary(
    name = "jcxxgen",
    srcs = ["jcxxgen.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//common/util:init_command_line",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)

jcxxgen(
    name = "jcxxgen_testfile",
    src = "jcxxgen_testfile.yaml",
    out = "jcxxgen_testfile.h",
    namespace = "verible::test",
)

cc_test(
    name = "jcxxgen_test",
    srcs = ["jcxxgen_test.cc"],
    copts = select({
        "@platforms//os:windows": [],
        "//conditions:default": ["-fexceptions"],
    }),
    features = ["-use_header_modules"],  # precompiled headers incompatible with -fexceptions.
    deps = [
        ":jcxxgen_testfile",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
