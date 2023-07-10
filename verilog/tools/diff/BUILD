"""Tool for comparing Verilog source code.
"""

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
)

cc_binary(
    name = "verible-verilog-diff",
    srcs = ["verilog_diff.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:enum-flags",
        "//common/util:file-util",
        "//common/util:init-command-line",
        "//common/util:logging",
        "//verilog/analysis:verilog-equivalence",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
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
