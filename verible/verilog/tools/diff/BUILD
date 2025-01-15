"""Tool for comparing Verilog source code.
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-diff",
    srcs = ["verilog-diff.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//verible/common/util:enum-flags",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:logging",
        "//verible/verilog/analysis:verilog-equivalence",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/strings",
    ],
)

sh_test_with_runfiles_lib(
    name = "diff-user-errors_test",
    size = "small",
    srcs = ["diff_user_errors_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test_with_runfiles_lib(
    name = "diff-format-match_test",
    size = "small",
    srcs = ["diff_format_match_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test_with_runfiles_lib(
    name = "diff-format-mismatch_test",
    size = "small",
    srcs = ["diff_format_mismatch_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test_with_runfiles_lib(
    name = "diff-format-lex-error_test",
    size = "small",
    srcs = ["diff_format_lex_error_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test_with_runfiles_lib(
    name = "diff-obfuscate-match_test",
    size = "small",
    srcs = ["diff_obfuscate_match_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test_with_runfiles_lib(
    name = "diff-obfuscate-mismatch_test",
    size = "small",
    srcs = ["diff_obfuscate_mismatch_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)
