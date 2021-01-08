"""Tool for comparing Verilog source code.
"""

licenses(["notice"])

cc_binary(
    name = "verible-verilog-diff",
    srcs = ["verilog_diff.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/strings:obfuscator",
        "//common/util:enum_flags",
        "//common/util:file_util",
        "//common/util:init_command_line",
        "//common/util:logging",
        "//verilog/analysis:verilog_equivalence",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
    ],
)

# For a limited time, we provide the old name to be compatible with the
# old installation. At some point, this script will print a deprecation warning
# and be removed entirely later.
filegroup(
    name = "verilog_diff-deprecated",
    srcs = [
        "verilog_diff",
        ":verible-verilog-diff",
    ],
    visibility = ["//visibility:public"],
)

sh_test(
    name = "diff_user_errors_test",
    size = "small",
    srcs = ["diff_user_errors_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test(
    name = "diff_format_match_test",
    size = "small",
    srcs = ["diff_format_match_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test(
    name = "diff_format_mismatch_test",
    size = "small",
    srcs = ["diff_format_mismatch_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test(
    name = "diff_format_lex_error_test",
    size = "small",
    srcs = ["diff_format_lex_error_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test(
    name = "diff_obfuscate_match_test",
    size = "small",
    srcs = ["diff_obfuscate_match_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)

sh_test(
    name = "diff_obfuscate_mismatch_test",
    size = "small",
    srcs = ["diff_obfuscate_mismatch_test.sh"],
    args = ["$(location :verible-verilog-diff)"],
    data = [":verible-verilog-diff"],
)
