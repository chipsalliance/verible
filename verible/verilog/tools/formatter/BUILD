# 'verilog_format' is a SystemVerilog source code formatter.
#

load("//bazel:sh_test_with_runfiles_lib.bzl", "sh_test_with_runfiles_lib")
load("//bazel:variables.bzl", "STATIC_EXECUTABLES_FEATURE")

package(
    default_applicable_licenses = ["//:license"],
    default_visibility = ["//visibility:private"],
    features = ["layering_check"],
)

cc_binary(
    name = "verible-verilog-format",
    srcs = ["verilog-format.cc"],
    features = STATIC_EXECUTABLES_FEATURE,
    visibility = ["//visibility:public"],  # for verilog_style_lint.bzl
    deps = [
        "//verible/common/strings:position",
        "//verible/common/util:file-util",
        "//verible/common/util:init-command-line",
        "//verible/common/util:interval-set",
        "//verible/common/util:iterator-range",
        "//verible/verilog/formatting:format-style",
        "//verible/verilog/formatting:format-style-init",
        "//verible/verilog/formatting:formatter",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:usage",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
    ],
)

# This script is intended to run post-install and expect to be co-located with:
#   //verible/verilog/tools/formatter:verible-verilog-format
#   //verible/common/tools:verible-patch-tool
filegroup(
    name = "git-verible-verilog-format",
    srcs = ["git-verible-verilog-format.sh"],
    visibility = ["//:__pkg__"],  # for release
)

# This script is intended to run post-install and expect to be co-located with:
#   //verible/verilog/tools/formatter:verible-verilog-format
#   //verible/common/tools:verible-patch-tool
#   //verible/common/tools:verible-transform-interactive
filegroup(
    name = "verible-verilog-format-changed-lines-interactive",
    srcs = ["verible-verilog-format-changed-lines-interactive.sh"],
    visibility = ["//:__pkg__"],  # for release
)

sh_binary(
    name = "diff-formatter",
    srcs = ["diff_formatter.sh"],
)

sh_binary(
    name = "triage-formatter",
    srcs = ["triage_formatter.sh"],
)

sh_test_with_runfiles_lib(
    name = "format-file_test",
    size = "small",
    srcs = ["format_file_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-file-check_test",
    size = "small",
    srcs = ["format_file_check_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-file-lex-error_test",
    size = "small",
    srcs = ["format_file_lex_error_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-file-syntax-error_test",
    size = "small",
    srcs = ["format_file_syntax_error_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-file-lines_test",
    size = "small",
    srcs = ["format_file_lines_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-file-badlines_test",
    size = "small",
    srcs = ["format_file_badlines_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-inplace_test",
    size = "small",
    srcs = ["format_inplace_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-stdin_test",
    size = "small",
    srcs = ["format_stdin_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)

sh_test_with_runfiles_lib(
    name = "format-stdin-inplace_test",
    size = "small",
    srcs = ["format_stdin_inplace_test.sh"],
    args = ["$(location :verible-verilog-format)"],
    data = [":verible-verilog-format"],
)
